#ifndef ARXMLPARSER_H
#define ARXMLPARSER_H

#include <QString>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNodeList>

class ArxmlParser
{
public:
    ArxmlParser() = default;

    bool parseFile(const QString &filePath);

private:
    void traverse(const QDomElement &el, int depth);
};

#endif // ARXMLPARSER_H