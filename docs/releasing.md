# 发布与 GitHub 二进制包

项目现在把“编译”“临时产物”和“正式发布”分成两条工作流：

- `.github/workflows/ci.yml`：每次推送到 `main` 或提交 PR 时运行。Linux C++23
  会构建 CLI、GUI 并执行测试；Windows C++23 构建 CLI 与 GUI；Docker 额外验证
  GCC 16.1/C++26，但它不是默认发布标准。
- `.github/workflows/release.yml`：只在推送 `vX.Y.Z` tag 或手工指定已有 tag 时运行。
  Linux 生成 `.deb` 和 `.tar.gz`，Windows 生成 NSIS `.exe` 安装器和 `.zip`，最后生成
  `SHA256SUMS` 并把全部文件附加到同一个 GitHub Release。

## Actions artifact 与 Release asset 的区别

构建 job 不能直接共享本地磁盘，所以各平台先用 `actions/upload-artifact` 上传临时
artifact。`publish` job 再用 `actions/download-artifact` 汇总它们。artifact 属于某次
Actions 运行，主要用于调试和 job 间传递，并会按保留期过期。

`gh release create TAG files...` 上传的是 Release asset。它显示在仓库 Releases 页面，
面向最终用户，生命周期跟随 Release。这就是常见开源项目在一个版本下面列出多个系统
安装包的实现方式。

## 第一次发布

1. 确保仓库 Settings -> Actions -> General -> Workflow permissions 允许工作流写入内容。
   工作流已经只在 `publish` job 声明 `contents: write`，不需要创建个人访问令牌。
2. 把 `xmake.lua` 顶部的版本改成准备发布的版本，例如 `0.1.0`。tag 必须与它完全一致，
   只是 tag 多一个 `v`。
3. 合并并确认 CI 全绿。
4. 创建并推送 tag：

   ```bash
   git tag -s v0.1.0 -m "mksync 0.1.0"
   git push origin v0.1.0
   ```

   没有配置 GPG 签名时，可暂时用 `git tag -a` 代替 `git tag -s`。
5. 打开仓库 Actions -> Release 查看进度。全部 job 成功后，Releases 页面会出现安装包和
   自动生成的变更说明。

版本校验 job 会拒绝类似“tag 是 `v0.1.0`，但 `xmake.lua` 仍是 `0.0.1`”的发布，防止
安装包文件名和 Release 版本不一致。

## 手工重跑已有 tag

在 Actions -> Release -> Run workflow 中填写已有 tag，例如 `v0.1.0`。这个入口用于修复
CI 环境后重新构建；它不会替你选择未提交的本地代码，也不会绕过 tag/项目版本校验。

如果目标 Release 已经存在，`gh release create` 会失败。此时应先判断是修复工作流后重建
同一版本，还是发布补丁版本。正式发布后更推荐增加版本并创建新 tag，避免悄悄替换用户
已经下载的二进制文件。

## 本地打包

先按目标平台配置 release 和 GUI，然后选择格式：

```bash
xmake f -c -m release --stdcxx=23 --enable_gui=y --enable_tests=n
xmake pack -f targz -o dist
```

Debian/Ubuntu 环境可使用 `-f deb`，Windows 可使用 `-f nsis,zip`。`lua/pack.lua` 同时绑定
`mksync` 和 `mksync-gui`：Windows 上 Xmake 的 `qt.quickapp` 安装钩子会运行
`windeployqt`；macOS 会打包 Qt `.app`；Linux 的 Qt、XCB、Wayland 和 portal 库按系统依赖
处理，`.deb` 由原生工具生成依赖关系。当前没有 macOS 输入后端，因此 Release 暂不发布
无实际功能的 macOS 安装包。
