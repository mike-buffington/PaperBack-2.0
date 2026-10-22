# Publish PaperBack 2.0 to a private GitHub repository

## 1. Create a private repository

1. Sign in to GitHub and select **New repository**.
2. Name it `PaperBack` (or another name).
3. Choose **Private**.
4. Do not add a generated README, `.gitignore`, or license on GitHub: this source package already includes a README and `.gitignore`. Licensing needs review before a public release.
5. Create the repository.

## 2. Push the source from Terminal

Open Terminal in this extracted source folder and run:

```bash
git init
git branch -M main
git add .
git commit -m "PaperBack 2.0 for macOS"
git remote add origin https://github.com/YOUR-USERNAME/PaperBack.git
git push -u origin main
```

Replace `YOUR-USERNAME` with your actual GitHub account name. GitHub may ask you to authenticate; use GitHub Desktop or a supported Git credential manager if preferred. Do not enter a GitHub account password as a Git push password.

## 3. Build the standalone app on your Mac

```bash
cmake -S . -B xcode -G Xcode
open xcode/PaperBack.xcodeproj
```

In Xcode choose **PaperBack** and **My Mac**, then **Product → Build**. In the Products list, right-click `PaperBack.app` → **Show in Finder**.

## 4. Package the application

For example, after copying the built `PaperBack.app` into `/Applications`:

```bash
cd /Applications
ditto -c -k --sequesterRsrc --keepParent PaperBack.app ~/Desktop/PaperBack-2.0-macOS.zip
```

This preserves macOS application metadata and resource forks. Verify the ZIP extracts to a working `PaperBack.app` before publishing.

## 5. Create a private GitHub Release

1. In the private repository, select **Releases → Draft a new release**.
2. Create a tag `v2.0` targeting `main`.
3. Set the release title to `PaperBack 2.0 for macOS`.
4. Attach `PaperBack-2.0-macOS.zip` from your Desktop.
5. Publish the release.

Because the repository is private, only authorized repository members can view its releases and download its assets. To distribute the app to anyone without repository access, use a separate distribution channel or a public releases-only repository.

## Notes

- Do not commit Xcode build output or `PaperBack.app` to the source repository; attach the ZIP to the GitHub Release.
- This app's CMake configuration disables code signing. Signing and notarization are separate steps for broader distribution.
- Review third-party licensing before distributing the application or source code to other people.
