#include "mainwindow.h"
#include <QApplication>

#pragma comment(linker, "/subsystem:console") // 设置连接器选项

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    MainWindow wind;
    wind.show();
    return QApplication::exec();
}
