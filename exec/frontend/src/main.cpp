#include "mainwindow.h"
#include <QApplication>
#include <spdlog/spdlog.h>

#pragma comment(linker, "/subsystem:console") // 设置连接器选项

int main(int argc, char *argv[])
{

#if defined(_WIN32)
    ::SetConsoleCP(65001);
    ::SetConsoleOutputCP(65001);
    std::setlocale(LC_ALL, ".utf-8");
#endif

    ILIAS_LOG_SET_LEVEL(ILIAS_INFO_LEVEL);
    NEKO_LOG_SET_LEVEL(NEKO_LOG_LEVEL_INFO);
    spdlog::set_level(spdlog::level::info);
    QApplication app(argc, argv);
    MainWindow   wind;
    wind.show();
    return QApplication::exec();
}
