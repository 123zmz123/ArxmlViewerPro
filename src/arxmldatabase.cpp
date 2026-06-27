#include "arxmldatabase.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>

ArxmlDatabase::ArxmlDatabase() {}

ArxmlDatabase::~ArxmlDatabase()
{
    if (m_db.isOpen())
        m_db.close();
}

bool ArxmlDatabase::init(const QString &dbPath)
{
    const QString connName = "arxml_main";

    if (QSqlDatabase::contains(connName))
        QSqlDatabase::removeDatabase(connName);

    m_db = QSqlDatabase::addDatabase("QSQLITE", connName);
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        qWarning() << "Cannot open database:" << m_db.lastError().text();
        return false;
    }

    createTables();
    return true;
}

void ArxmlDatabase::createTables()
{
    QSqlQuery q(m_db);

    q.exec("CREATE TABLE IF NOT EXISTS elements ("
           "uuid            TEXT UNIQUE NOT NULL,"
           "tag_name        TEXT NOT NULL,"
           "short_name      TEXT,"
           "file_path       TEXT NOT NULL,"
           "depth           INTEGER DEFAULT 0,"
           "full_path       TEXT,"
           "value           TEXT"
           ")");

    q.exec("CREATE TABLE IF NOT EXISTS indexed_files ("
           "id            INTEGER PRIMARY KEY AUTOINCREMENT,"
           "file_path     TEXT UNIQUE NOT NULL,"
           "file_name     TEXT,"
           "file_mtime    INTEGER,"
           "indexed_at    DATETIME DEFAULT CURRENT_TIMESTAMP,"
           "element_count INTEGER DEFAULT 0"
           ")");

    q.exec("CREATE INDEX IF NOT EXISTS idx_elements_short_name ON elements(short_name)");
    q.exec("CREATE INDEX IF NOT EXISTS idx_elements_tag_name ON elements(tag_name)");
    q.exec("CREATE INDEX IF NOT EXISTS idx_elements_file_path ON elements(file_path)");

    // Schema 迁移：自动补齐缺失列
    QSqlQuery check(m_db);
    check.exec("PRAGMA table_info(elements)");
    bool hasValue = false;
    while (check.next()) {
        if (check.value(1).toString() == "value") {
            hasValue = true;
            break;
        }
    }
    if (!hasValue) {
        q.exec("ALTER TABLE elements ADD COLUMN value TEXT");
        qDebug() << "Migrated: added value column to elements";
    }
}

bool ArxmlDatabase::beginTransaction()
{
    return m_db.transaction();
}

bool ArxmlDatabase::commit()
{
    return m_db.commit();
}

bool ArxmlDatabase::rollback()
{
    return m_db.rollback();
}

bool ArxmlDatabase::isFileIndexed(const QString &filePath)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT id FROM indexed_files WHERE file_path = ?");
    q.addBindValue(filePath);
    q.exec();
    return q.next();
}

QStringList ArxmlDatabase::indexedFiles()
{
    QStringList files;
    QSqlQuery q(m_db);
    q.exec("SELECT file_path FROM indexed_files ORDER BY file_path");
    while (q.next())
        files.append(q.value(0).toString());
    return files;
}

bool ArxmlDatabase::insertIndexedFile(const QString &filePath, const QString &fileName, qint64 mtime)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT OR REPLACE INTO indexed_files (file_path, file_name, file_mtime, element_count) "
              "VALUES (?, ?, ?, (SELECT COUNT(*) FROM elements WHERE file_path = ?))");
    q.addBindValue(filePath);
    q.addBindValue(fileName);
    q.addBindValue(mtime);
    q.addBindValue(filePath);
    return q.exec();
}

bool ArxmlDatabase::removeFile(const QString &filePath)
{
    m_db.transaction();
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM elements WHERE file_path = ?");
    q.addBindValue(filePath);
    q.exec();
    q.prepare("DELETE FROM indexed_files WHERE file_path = ?");
    q.addBindValue(filePath);
    q.exec();
    m_db.commit();
    return true;
}

bool ArxmlDatabase::insertElement(const QString &uuid,
                                   const QString &tagName,
                                   const QString &shortName,
                                   const QString &filePath,
                                   int depth,
                                   const QString &fullPath,
                                   const QString &value)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT OR REPLACE INTO elements "
              "(uuid, tag_name, short_name, file_path, depth, full_path, value) "
              "VALUES (?, ?, ?, ?, ?, ?, ?)");
    q.addBindValue(uuid);
    q.addBindValue(tagName);
    q.addBindValue(shortName);
    q.addBindValue(filePath);
    q.addBindValue(depth);
    q.addBindValue(fullPath);
    q.addBindValue(value);
    return q.exec();
}

QSqlQuery ArxmlDatabase::searchByUuid(const QString &uuid)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT uuid, tag_name, short_name, file_path, depth, full_path "
              "FROM elements WHERE uuid = ?");
    q.addBindValue(uuid);
    q.exec();
    return q;
}

QSqlQuery ArxmlDatabase::searchByShortName(const QString &keyword, const QString &filePath)
{
    QSqlQuery q(m_db);
    if (filePath.isEmpty()) {
        q.prepare("SELECT uuid, tag_name, short_name, file_path, full_path "
                  "FROM elements WHERE short_name LIKE ? ORDER BY short_name");
        q.addBindValue("%" + keyword + "%");
    } else {
        q.prepare("SELECT uuid, tag_name, short_name, file_path, full_path "
                  "FROM elements WHERE short_name LIKE ? AND file_path = ? ORDER BY short_name");
        q.addBindValue("%" + keyword + "%");
        q.addBindValue(filePath);
    }
    q.exec();
    return q;
}

QSqlQuery ArxmlDatabase::searchByTagName(const QString &tag, const QString &filePath)
{
    QSqlQuery q(m_db);
    if (filePath.isEmpty()) {
        q.prepare("SELECT uuid, tag_name, short_name, file_path, full_path "
                  "FROM elements WHERE tag_name = ? ORDER BY short_name");
        q.addBindValue(tag);
    } else {
        q.prepare("SELECT uuid, tag_name, short_name, file_path, full_path "
                  "FROM elements WHERE tag_name = ? AND file_path = ? ORDER BY short_name");
        q.addBindValue(tag);
        q.addBindValue(filePath);
    }
    q.exec();
    return q;
}

QSqlQuery ArxmlDatabase::searchByFullPath(const QString &fullPath, const QString &filePath)
{ 
    QSqlQuery q(m_db);
    q.prepare("SELECT uuid, tag_name, short_name, file_path, full_path "
              "FROM elements WHERE full_path LIKE ? AND file_path LIKE? ORDER BY short_name");
    q.addBindValue(fullPath);
    q.addBindValue(filePath);
    q.exec();
    return q;
}

QSqlQuery ArxmlDatabase::allElements(const QString &filePath)
{
    QSqlQuery q(m_db);
    if (filePath.isEmpty()) {
        q.exec("SELECT uuid, tag_name, short_name, file_path, full_path "
               "FROM elements ORDER BY file_path, full_path");
    } else {
        q.prepare("SELECT uuid, tag_name, short_name, file_path, full_path "
                  "FROM elements WHERE file_path = ? ORDER BY full_path");
        q.addBindValue(filePath);
        q.exec();
    }
    return q;
}