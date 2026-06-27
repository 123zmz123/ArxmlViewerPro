#include <QCoreApplication>
#include <QDebug>
#include "arxmlparser.h"
#include <QString>
#include "arxmldatabase.h"

QString path = "/home/zmz/cprj/ArxmlViewerPro/test.arxml";
QString dbpath = "/home/zmz/cprj/ArxmlViewerPro/my.db";
QString find = "/AUTOSAR_Can/EcucModuleDefs/Can";
QString field = "CATEGORY";

int main(int argc, char *argv[])
{
    ArxmlParser parser;
    ArxmlDatabase db;
    db.init(dbpath);
    QCoreApplication app(argc, argv);
    QString FieldValue= QString();
    if(!parser.parseFile(path)){
        qDebug() << "Error parsing file:" << path;
    };
    db.beginTransaction();
    parser.indexToDatabase(db);
    db.insertIndexedFile(path, "test.arxml", 0);
    db.commit();
    // FieldValue= parser.getValue(find,field);
    // if(!FieldValue.isNull()){
    //     qDebug() << "Found:" << find;
    //     qDebug() << "Value:" << FieldValue;
    // }

    // parser.setValue(find, field,"AAAAAAAAAA");
    // parser.saveFile("/home/zmz/cprj/ArxmlViewerPro/test1_copy.arxml");
    // return 0;
}