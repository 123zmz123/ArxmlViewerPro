#ifndef ARXMLDATABASE_H
#define ARXMLDATABASE_H

#include <QString>
#include <QStringList>
#include <QSqlDatabase>
#include <QSqlQuery>

class ArxmlParser;

class ArxmlDatabase
{
public:
    ArxmlDatabase();
    ~ArxmlDatabase();

    bool init(const QString &dbPath = ":memory:");

    // 事务
    bool beginTransaction();
    bool commit();
    bool rollback();

    // 文件管理
    bool isFileIndexed(const QString &filePath);
    QStringList indexedFiles();
    bool insertIndexedFile(const QString &filePath, const QString &fileName, qint64 mtime);
    bool removeFile(const QString &filePath);

    // 索引操作
    bool indexDocument(ArxmlParser &parser);

    // 元素操作
    bool insertElement(const QString &uuid,
                       const QString &tagName,
                       const QString &shortName,
                       const QString &filePath,
                       int depth,
                       const QString &fullPath,
                       const QString &value);

    QSqlQuery searchByUuid(const QString &uuid);

    // 查询
    QSqlQuery searchByShortName(const QString &keyword, const QString &filePath = {});
    QSqlQuery searchByTagName(const QString &tag, const QString &filePath = {});
    QSqlQuery searchByFullPath(const QString &path, const QString &filePath);
    QSqlQuery allElements(const QString &filePath = {});

private:
    QSqlDatabase m_db;

    void createTables();
};

#endif // ARXMLDATABASE_H