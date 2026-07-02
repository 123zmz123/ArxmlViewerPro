#include <QApplication>
#include <QDebug>
#include "ui/mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    MainWindow w;
    w.show();

    // 可选：启动时自动打开上次目录
    // w.openDirectory("/home/zmz/cprj/ArxmlViewerPro/AutoSar");

    return app.exec();
}