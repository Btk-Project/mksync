#include <ilias/platform.hpp>
#include <ilias/task.hpp>
#include <ilias/fs/console.hpp>
#include <ilias/sync/scope.hpp>
#include <iostream>
#include <gtest/gtest.h>

using ILIAS_NAMESPACE::Console;
using ILIAS_NAMESPACE::Error;
using ILIAS_NAMESPACE::PlatformContext;
using ILIAS_NAMESPACE::Result;
using ILIAS_NAMESPACE::SystemError;
using ILIAS_NAMESPACE::Task;
using ILIAS_NAMESPACE::TaskScope;

Task<void> task1(int id)
{
    // 一个普通任务一旦可以被执行，就会立刻执行完。
    std::cout << "run task" << id << std::endl;
    co_return;
}

Task<void> main_task()
{
    // 主动挂起自己并等待 Task1 执行完成。
    co_await task1(1);

    // 提交一个任务
    ilias_go task1(2);
    // 此时不会立刻执行，而是等待 Task1 执行完成之后才会执行 Task2, 因为当前函数还在执行。
    std::cout << "post task2, now task2 is not running, main task countinue..." << std::endl;
    // 此时可以通过sleep或挂起其他io操作来让出当前线程，让其他任务有机会执行。
    auto ret2 = co_await ILIAS_NAMESPACE::sleep(std::chrono::milliseconds(1000));
    std::cout << "sleep 1000ms, main task countinue..." << std::endl;
    // 最后结束。
    co_return;
}

Task<void> main_task2()
{
    std::cout << "this will not run." << std::endl;
    co_return;
}

TEST(Task, TestTask)
{
    // main 是一个普通函数，不是协程，所以不能直接使用 co_await, co_return 等协程关键字
    PlatformContext
        context; // 创建平台默认上下文，该上下文中包含了协程的调度器，不同上下文支持的操作会有不同。
    std::cout << "main start" << std::endl;
    ilias_wait main_task(); // 等待主任务完成, 通过 ilias_wait/ilias_go 提交任务到协程调度器中执行
    // ilias_wait将会等待协程调度器中的任务执行完成，并返回结果
    ilias_go main_task2(); // 提交任务到协程调度器中执行，但不等待任务完成
    // ilias_go
    // 将会立即返回，不会等待任务完成，也不会进入调度器，所以任务不会被执行，除非主动进入调度器。
    // 因此ilias_go主要用于提交不需要立即执行的任务，并在之后会挂起自己来执行其他被提交的任务。
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}