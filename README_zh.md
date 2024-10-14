# mksync

## 简介 ([English](/README.md))

mksync 可以在 Windows 和 Linux 系统的多台计算机之间同步鼠标和键盘。（macOS 可能在未来的计划中）

它还支持剪贴板共享、文件传输和拖放文件。（未来将通过插件添加支持）

## 它是什么？
Mksync 是一款免费的开源键盘和鼠标共享库和应用程序。

mksync 中的应用程序模拟了 KVM 切换器的功能，在历史上，KVM 切换器可以让你使用一个键盘和鼠标控制多台计算机，方法是转动盒子上的拨盘，随时切换你要控制的机器。 Mksync 通过软件实现了这一功能，你只需将鼠标移动到屏幕边缘，或通过按键将焦点切换到不同的系统，就能告诉它要控制哪台电脑。

该 app 的基本功能没有图形用户界面，但我们采用插件方式来支持图形用户界面。

该 mks 库是 mksync 的开发库，您可以使用 mksync 的协议来开发类似的应用程序，或者为 mksync 开发插件以获得更多功能。

Mksync 是基于协同、障碍和输入跳跃的灵感而开发的。你也可以说 mksync 是它们的重构版本。

## 有什么不同？

由于在使用 synergy 和 barrier 时发现连接体验不佳。我们需要事先在服务器上设置可能连接的用户的用户名，并设置该用户的屏幕位置。只有做好这些准备工作后，我们才能使用同名客户端进行连接。这非常不方便，而且当某些电脑有多个屏幕时，我们无法像扩展屏幕那样移动它们！

而且它们不支持使用剪贴板跨系统传输文件，也不支持拖放文件。

由于上述问题的存在，我们决定在 synergy 和 barrier 的基础上进行重构。并决定使用插件化解决方案来提供更多内容。

## 项目目标

#### 版本 v0.1.0
在 Windows 和 Linux 系统的多台计算机之间同步鼠标和键盘。（基本功能）

#### 版本 v0.2.0
实现插件管理器。为剪贴板共享、文件传输、文件拖放提供 mks 库支持。

#### 版本 v0.3.0
实现剪贴板共享。

#### 版本 v1.0.0
实现文件传输。（此时，mksync 将发布正式版本）

#### 版本 v1.0.0 之后
只有在正式版本推出后，我们才会根据需要实现文件拖放和对更多系统（如 mocOS）的支持。 当然，图形用户界面也计划在正式版本发布后实现。

## 如何构建?
mksync 目前只是测试版，没有发布安装程序，你需要自己从源代码中编译和安装。

mksync 使用 xmake 构建，相关 xmake 教程请查看 [xmake-io](https://xmake.io/).

### 配置

在构建之前，您需要对项目进行配置。

#### Windows 上使用 msvc
```
xmake f -p windows -a x64 -m releasedbg -k shared --runtimes=MD --third_party_kind=shared -cv
```
#### Windows 上使用 clang-cl
```
xmake f -p windows -a x64 -m releasedbg -k shared --runtimes=MD --toolchain=clang-cl --third_party_kind=shared -cv
```
#### Windows 上使用 mingw
```
xmake f -p mingw -a x86_64 -m releasedbg -k shared --runtimes=stdc++_shared --third_party_kind=shared -cv
```
#### Linux 上使用 gcc
```
xmake f -a x86_64 -m release -k shared --runtimes=stdc++_shared --third_party_kind=shred -cv
```
#### Linux 上使用 gcc-13
```
xmake f -a x86_64 -m release -k shared --runtimes=stdc++_shared --toolchain=gcc-13 --third_party_kind=shred -cv
```
#### Linux 上使用 llvm
```
xmake f -a x86_64 -m release -k shared --runtimes=stdc++_shared --toolchain=llvm --third_party_kind=shred -cv
```

### 编译

配置完成后，就可以使用此命令编译此项目了：
```
xmake
```
或者使用此命令全部重新编译：
```
xmake -r
```
或者使用此命令显示编译详情：
```
xmake -v
```
以及它们的组合：
```
xmake -rv
```