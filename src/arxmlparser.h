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
    bool saveFile(const QString &filePath);
    QDomElement root() { return m_doc.documentElement(); }
    QDomDocument &doc() { return m_doc; }

private:
    QDomDocument m_doc;
    void traverse(const QDomElement &el, int depth);
};

#endif // ARXMLPARSER_H