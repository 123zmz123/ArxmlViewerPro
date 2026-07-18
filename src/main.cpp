#include <QApplication>
#include <QIcon>
#include "ui/mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/ui/icon.png"));

    MainWindow w;
    w.show();

    // 可选：启动时自动打开上次目录
    // w.openDirectory("/home/zmz/cprj/ArxmlViewerPro/AutoSar");

    return app.exec();
}