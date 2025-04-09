#include "mainwindow.h"
#include <QApplication>
#include <spdlog/spdlog.h>

#pragma comment(linker, "/subsystem:console") // 设置连接器选项

int main(int argc, char *argv[]) {
    ILIAS_LOG_SET_LEVEL(ILIAS_INFO_LEVEL);
    spdlog::set_level(spdlog::level::info);
    QApplication app(argc, argv);
    MainWindow wind;
    wind.show();
    return QApplication::exec();
}
