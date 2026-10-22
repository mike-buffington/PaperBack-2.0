// Portable macOS/Linux wrapper around PaperBak 1.10's original raster decoder.
// The image-analysis and dot-recognition algorithms below are retained from
// Decoder.cpp. Only Windows UI/allocation dependencies are replaced.
#include "paperbak_decoder.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using uchar = unsigned char;
using ushort = unsigned short;
using uint = unsigned int;
#define max(a,b) std::max((a),(b))
#define min(a,b) std::min((a),(b))
#define NDOT 32
#define NDATA 90
#define SUPERBLOCK 0xFFFFFFFFu
#define M_BEST 0x00000001
#define NHYST 1024
#define NPEAK 32
#define SUBDX 8
#define SUBDY 8
#define GMEM_FIXED 0
using HGLOBAL = void*;
static void* GlobalAlloc(int, std::size_t n) { return std::malloc(n); }
static void GlobalFree(HGLOBAL p) { std::free(p); }
static void Initqualitymap(int,int) {}
static thread_local std::string decoder_error;
static void Reporterror(const char* s) { decoder_error = s ? s : "Decoder error"; }

extern int Decode8(uchar *data,int *eras_pos,int no_eras,int pad);
extern ushort Crc16(uchar *data,int length);

#pragma pack(push,1)
struct t_data { std::uint32_t addr; uchar data[NDATA]; ushort crc; uchar ecc[32]; };
#pragma pack(pop)
static_assert(sizeof(t_data)==128, "PaperBak block layout changed");

struct t_block { std::uint32_t addr; std::uint32_t recsize; uchar data[NDATA]; };
struct t_procdata {
  int step=0, mode=0; uchar *data=nullptr; int sizex=0,sizey=0;
  int gridxmin=0,gridxmax=0,gridymin=0,gridymax=0;
  int searchx0=0,searchx1=0,searchy0=0,searchy1=0;
  int cmean=0,cmin=0,cmax=0; float sharpfactor=0;
  float xpeak=0,xstep=0,xangle=0,ypeak=0,ystep=0,yangle=0,blockborder=0;
  int bufdx=0,bufdy=0; uchar *buf1=nullptr,*buf2=nullptr; int *bufx=nullptr,*bufy=nullptr;
  uchar *unsharp=nullptr,*sharp=nullptr; float blockxpeak=0,blockypeak=0,blockxstep=0,blockystep=0;
  int nposx=0,nposy=0,posx=0,posy=0; t_data uncorrected{}; t_block *blocklist=nullptr;
  int maxdotsize=0,orientation=-1,ngood=0,nbad=0,nsuper=0,nrestored=0;
};
static float Findpeaks(int *h,int n,float *bestpeak,float *beststep) {
  int i,j,k,ampl,amin,amax,d,l[NHYST],limit,sum;
  int npeak,dist,bestdist,bestcount,height[NPEAK];
  float area,moment,peak[NPEAK],weight[NPEAK];
  float x0,step,sn,sx,sy,sxx,syy,sxy;
  // I expect at least 16 and at most NHYST points in the histogramm.
  if (n<16) return 0.0;
  if (n>NHYST) n=NHYST;
  // Get absolute minimum and maximum.
  amin=amax=h[0];
  for (i=1; i<n; i++) {
    if (h[i]<amin) amin=h[i];
    if (h[i]>amax) amax=h[i]; };
  // Remove gradients by shadowing over 32 pixels. May create small artefacts
  // in the vicinity of the main peak.
  d=(amax-amin+16)/32;
  ampl=h[0];
  for (i=0; i<n; i++) {
    l[i]=ampl=max(ampl-d,h[i]); };
  amax=0;
  for (i=n-1; i>=0; i--) {
    ampl=max(ampl-d,l[i]);
    l[i]=ampl-h[i];
    amax=max(amax,l[i]); };

// TRY TO COMPARE WITH SECOND LARGE PEAK?

  // I set peak limit to 3/4 of the amplitude of the highest peak. This
  // solution at least works in 90% of all cases.
  limit=amax*3/4;
  if (limit==0) limit=1;
  // Start search and skip incomplete first peak.
  i=0; npeak=0;
  while (i<n && l[i]>limit) i++;
  // Find peaks.
  while (i<n && npeak<NPEAK) {
    // Find next peak.
    while (i<n && l[i]<=limit) i++;
    // Calculate peak parameters.
    area=0.0; moment=0.0; amax=0;
    while (i<n && l[i]>limit) {
      ampl=l[i]-limit;
      area+=ampl;
      moment+=ampl*i;
      amax=max(amax,l[i]);
      i++; };
    // Don't process incomplete peaks.
    if (i>=n) break;
    // Add peak to the list, removing weak artefacts.
    if (npeak>0) {
      if (amax*8<height[npeak-1]) continue;
      if (amax>height[npeak-1]*8) npeak--; };
    peak[npeak]=moment/area;
    weight[npeak]=area;
    height[npeak]=amax;
    npeak++;
  };
  // At least two peaks are necessary to detect the step.
  if (npeak<2) return 0.0;
  // Calculate all possible distances between the found peaks.
  for (i=0; i<n; i++) l[i]=0;
  for (i=0; i<npeak-1; i++) {
    for (j=i+1; j<npeak; j++) {
      l[(int)(peak[j]-peak[i])]++;
    };
  };
  // Find group with the maximal number of peaks. I allow for approximately 3%
  // dispersion. Distances under 16 pixels are too short to be real. Caveat:
  // this method can't distinguish direct sequence from interleaved.
  bestdist=0; bestcount=0;
  for (i=16; i<n; i++) {
    if (l[i]==0) continue;
    sum=0;
    for (j=i; j<=i+i/33+1 && j<n; j++) sum+=l[j];
    if (sum>bestcount) {               // Shorter is better
      bestdist=i;
      bestcount=sum;
    };
  };
  if (bestdist==0) return 0.0;
  // Now determine the parameters of the sequence. The method I use is not very
  // good but usually sufficient.
  sn=sx=sy=sxx=syy=sxy=0.0;
  moment=0.0;
  for (i=0; i<npeak-1; i++) {
    for (j=i+1; j<npeak; j++) {
      dist=peak[j]-peak[i];
      if (dist<bestdist || dist>=bestdist+bestdist/33+1) continue;
      if (sn==0.0)                     // First link
        k=0;
      else {
        x0=(sx*sxy-sxx*sy)/(sx*sx-sn*sxx);
        step=(sx*sy-sn*sxy)/(sx*sx-sn*sxx);
        k=(peak[i]-x0+step/2.0)/step; };
      sn+=2.0;
      sx+=k*2+1;
      sy+=peak[i]+peak[j];
      sxx+=k*k+(k+1)*(k+1);
      syy+=peak[i]*peak[i]+peak[j]*peak[j];
      sxy+=peak[i]*k+peak[j]*(k+1);
      moment+=height[i]+height[j];
    };
  };
  *bestpeak=(sx*sxy-sxx*sy)/(sx*sx-sn*sxx);
  *beststep=(sx*sy-sn*sxy)/(sx*sx-sn*sxx);
  return moment/sn;
};

// Given grid of recognized dots, extracts saved information. Returns number of
// corrected erorrs (0..16) on success and 17 if information is not readable.
static int Recognizebits(t_data *result,uchar grid[NDOT][NDOT],
  t_procdata *pdata) {
  int i,j,k,q,r,factor,lcorr,c,cmin,cmax,limit;
  int grid1[NDOT][NDOT],answer,bestanswer;
  static int lastgood;
  ushort crc;
  t_data uncorrected,bestresult;
  cmin=pdata->cmin;
  cmax=pdata->cmax;
  bestanswer=17;
  // If orientation is not yet known, try all possible orientations + mirroring.
  for (r=0; r<8; r++) {
    if (pdata->orientation>=0 && r!=pdata->orientation) continue;
    // Try 3 different point overlapping factors, combined with 3 different
    // thresholds. Usually all cells are alike, so I remember the last known
    // good combination and start with it.
    for (k=0; k<9; k++) {
      q=(k+lastgood)%9;
      switch (q) {
        case 0: factor=1000; lcorr=0; break;
        case 1: factor=32; lcorr=0; break;
        case 2: factor=16; lcorr=0; break;
        case 3: factor=1000; lcorr=(cmin-cmax)/16; break;
        case 4: factor=32; lcorr=(cmin-cmax)/16; break;
        case 5: factor=16; lcorr=(cmin-cmax)/16; break;
        case 6: factor=1000; lcorr=(cmax-cmin)/16; break;
        case 7: factor=32; lcorr=(cmax-cmin)/16; break;
        case 8: factor=16; lcorr=(cmax-cmin)/16; break;
        default: factor=1000; lcorr=0; lastgood=0; break; };
      // Correct grid for overlapping dots and calculate limit between black
      // and white. I take into account only adjacent dots; the influence of
      // diagonals is significantly lower.
      limit=0;
      for (j=0; j<NDOT; j++) {
        for (i=0; i<NDOT; i++) {
          c=grid[i][j]*factor;
          if (i>0) c-=grid[j][i-1]; else c-=cmax;
          if (i<31) c-=grid[j][i+1]; else c-=cmax;
          if (j>0) c-=grid[j-1][i]; else c-=cmax;
          if (j<31) c-=grid[j+1][i]; else c-=cmax;
          grid1[j][i]=c;
          limit+=c;
        };
      };
      limit=limit/1024+lcorr*factor;
      // Extract data according to the selected orientation.
      memset(result,0,sizeof(t_data));
      for (j=0; j<NDOT; j++) {
        for (i=0; i<NDOT; i++) {
          switch (r) {
            case 0: c=grid1[j][i]; break;
            case 1: c=grid1[i][NDOT-1-j]; break;
            case 2: c=grid1[NDOT-1-j][NDOT-1-i]; break;
            case 3: c=grid1[NDOT-1-i][j]; break;
            case 4: c=grid1[i][j]; break;
            case 5: c=grid1[j][NDOT-1-i]; break;
            case 6: c=grid1[NDOT-1-i][NDOT-1-j]; break;
            case 7: c=grid1[NDOT-1-j][i]; break;
          };
          if (c<limit) {
            ((uint32_t *)result)[j]|=1<<i;
          };
        };
      };
      // XOR with grid that corrects mean brightness.
      for (j=0; j<NDOT; j++) {
        ((uint32_t *)result)[j]^=(j & 1?0xAAAAAAAA:0x55555555); };
      // Apply ECC to restore invalid data.
      if (pdata->mode & M_BEST)
        memcpy(&uncorrected,result,sizeof(t_data));
      else
        memcpy(&pdata->uncorrected,result,sizeof(t_data));
      answer=Decode8((uchar *)result,NULL,0,127);
      if (answer<0) answer=17;
      // Verify data for correctness by calculating CRC.
      if (answer<=16) {
        crc=(ushort)(Crc16((uchar *)result,NDATA+4)^0x55AA);
        if (crc==result->crc) {
          // Data recognized correctly, save orientation of actually processed
          // page and factoring.
          pdata->orientation=r;
          // Report success.
          if ((pdata->mode & M_BEST)==0) {
            lastgood=q;
            return answer; }
          else if (answer<bestanswer) {
            bestanswer=answer;
            bestresult=*result;
            memcpy(&pdata->uncorrected,&uncorrected,sizeof(t_data));
          };
        };
      };
    };
  };
  if (pdata->mode & M_BEST)
    *result=bestresult;
  return bestanswer;
};

// Determines rough grid position.
static void Getgridposition(t_procdata *pdata) {
  int i,j,nx,ny,stepx,stepy,sizex,sizey;
  int c,cmin,cmax,distrx[256],distry[256],limit;
  uchar *data,*pd;
  // Get frequently used variables.
  sizex=pdata->sizex;
  sizey=pdata->sizey;
  data=pdata->data;
  // Check overall bitmap size.
  if (sizex<=3*NDOT || sizey<=3*NDOT) {
    Reporterror("Bitmap is too small to process");
    pdata->step=0; return; };
  // Select horizontal and vertical lines (at most 256 in each direction) to
  // check for grid location.
  stepx=sizex/256+1; nx=(sizex-2)/stepx; if (nx>256) nx=256;
  stepy=sizey/256+1; ny=(sizey-2)/stepy; if (ny>256) ny=256;
  // The main problem in determining the grid location are the black and/or
  // white borders around the grid. To distinguish between borders with more or
  // less constant intensity and quickly changing raster, I take into account
  // only the fast intensity changes over the short distance (2 pixels).
  // Caveat: this approach may fail for artificially created bitmaps.
  memset(distrx,0,nx*sizeof(int));
  memset(distry,0,ny*sizeof(int));
  for (j=0; j<ny; j++) {
    pd=data+j*stepy*sizex;
    for (i=0; i<nx; i++,pd+=stepx) {
      c=pd[0];         cmin=c;           cmax=c;
      c=pd[2];         cmin=min(cmin,c); cmax=max(cmax,c);
      c=pd[sizex+1];   cmin=min(cmin,c); cmax=max(cmax,c);
      c=pd[2*sizex];   cmin=min(cmin,c); cmax=max(cmax,c);
      c=pd[2*sizex+2]; cmin=min(cmin,c); cmax=max(cmax,c);
      distrx[i]+=cmax-cmin;
      distry[j]+=cmax-cmin;
    };
  };
  // Get rough bitmap limits in horizontal direction (at the level 50% of
  // maximum).
  limit=0;
  for (i=0; i<nx; i++) {
    if (distrx[i]>limit) limit=distrx[i]; };
  limit/=2;
  for (i=0; i<nx-1; i++) {
    if (distrx[i]>=limit) break; };
  pdata->gridxmin=i*stepx;
  for (i=nx-1; i>0; i--) {
    if (distrx[i]>=limit) break; };
  pdata->gridxmax=i*stepx;
  // Get rough bitmap limits in vertical direction.
  limit=0;
  for (j=0; j<ny; j++) {
    if (distry[j]>limit) limit=distry[j]; };
  limit/=2;
  for (j=0; j<ny-1; j++) {
    if (distry[j]>=limit) break; };
  pdata->gridymin=j*stepy;
  for (j=ny-1; j>0; j--) {
    if (distry[j]>=limit) break; };
  pdata->gridymax=j*stepy;
  // Step finished.
  pdata->step++;
};

// Selects search range, determines grid intensity and estimates sharpness.
static void Getgridintensity(t_procdata *pdata) {
  int i,j,sizex,sizey,centerx,centery,dx,dy,n;
  int searchx0,searchy0,searchx1,searchy1;
  int distrc[256],distrd[256],cmean,cmin,cmax,limit,sum,contrast;
  uchar *data,*pd;
  // Get frequently used variables.
  sizex=pdata->sizex;
  sizey=pdata->sizey;
  data=pdata->data;
  // Select X and Y ranges to search for the grid. As I use affine transforms
  // instead of more CPU-intensive rotations, these ranges are determined for
  // Y=0 (searchx0,searchx1) and for X=0 (searchy0,searchy1).
  centerx=(pdata->gridxmin+pdata->gridxmax)/2;
  centery=(pdata->gridymin+pdata->gridymax)/2;
  searchx0=centerx-NHYST/2; if (searchx0<0) searchx0=0;
  searchx1=searchx0+NHYST; if (searchx1>sizex) searchx1=sizex;
  searchy0=centery-NHYST/2; if (searchy0<0) searchy0=0;
  searchy1=searchy0+NHYST; if (searchy1>sizey) searchy1=sizey;
  dx=searchx1-searchx0;
  dy=searchy1-searchy0;
  // Determine mean, minimal and maximal intensity of the central area, and
  // sharpness of the image. As a minimum I take the level not reached by 3%
  // of all pixels, as a maximum - level exceeded by 3% of pixels.
  memset(distrc,0,sizeof(distrc));
  memset(distrd,0,sizeof(distrd));
  cmean=0; n=0;
  for (j=0; j<dy-1; j++) {
    pd=data+(searchy0+j)*sizex+searchx0;
    for (i=0; i<dx-1; i++,pd++) {
      distrc[*pd]++; cmean+=*pd; n++;
      distrd[abs(pd[1]-pd[0])]++;
      distrd[abs(pd[sizex]-pd[0])]++;
    };
  };
  // Calculate mean, minimal and maximal image intensity.
  cmean/=n;
  limit=n/33;                          // 3% of the total number of pixels
  for (cmin=0,sum=0; cmin<255; cmin++) {
    sum+=distrc[cmin];
    if (sum>=limit) break; };
  for (cmax=255,sum=0; cmax>0; cmax--) {
    sum+=distrc[cmax];
    if (sum>=limit) break; };
  if (cmax-cmin<1) {
    Reporterror("No image");
    pdata->step=0;
    return; };
  // Estimate image sharpness. The factor is rather empirical. Later, when
  // dot size is known, this value will be corrected.
  limit=n/10;                          // 5% (each point is counted twice)
  for (contrast=255,sum=0; contrast>1; contrast--) {
    sum+=distrd[contrast];
    if (sum>=limit) break; };
  pdata->sharpfactor=(cmax-cmin)/(2.0*contrast)-1.0;
  // Save results.
  pdata->searchx0=searchx0;
  pdata->searchx1=searchx1;
  pdata->searchy0=searchy0;
  pdata->searchy1=searchy1;
  pdata->cmean=cmean;
  pdata->cmin=cmin;
  pdata->cmax=cmax;
  // Step finished.
  pdata->step++;
};

// Find angle and step of vertical grid lines.
static void Getxangle(t_procdata *pdata) {
  int i,j,a,x,y,x0,y0,dx,dy,sizex;
  int h[NHYST],nh[NHYST],ystep;
  uchar *data,*pd;
  float weight,xpeak,xstep;
  float maxweight,bestxpeak,bestxangle,bestxstep;
  // Get frequently used variables.
  sizex=pdata->sizex;
  data=pdata->data;
  x0=pdata->searchx0;
  y0=pdata->searchy0;
  dx=pdata->searchx1-x0;
  dy=pdata->searchy1-y0;
  // Calculate vertical step. 256 lines are sufficient. Warning: danger of
  // moire, especially on synthetic bitmaps!
  ystep=dy/256; if (ystep<1) ystep=1;
  maxweight=0.0;
  xstep=bestxstep=0.0;
  // Determine rough angle, step and base for the vertical grid lines. Due to
  // the oversimplified conversion, cases a=+-1 are almost identical to a=0.
  // Maximal allowed angle is approx. +/-5 degrees (1/10 radian).
  for (a=-(NHYST/20)*2; a<=(NHYST/20)*2; a+=2) {
    // Clear histogramm.
    memset(h,0,dx*sizeof(int));
    memset(nh,0,dx*sizeof(int));
    // Gather histogramm.
    for (j=0; j<dy; j+=ystep) {
      y=y0+j;
      x=x0+(y0+j)*a/NHYST;             // Affine transformation
      pd=data+y*sizex+x;
      for (i=0; i<dx; i++,x++,pd++) {
        if (x<0) continue;
        if (x>=sizex) break;
        h[i]+=*pd; nh[i]++;
      };
    };
    // Normalize histogramm.
    for (i=0; i<dx; i++) {
      if (nh[i]>0) h[i]/=nh[i]; };
    // Find peaks. On small synthetic bitmaps (height less than NHYST/2
    // pixels) weights for a=0 and +/-2 are the same and routine would select
    // -2 as a best angle. To solve this problem, I add small correction that
    // preferes zero angle.
    weight=Findpeaks(h,dx,&xpeak,&xstep)+1.0/(abs(a)+10.0);
    if (weight>maxweight) {
      bestxpeak=xpeak+x0;
      bestxangle=(float)a/NHYST;
      bestxstep=xstep;
      maxweight=weight;
    };
  };
  // Analyse and save results.
  if (maxweight==0.0 || bestxstep<NDOT) {
    Reporterror("No grid");
    pdata->step=0;
    return; };
  pdata->xpeak=bestxpeak;
  pdata->xstep=bestxstep;
  pdata->xangle=bestxangle;
  // Step finished.
  pdata->step++;
};

// Find angle and step of horizontal grid lines. Very similar to Getxangle().
static void Getyangle(t_procdata *pdata) {
  int i,j,a,x,y,x0,y0,dx,dy,sizex,sizey;
  int h[NHYST],nh[NHYST],xstep;
  uchar *data,*pd;
  float weight,ypeak,ystep;
  float maxweight,bestypeak,bestyangle,bestystep;
  // Get frequently used variables.
  sizex=pdata->sizex;
  sizey=pdata->sizey;
  data=pdata->data;
  x0=pdata->searchx0;
  y0=pdata->searchy0;
  dx=pdata->searchx1-x0;
  dy=pdata->searchy1-y0;
  // Calculate vertical step. 256 lines are sufficient. Warning: danger of
  // moire, especially on synthetic bitmaps!
  xstep=dx/256; if (xstep<1) xstep=1;
  maxweight=0.0;
  ystep=bestystep=0.0;
  // Determine rough angle, step and base for the vertical grid lines. I do not
  // take into account the changes of angle caused by the X transformation.
  for (a=-(NHYST/20)*2; a<=(NHYST/20)*2; a+=2) {
    // Clear histogramm.
    memset(h,0,dy*sizeof(int));
    memset(nh,0,dy*sizeof(int));
    for (i=0; i<dx; i+=xstep) {
      x=x0+i;
      y=y0+(x0+i)*a/NHYST;             // Affine transformation
      pd=data+y*sizex+x;
      for (j=0; j<dy; j++,y++,pd+=sizex) {
        if (y<0) continue;
        if (y>=sizey) break;
        h[j]+=*pd; nh[j]++;
      };
    };
    // Normalize histogramm.
    for (j=0; j<dy; j++) {
      if (nh[j]>0) h[j]/=nh[j]; };
    // Find peaks.
    weight=Findpeaks(h,dy,&ypeak,&ystep)+1.0/(abs(a)+10.0);
    if (weight>maxweight) {
      bestypeak=ypeak+y0;
      bestyangle=(float)a/NHYST;
      bestystep=ystep;
      maxweight=weight;
    };
  };
  // Analyse and save results.
  if (maxweight==0.0 || bestystep<NDOT ||
    bestystep<pdata->xstep*0.40 ||
    bestystep>pdata->xstep*2.50
  ) {
    Reporterror("No grid");
    pdata->step=0;
    return; };
  pdata->ypeak=bestypeak;
  pdata->ystep=bestystep;
  pdata->yangle=bestyangle;
  // Step finished.
  pdata->step++;
};

// Prepare data and allocate memory for data decoding.
static void Preparefordecoding(t_procdata *pdata) {
  int sizex,sizey,dx,dy;
  float xstep,ystep,border,sharpfactor,shift,maxxshift,maxyshift,dotsize;
  // Get frequently used variables.
  sizex=pdata->sizex;
  sizey=pdata->sizey;
  xstep=pdata->xstep;
  ystep=pdata->ystep;
  border=pdata->blockborder;
  sharpfactor=pdata->sharpfactor;
  // Empirical formula: the larger the angle, the more imprecise is the
  // expected position of the block.
  if (border<=0.0) {
    border=max(fabs(pdata->xangle),fabs(pdata->yangle))*5.0+0.4;
    pdata->blockborder=border; };
  // Correct sharpness for known dot size. This correction is empirical.
  dotsize=max(xstep,ystep)/(NDOT+3.0);
  sharpfactor+=1.3/dotsize-0.1;
  if (sharpfactor<0.0) sharpfactor=0.0;
  else if (sharpfactor>2.0) sharpfactor=2.0;
  pdata->sharpfactor=sharpfactor;
  // Calculate start coordinates and number of block that fit onto the page
  // in X direction.
  maxxshift=fabs(pdata->xangle*sizey);
  if (pdata->xangle<0.0)
    shift=0.0;
  else
    shift=maxxshift;
  while (pdata->xpeak-xstep>-shift-xstep*border)
    pdata->xpeak-=xstep;
  pdata->nposx=(int)((sizex+maxxshift)/xstep);
  // The same in Y direction.
  maxyshift=fabs(pdata->yangle*sizex);
  if (pdata->yangle<0.0)
    shift=0.0;
  else
    shift=maxyshift;
  while (pdata->ypeak-ystep>-shift-ystep*border)
    pdata->ypeak-=ystep;
  pdata->nposy=(int)((sizey+maxyshift)/ystep);
  // Start new quality map. Note that this call doesn't force map to be
  // displayed.
  Initqualitymap(pdata->nposx,pdata->nposy);
  // Allocate block buffers.
  dx=xstep*(2.0*border+1.0)+1.0;
  dy=ystep*(2.0*border+1.0)+1.0;
  pdata->buf1=(uchar *)GlobalAlloc(GMEM_FIXED,dx*dy);
  pdata->buf2=(uchar *)GlobalAlloc(GMEM_FIXED,dx*dy);
  pdata->bufx=(int *)GlobalAlloc(GMEM_FIXED,dx*sizeof(int));
  pdata->bufy=(int *)GlobalAlloc(GMEM_FIXED,dy*sizeof(int));
  pdata->blocklist=(t_block *)
    GlobalAlloc(GMEM_FIXED,pdata->nposx*pdata->nposy*sizeof(t_block));
  // Check that we have enough memory.
  if (pdata->buf1==NULL || pdata->buf2==NULL ||
    pdata->bufx==NULL || pdata->bufy==NULL || pdata->blocklist==NULL
  ) {
    if (pdata->buf1!=NULL) GlobalFree((HGLOBAL)pdata->buf1);
    if (pdata->buf2!=NULL) GlobalFree((HGLOBAL)pdata->buf2);
    if (pdata->bufx!=NULL) GlobalFree((HGLOBAL)pdata->bufx);
    if (pdata->bufy!=NULL) GlobalFree((HGLOBAL)pdata->bufy);
    if (pdata->blocklist!=NULL) GlobalFree((HGLOBAL)pdata->blocklist);
    Reporterror("Low memory");
    pdata->step=0;
    return; };
  // Determine maximal size of the dot on the bitmap.
  if (xstep<2*(NDOT+3) || ystep<2*(NDOT+3))
    pdata->maxdotsize=1;
  else if (xstep<3*(NDOT+3) || ystep<3*(NDOT+3))
    pdata->maxdotsize=2;
  else if (xstep<4*(NDOT+3) || ystep<4*(NDOT+3))
    pdata->maxdotsize=3;
  else
    pdata->maxdotsize=4;
  // Prepare superblock.
  // Superblock accumulation is handled by the portable wrapper.
  // Initialize remaining items.
  pdata->bufdx=dx;
  pdata->bufdy=dy;
  pdata->orientation=-1;               // As yet, unknown page orientation
  pdata->ngood=0;
  pdata->nbad=0;
  pdata->nsuper=0;
  pdata->nrestored=0;
  pdata->posx=pdata->posy=0;           // First block to scan
  // Step finished.
  pdata->step++;
};

// The most important routine, converts scanned blocks into data. Used both by
// data decoder and by block display. Returns -1 if block cannot be located,
// 0 to 16 if block is correctly decoded and 17 if block is unrecoverable.
int Decodeblock(t_procdata *pdata,int posx,int posy,t_data *result) {
  int i,j,x,y,x0,y0,dx,dy,sizex,sizey,*bufx,*bufy;
  int c,cmin,cmax,dotsize,shift,shiftmax,sum,answer,bestanswer;
  float xangle,yangle,xbmp,ybmp,xres,yres,sharpfactor;
  float xpeak,xstep,ypeak,ystep,halfdot;
  float sy,syy,disp,dispmin,dispmax;
  uchar *psrc,*pdest,*data,g[9][NDOT][NDOT],grid[NDOT][NDOT];
  t_data uncorrected,bestresult;
  // Get frequently used variables.
  sizex=pdata->sizex;
  sizey=pdata->sizey;
  xangle=pdata->xangle;
  yangle=pdata->yangle;
  data=pdata->data;
  cmin=pdata->cmin;
  cmax=pdata->cmax;
  sharpfactor=pdata->sharpfactor;
  bufx=pdata->bufx;
  bufy=pdata->bufy;
  // Get block coordinates in the bitmap. Note that bitmap in memory is placed
  // upside down.
  x0=pdata->xpeak+pdata->xstep*(posx-pdata->blockborder);
  y0=pdata->ypeak+pdata->ystep*(pdata->nposy-posy-1-pdata->blockborder);
  dx=pdata->bufdx;
  dy=pdata->bufdy;
  // Rotate selected block to 'unsharp' buffer using bilinear interpolation.
  // Fast discrete shifts are also thinkable but deliver significantly higher
  // error rate.
  if (sharpfactor>0.0)
    pdest=pdata->buf2;                 // Sharping necessary
  else
    pdest=pdata->buf1;
  pdata->unsharp=pdest;
  for (j=0; j<dy; j++) {
    xbmp=x0+(y0+j)*xangle;
    if (xbmp>=0.0) x=xbmp;             // Integer and fractional parts
    else x=xbmp-1.0;
    xres=xbmp-x;
    for (i=0; i<dx; i++,pdest++,x++) {
      ybmp=y0+j+(x0+i)*yangle;
      if (ybmp>0.0) y=ybmp;
      else y=ybmp-1.0;
      yres=ybmp-y;
      if (x<0 || x>=sizex-1 || y<0 || y>=sizey-1)
        *pdest=(uchar)cmax;            // Fill areas outside the page white
      else {
        psrc=data+y*sizex+x;
        *pdest=(psrc[0]+(psrc[1]-psrc[0])*xres)*(1.0-yres)+
        (psrc[sizex]+(psrc[sizex+1]-psrc[sizex])*xres)*yres;
      };
    };
  };
  // Sharpen rotated block, if necessary.
  if (sharpfactor>0.0) {
    psrc=pdata->buf2;
    pdest=pdata->buf1;
    for (j=0; j<dy; j++) {
      for (i=0; i<dx; i++,psrc++,pdest++) {
        if (i==0 || i==dx-1 || j==0 || j==dy-1)
          *pdest=*psrc;
        else {
          *pdest=(uchar)max(cmin,min((int)(psrc[0]*(1.0+4.0*sharpfactor)-
          (psrc[-dx]+psrc[-1]+psrc[1]+psrc[dx])*sharpfactor),cmax));
        };
      };
    };
  };
  pdata->sharp=pdata->buf1;
  // Find grid lines for the whole block. This works perfectly for laser
  // printers. For bidirectional jet printers, splitting left and right
  // borders into several pieces may give better results.
  memset(bufx,0,dx*sizeof(int));
  memset(bufy,0,dy*sizeof(int));
  psrc=pdata->buf1;
  for (j=0; j<dy; j++) {
    for (i=0; i<dx; i++,psrc++) {
      bufx[i]+=*psrc;
      bufy[j]+=*psrc;
    };
  };
  if (Findpeaks(bufx,dx,&xpeak,&xstep)<=0.0)
    return -1;                         // No X grid
  if (fabs(xstep-pdata->xstep)>pdata->xstep/16.0)
    return -1;                         // Invalid grid step
  if (Findpeaks(bufy,dy,&ypeak,&ystep)<=0.0)
    return -1;                         // No Y grid
  if (fabs(ystep-pdata->ystep)>pdata->ystep/16.0)
    return -1;                         // Invalid grid step
  // Save block position for displaying purposes.
  pdata->blockxpeak=xpeak;
  pdata->blockxstep=xstep;
  pdata->blockypeak=ypeak;
  pdata->blockystep=ystep;
  // Calculate dot step and correct peaks so that they point to first dot.
  xstep=xstep/(NDOT+3.0);
  xpeak+=2.0*xstep;
  ystep=ystep/(NDOT+3.0);
  ypeak+=2.0*ystep;
  // In search-for-the-best-quality mode, I look for the best possible
  // decoding. Helps to estimate the overall quality of the picture.
  bestanswer=17;
  // Try different dot sizes, starting from 1x1 pixel. If scanner resolution
  // is sufficient, 2x2 dot usually gives best results.
  for (dotsize=1; dotsize<=pdata->maxdotsize; dotsize++) {
    halfdot=dotsize/2.0-1.0;
    for (j=0; j<NDOT; j++) {
      y=ypeak+ystep*j-halfdot;
      for (i=0; i<NDOT; i++) {
        x=xpeak+xstep*i-halfdot;
        // For each dot size I try +/- 1 pixel shifts in all possible
        // directions.
        for (shift=0; shift<9; shift++) {
          switch (shift) {
            case 0: psrc=pdata->buf1+(y-1)*dx+(x-1); break;
            case 1: psrc=pdata->buf1+(y-1)*dx+(x+0); break;
            case 2: psrc=pdata->buf1+(y-1)*dx+(x+1); break;
            case 3: psrc=pdata->buf1+(y+0)*dx+(x-1); break;
            case 4: psrc=pdata->buf1+(y+0)*dx+(x+0); break;
            case 5: psrc=pdata->buf1+(y+0)*dx+(x+1); break;
            case 6: psrc=pdata->buf1+(y+1)*dx+(x-1); break;
            case 7: psrc=pdata->buf1+(y+1)*dx+(x+0); break;
            case 8: psrc=pdata->buf1+(y+1)*dx+(x+1); break; };
          switch (dotsize) {
            case 4:                    // Rounded 4x4 dot (rarely works)
              sum=(psrc[1]+psrc[2]+psrc[dx]+psrc[dx+1]+psrc[dx+2]+psrc[dx+3]+
                psrc[2*dx]+psrc[2*dx+1]+psrc[2*dx+2]+psrc[2*dx+3]+
                psrc[3*dx+1]+psrc[3*dx+2])/12;
              break;
            case 3:                    // 3x3 pixel
              sum=(psrc[0]+psrc[1]+psrc[2]+psrc[dx]+psrc[dx+1]+psrc[dx+2]+
                psrc[2*dx]+psrc[2*dx+1]+psrc[2*dx+2])/9;
              break;
            case 2:                    // 2x2 pixel (usually the best)
              sum=(psrc[0]+psrc[1]+psrc[dx]+psrc[dx+1])/4;
              break;
            default:                   // 1x1 pixel dot (or internal error)
              sum=psrc[0];
            break; };
          g[shift][j][i]=(uchar)sum;
        };
      };
    };
    // We have gathered 9 grids with 1-pixel shifts. Non-shifted grid is the
    // most probable good candidate, try it first.
    answer=Recognizebits(result,g[4],pdata);
    // Don't stop if in search-for-the-best-quality mode.
    if ((pdata->mode & M_BEST)!=0 && answer<bestanswer) {
      bestanswer=answer;
      bestresult=*result;
      uncorrected=pdata->uncorrected;
      if (answer!=0) answer=17; };
    // If data recognition fails, combine grid from subblocks SUBDX*SUBDY dots
    // with maximal dispersion. This compensates for small distortions, even
    // nonlinear, and partially for bidirectional print.
    if (answer==17) {
      for (j=0; j<NDOT; j+=SUBDY) {
        for (i=0; i<NDOT; i+=SUBDX) {
          dispmin=1.0e99; dispmax=-1.0e99;
          for (shift=0; shift<9; shift++) {
            sy=0.0; syy=0.0;
            for (y=j; y<j+SUBDY; y++) {
              for (x=i; x<i+SUBDX; x++) {
                c=g[shift][y][x];
                sy+=c; syy+=c*c;
              };
            };
            // Dispersion in the mathematical sense is a bit different beast
            // (includes Division, Square Roots and Other Incomprehensible
            // Things), but we are interested only in the shift corresponding
            // to the maximum.
            disp=syy*SUBDX*SUBDY-sy*sy;
            if (disp<dispmin) dispmin=disp;
            if (disp>dispmax) {
              dispmax=disp;
              shiftmax=shift;
            };
          };
          // If difference between minimal and maximal dispersion is low (the
          // case of mostly black/mostly white dots), I set shift to zero. 20%
          // for disp equals to roughly 10% in strict mathematical sense.
          if (dispmax-dispmin<dispmax/5.0)
            shiftmax=4;
          // Copy subblock with maximal dispersion to main grid.
          for (y=j; y<j+SUBDY; y++) {
            for (x=i; x<i+SUBDX; x++) {
              grid[y][x]=g[shiftmax][y][x];
            };
          };
        };
      };
      // Try to recognize data in the combined grid.
      answer=Recognizebits(result,grid,pdata);
      // Again, don't stop if in search-for-the-best-quality mode.
      if ((pdata->mode & M_BEST)!=0 && answer<bestanswer) {
        bestanswer=answer;
        bestresult=*result;
        uncorrected=pdata->uncorrected;
        if (answer!=0) answer=17;
      };
    };
    // If data is restored, we don't need different dot size.
    if (answer<17) break;
  };
  if (pdata->mode & M_BEST) {
    answer=bestanswer;
    *result=bestresult;
    pdata->uncorrected=uncorrected; };
  return answer;
};


static void free_decoder_buffers(t_procdata& p) {
  if (p.buf1) GlobalFree(p.buf1); if (p.buf2) GlobalFree(p.buf2);
  if (p.bufx) GlobalFree(p.bufx); if (p.bufy) GlobalFree(p.bufy);
  if (p.blocklist) GlobalFree(p.blocklist);
  p.buf1=p.buf2=nullptr; p.bufx=p.bufy=nullptr; p.blocklist=nullptr;
}

namespace paperbak {
DecodePageResult decode_page(const std::uint8_t* pixels, int width, int height) {
  DecodePageResult out;
  decoder_error.clear();
  if (!pixels || width<=0 || height<=0) { out.error="Invalid image"; return out; }
  t_procdata p{}; p.data=const_cast<uchar*>(pixels); p.sizex=width; p.sizey=height; p.blockborder=0.0f;
  Getgridposition(&p); if (p.step==0) { out.error=decoder_error; return out; }
  Getgridintensity(&p); if (p.step==0) { out.error=decoder_error; return out; }
  Getxangle(&p); if (p.step==0) { out.error=decoder_error; return out; }
  Getyangle(&p); if (p.step==0) { out.error=decoder_error; return out; }
  Preparefordecoding(&p); if (p.step==0) { out.error=decoder_error; free_decoder_buffers(p); return out; }
  out.rasterFound=true; out.gridColumns=p.nposx; out.gridRows=p.nposy; out.xStep=p.xstep; out.yStep=p.ystep; out.xAngle=p.xangle; out.yAngle=p.yangle;
  for (int y=0; y<p.nposy; ++y) for (int x=0; x<p.nposx; ++x) {
    t_data d{}; int a=Decodeblock(&p,x,y,&d);
    if (a<0) continue;
    if (a>=17) { ++out.badBlocks; continue; }
    DecodedBlock b; b.gridX=x; b.gridY=y; b.eccCorrections=a;
    std::memcpy(&b.block,&d,sizeof(d)); out.blocks.push_back(b);
    out.correctedBytes += a;
    if (d.addr==SUPERBLOCK) ++out.superBlocks; else ++out.goodBlocks;
  }
  free_decoder_buffers(p);
  return out;
}
}
