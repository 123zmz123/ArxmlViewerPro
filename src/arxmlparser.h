#ifndef ARXMLPARSER_H
#define ARXMLPARSER_H

#include <QString>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNodeList>

class ArxmlDatabase;

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

    // 数据库索引
    bool indexToDatabase(ArxmlDatabase &db);

private:
    QDomDocument m_doc;
    QString m_filePath;
    void traverse(const QDomElement &el, int depth);
    QDomElement findInSubtree(const QDomElement &el, const QString &targetName);
    QDomElement findChildByTag(const QDomElement &parent, const QString &tag);

    // 数据库索引辅助
    void indexElementToDb(ArxmlDatabase &db, const QDomElement &el,
                          int depth,
                          const QString &pathPrefix);
    QString collectChildText(const QDomElement &el, const QString &tagName);
};

#endif // ARXMLPARSER_H