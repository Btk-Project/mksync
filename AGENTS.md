# 项目指导

编程语言使用C++26 编译器为GCC16.1
mksync目标是做鼠标和键盘的同步 作为input leap的类似的项目

## 错误处理

使用`Result<T, E>` 作为错误处理类型 `IoResult<T>` 是 `Result<T, std::error_code>` 的别名
异常只用于致命错误
Err来标记错误类型

```cpp
auto fn() -> IoResult<void> {
    return Err(MyErrorType::Err);
}
```

- 方便的宏 ILIAS_TRY(var, expression) 和 ILIAS_TRYV(expression) 用于向上传递错误的样板代码
- 在协程的异步版 ILIAS_CO_TRY 和 ILIAS_CO_TRYV
- 如果只向上传递 尽量使用宏增强可读性

## 异步

使用ilias作为异步运行时 类似tokio 取消语义和tokio一样
`Task<T>` 作为异步任务 `IoTask<T>` 是 `Task<IoResult<T>` 的别名

- ilias::TaskScope 进行结构化并发
- whenAny whenAll
- spawn 作为裸任务

## 反射

使用C++26反射减少样板代码 反射的工具库在
`refl/*.hpp` 里面

## Project Layout

- `src/`：
- `src/preinclude.hpp` 所有源文件除了 refl部分 需要包含这个头文件
- `src/refl`：C++26 反射辅助库
- `tests/`：测试代码
- `xmake.lua`：构建配置和依赖声明

## 构建

xmake build 进行build
xmake test 进行测试

## 日志和记忆

长期计划和目标应该放入.agent文件夹下

## Coding Style & Naming Conventions

Use UTF-8 source files, 4-space indentation, no tabs, and a 100-column limit as defined in `.clang-format_18` and `.clang-format_19`. The clang-tidy configuration enables bugprone, cert, analyzer, modernize, performance, portability, readability, and related checks. Follow the naming rules in `.clang-tidy`: types and enums use `CamelCase`, functions and local variables use `camelBack`, global variables use `gCamelCase`, and static variables use `kCamelCase`.

## Testing Guidelines

There is no active test target yet. Add new tests under `tests/` and wire them through Xmake when introducing behavior that can regress. Prefer names that describe the component and behavior, such as `refl_enum_error_test.cpp`. Once a test target exists, document and use the corresponding `xmake test` or target-specific command.

## Security & Configuration Tips

Do not commit local build state from `.xmake/`, `.vscode/`, `build/`, or `bin/`. Keep dependency changes centralized in `xmake.lua`, and document any new system package requirements near the relevant platform block.
