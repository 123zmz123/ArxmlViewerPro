#include <QCoreApplication>
#include <QDebug>
#include "arxmldatabase.h"
#include "arxmlparser.h"
#include "arxmlutils.h"
#include <QMap>
#include <sys/resource.h>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QMap<QString, ArxmlParser> parsers;
    QString dbpath = "/home/zmz/cprj/ArxmlViewerPro/my.db";
    QString configPath = "/home/zmz/cprj/ArxmlViewerPro/AutoSar";

    ArxmlDatabase db;
    if (!db.init(dbpath)) {
        qWarning() << "Failed to init database";
        return 1;
    }

    QStringList files = ArxmlUtils::collectArxmlFiles(configPath);
    qDebug() << "Found" << files.size() << ".arxml files";

    for (const QString &filePath : files) {
        ArxmlParser parser;
        if (!parser.parseFile(filePath)) {
            qWarning() << "Failed to parse:" << filePath;
            continue;
        }
        db.indexDocument(parser);
        parsers.insert(filePath, parser);
    }

    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    qDebug() << "\n=== Memory Usage ===";
    qDebug() << "Parsers loaded:" << parsers.size() << "files";
    qDebug() << "Max RSS:" << usage.ru_maxrss << "KB"
             << "(" << usage.ru_maxrss / 1024.0 << "MB)";

    return 0;
}
