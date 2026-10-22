#import <Foundation/Foundation.h>

@interface PaperBakBridge : NSObject
+ (NSDictionary<NSString *, id> * _Nullable)inspectPages:(NSArray<NSString *> *)paths error:(NSError **)error;
+ (BOOL)restorePages:(NSArray<NSString *> *)paths to:(NSString *)destination password:(NSString *)password error:(NSError **)error;
+ (NSDictionary<NSString *, id> * _Nullable)inspect:(NSString *)path error:(NSError **)error;
+ (BOOL)restore:(NSString *)source to:(NSString *)destination password:(NSString *)password error:(NSError **)error;
+ (NSDictionary<NSString *, NSNumber *> * _Nullable)sourceLayout:(NSString *)source compression:(NSString *)compression lzmaLevel:(NSInteger)lzmaLevel error:(NSError **)error;
+ (NSInteger)requiredCells:(NSString *)path error:(NSError **)error;
+ (BOOL)createPNG:(NSString *)source destination:(NSString *)destination columns:(NSInteger)columns dotPercent:(NSInteger)dotPercent compression:(NSString *)compression lzmaLevel:(NSInteger)lzmaLevel error:(NSError **)error;
+ (BOOL)createEncryptedPNG:(NSString *)source destination:(NSString *)destination columns:(NSInteger)columns rows:(NSInteger)rows recoveryGroupSize:(NSInteger)recoveryGroupSize password:(NSString *)password dotPercent:(NSInteger)dotPercent compression:(NSString *)compression lzmaLevel:(NSInteger)lzmaLevel error:(NSError **)error;
+ (BOOL)createMultiPagePNGs:(NSString *)source destinationFolder:(NSString *)folder columns:(NSInteger)columns rows:(NSInteger)rows pages:(NSInteger)pages recoveryGroupSize:(NSInteger)recoveryGroupSize password:(NSString *)password dotPercent:(NSInteger)dotPercent compression:(NSString *)compression lzmaLevel:(NSInteger)lzmaLevel error:(NSError **)error;
@end
