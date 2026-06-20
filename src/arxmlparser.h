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
    QDomElement findByPath(const QString &path);
    QString getValue(const QString &path, const QString &tagName = "VALUE");
    bool    setValue(const QString &path,  const QString &tagName,const QString &value);
private:
    QDomDocument m_doc;
    void traverse(const QDomElement &el, int depth);
    QDomElement findInSubtree(const QDomElement &el, const QString &targetName);
    QDomElement findChildByTag(const QDomElement &parent, const QString &tag);
};

#endif // ARXMLPARSER_H