#include "arxmlutils.h"

#include <QDir>
#include <QDirIterator>

QStringList ArxmlUtils::collectArxmlFiles(const QString &dirPath)
{
    QStringList result;
    QDirIterator it(dirPath, {"*.arxml"}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        result.append(it.filePath());
    }
    return result;
}
