#include <QCoreApplication>
#include <QDebug>
#include "arxmlparser.h"
#include <QString>

QString path = "/home/zmz/cprj/ArxmlViewerPro/test.arxml";
int main(int argc, char *argv[])
{
    ArxmlParser parser;
    QCoreApplication app(argc, argv);
    qDebug() << "Qt Version:" << qVersion();
    if(!parser.parseFile(path)){
        qDebug() << "Error parsing file:" << path;
    };
    parser.saveFile("/home/zmz/cprj/ArxmlViewerPro/test1_copy.arxml");
    return 0;
}