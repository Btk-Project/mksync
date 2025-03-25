#include "mainwindow.h"
#include <QApplication>

#pragma comment(linker, "/subsystem:console") // 设置连接器选项

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
