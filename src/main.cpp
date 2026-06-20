#include <QCoreApplication>
#include <QDebug>
#include "arxmlparser.h"
#include <QString>

QString path = "/home/zmz/cprj/ArxmlViewerPro/test.arxml";
QString find = "/AUTOSAR_Can/EcucModuleDefs/Can";
QString field = "CATEGORY";
int main(int argc, char *argv[])
{
    ArxmlParser parser;
    QCoreApplication app(argc, argv);
    QString FieldValue= QString();
    qDebug() << "Qt Version:" << qVersion();
    if(!parser.parseFile(path)){
        qDebug() << "Error parsing file:" << path;
    };
    FieldValue= parser.getValue(find,field);
    if(!FieldValue.isNull()){
        qDebug() << "Found:" << find;
        qDebug() << "Value:" << FieldValue;
    }

    parser.setValue(find, field,"AAAAAAAAAA");
    parser.saveFile("/home/zmz/cprj/ArxmlViewerPro/test1_copy.arxml");
    return 0;
}