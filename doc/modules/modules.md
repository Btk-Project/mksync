

# [Base](./Base/Base.md)<a id="Base"></a>
此模块为 headeronly.

## 作用
用于消除平台差异，消除编译器差异的头文件

# [Proto](./Proto/Proto.md)<a id="Proto"></a>
此模块为 library.

## 作用
用于规范 mksync 的各个功能的协议，保证跨平台数据的准确性。

## 备注
所有传输的数据需要走网络，因此统一使用大端序

# [Network](./Network/Network.md)<a id="Network"></a>
此模块为 library.

## 作用
用于处理 mksync 的跨平台网络通信。

# [Display](./Display/Display.md)<a id="Display"></a>
此模块为 library.

## 作用
用于跨平台获取屏幕分辨率和显示器设置，为鼠标功能做铺垫

# [Mouse](./Mouse/Mouse.md)<a id="Mouse"></a>
此模块为 library.

## 作用
用于跨平台 Hook 鼠标。拦截并发送鼠标坐标，以及鼠标点击事件。

## 依赖
[Display](#Display)

[Network](#Network)

# [Keyboard](./Keyboard/Keyboard.md)<a id="Keyboard"></a>
此模块为 library.

## 作用
用于跨平台 Hook 键盘。拦截并发送键盘事件。

## 依赖
[Network](#Network)