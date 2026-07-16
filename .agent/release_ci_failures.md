# Release CI 失败记录（2026-07-16）

## 结论

截至修复前，GitHub API 记录 `.github/workflows/release.yml` 共运行 6 次：6 次失败，
0 次成功。第 6 次已经成功生成 Linux、Windows 两个平台的 artifact，最后失败在发布
汇总 job。

## 各次失败点

| Run | Tag | API 确认的失败点 | 后续处理 |
| --- | --- | --- | --- |
| [#1](https://github.com/Btk-Project/mksync/actions/runs/29466790590) | `v0.0.1` | Linux `Build Linux packages`（exit 255）；Windows `Build Windows packages`（exit 1） | Debian 包改为仓库自有的二进制打包脚本；Windows GUI 与 CLI 统一 PCH 配置 |
| [#2](https://github.com/Btk-Project/mksync/actions/runs/29470981538) | `v0.0.2` | `Match the tag and project version`（exit 1） | tag 与 `xmake.lua` 的版本必须完全一致 |
| [#3](https://github.com/Btk-Project/mksync/actions/runs/29471031197) | `v0.0.1` | Windows `Build Windows packages`（exit 1） | 删除重复的 NSIS 安装步骤，并让 `xmake pack` 使用非交互 `-y` |
| [#4](https://github.com/Btk-Project/mksync/actions/runs/29471505084) | `v0.0.2` | Windows `Build Windows packages`（exit 1） | 修正 Xpack 中 `LICENSE` 相对路径 |
| [#5](https://github.com/Btk-Project/mksync/actions/runs/29477934242) | `v0.0.3` | Windows `Build Windows packages`（exit 1） | 为 Qt 部署补充 `qt.deploy.qmldir = qml` |
| [#6](https://github.com/Btk-Project/mksync/actions/runs/29478510933) | `v0.0.3` | `Create the release and upload packages`（exit 1）：`fatal: not a git repository` | 发布 job 显式设置 `GH_REPO`，不再依赖本地 Git checkout |

表中的失败 job、step 和退出码来自 GitHub Actions API/check-run；具体错误正文无法通过
未认证的公开日志 API 取得时，只记录紧随其后的对应修复，不补写未经确认的日志内容。

## 第 6 次失败的完整原因

`publish` 是独立 job，每个 Actions job 都使用新的 runner。它只下载 artifact，没有执行
`actions/checkout`，但 `gh release create` 未通过 `GH_REPO` 或 `--repo` 指定仓库，因此
GitHub CLI 尝试从当前目录的 Git remote 推断仓库并失败。

此外，API 显示 `v0.0.3` 在 workflow 开始前已经是一个 prerelease，且没有附件。即使只修复
仓库定位，继续执行 `gh release create` 也会因 Release 已存在而失败。这是截图错误后面尚未
执行到的第二个失败点。

## 当前修复

- 发布 step 设置 `GH_REPO: ${{ github.repository }}`，与官方 GitHub CLI 的无本地仓库用法一致。
- 先用 `gh release view` 判断 Release 是否存在。
- 已存在时执行 `gh release upload --clobber`，保留现有 Release 元数据并幂等覆盖同名附件。
- 不存在时执行 `gh release create --verify-tag --generate-notes`。
- artifact 为空时继续硬失败；发布前继续生成并上传 `SHA256SUMS`。

## 发布前检查

1. `xmake.lua` 版本必须等于去掉 `v` 前缀后的 tag。
2. Linux、Windows artifact 必须全部成功上传后，`publish` 才能运行。
3. 发布 job 不得隐式依赖其他 job 的工作目录或 checkout。
4. 手工重跑已有 tag 时，必须走“上传并覆盖附件”分支，不能再次创建 Release。
5. 正式发布后优先发补丁版本；`--clobber` 主要用于修复失败或未完成的同版本发布。

## 官方依据

- [GitHub CLI 环境变量](https://cli.github.com/manual/gh_help_environment)：`GH_REPO` 用于给原本
  依赖本地仓库上下文的命令显式指定 `[HOST/]OWNER/REPO`。
- [gh release create](https://cli.github.com/manual/gh_release_create)：`--verify-tag` 校验远端 tag，
  `--generate-notes` 通过 GitHub Release Notes API 生成说明。
- [gh release upload](https://cli.github.com/manual/gh_release_upload)：已有 Release 使用
  `--clobber` 覆盖同名附件。
