#include "arxmlparser.h"
#include "arxmldatabase.h"
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QDomNodeList>
#include <QUuid>

bool ArxmlParser::parseFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Cannot open file:" << filePath;
        return false;
    }

    m_filePath = filePath;

    if (!m_doc.setContent(&file)) {
        qWarning() << "Failed to parse XML";
        return false;
    }

    qDebug() << "Parsing:" << QFileInfo(filePath).fileName();
    QDomElement root = m_doc.documentElement();
    // traverse(root, 0);

    return true;
}

bool ArxmlParser::saveFile(const QString &filePath)
{
    QFile out(filePath);
    if (!out.open(QIODevice::WriteOnly)) {
        qWarning() << "Cannot open file for writing:" << filePath;
        return false;
    }
    out.write(m_doc.toByteArray());
    return true;
}

void ArxmlParser::traverse(const QDomElement &el, int depth)
{
    QString indent;
    indent.fill(' ', depth * 2);

    // 收集属性
    QString attrs;
    QDomNamedNodeMap attMap = el.attributes();
    for (int i = 0; i < attMap.count(); i++) {
        QDomAttr a = attMap.item(i).toAttr();
        attrs += QString(" %1=\"%2\"").arg(a.name(), a.value());
    }

    // 分类子节点
    QDomNodeList children = el.childNodes();
    int elemCount = 0;
    for (int i = 0; i < children.count(); i++) {
        if (children.at(i).isElement())
            elemCount++;
    }

    if (elemCount == 0) {
        // 叶子节点或空元素：取文本
        QString text = el.text().trimmed();
        if (text.isEmpty()) {
            qDebug().noquote() << indent + "<" + el.tagName() + attrs + "/>";
        } else {
            qDebug().noquote() << indent + "<" + el.tagName() + attrs + ">" + text + "</" + el.tagName() + ">";
        }
    } else {
        // 容器：递归子元素
        qDebug().noquote() << indent + "<" + el.tagName() + attrs + ">";
        for (int i = 0; i < children.count(); i++) {
            QDomNode child = children.at(i);
            if (child.isElement()) {
                traverse(child.toElement(), depth + 1);
            }
        }
        qDebug().noquote() << indent + "</" + el.tagName() + ">";
    }
}

QDomElement ArxmlParser::findByPath(const QString &path)
{
    QStringList parts = path.split('/', Qt::SkipEmptyParts);
    QDomElement current = m_doc.documentElement();

    for (const QString &targetName : parts) {
        QDomElement found = findInSubtree(current, targetName);
        if (found.isNull())
            return QDomElement();  // 某段路径没找到
        current = found;
    }
    return current;
}

QDomElement ArxmlParser::findInSubtree(const QDomElement &el, const QString &targetName)
{
    QDomNodeList children = el.childNodes();
    for (int i = 0; i < children.count(); i++) {
        if (!children.at(i).isElement())
            continue;
        QDomElement child = children.at(i).toElement();

        // 检查这个元素是否有SHORT-NAME等于目标名
        QDomElement sn = findChildByTag(child, "SHORT-NAME");
        if (!sn.isNull() && sn.text().trimmed() == targetName)
            return child;

        // 递归搜索子树
        QDomElement found = findInSubtree(child, targetName);
        if (!found.isNull())
            return found;
    }
    return QDomElement();
}

QDomElement ArxmlParser::findChildByTag(const QDomElement &parent, const QString &tag)
{
    QDomNodeList children = parent.childNodes();
    for (int i = 0; i < children.count(); i++) {
        if (children.at(i).isElement() && children.at(i).toElement().tagName() == tag)
            return children.at(i).toElement();
    }
    return QDomElement();
}

QString ArxmlParser::getValue(const QString &path, const QString &tagName)
{
    QDomElement el = findByPath(path);
    if (el.isNull())
        return QString();
    // qDebug() << el.tagName();
    // qDebug() << "getValue:";
    QDomElement sn = findChildByTag(el, tagName);
    return sn.isNull() ? QString() : sn.text().trimmed();
}

bool ArxmlParser::setValue(const QString &path,  const QString &tagName, const QString &value)
{ 
    QDomElement el = findByPath(path);
    if (el.isNull()) return false;
    QDomElement sn = findChildByTag(el, tagName);
    if (sn.isNull()) return false;

    // 清除所有旧子节点，追加一个新文本节点
    while (sn.hasChildNodes())
        sn.removeChild(sn.firstChild());
    sn.appendChild(m_doc.createTextNode(value));
    return true;
}

bool ArxmlParser::addElement(const QString &path, const QString &tagName, const QString &value = QString())
{ 
    QDomElement el = findByPath(path);
    if(el.isNull()) return false;
    QDomElement s_el = m_doc.createElement(tagName);

    if(!value.isEmpty()){
        s_el.appendChild(m_doc.createTextNode(value));
    }

    el.appendChild(s_el);
    return true;
}

bool ArxmlParser::indexToDatabase(ArxmlDatabase &db)
{
    QDomElement root = m_doc.documentElement();
    if (root.isNull())
        return false;

    indexElementToDb(db, root, 0, "");
    return true;
}

void ArxmlParser::indexElementToDb(ArxmlDatabase &db, const QDomElement &el,
                                    int depth,
                                    const QString &pathPrefix)
{
    QString uuid = el.attribute("UUID");
    QString parentPath;

    if (!uuid.isEmpty()) {
        // 有 UUID → 插入记录
        QString tagName = el.tagName();
        QString shortName = collectChildText(el, "SHORT-NAME");
        QString fullPath = pathPrefix + "/" + shortName;

        QString value = collectChildText(el, "VALUE");
        db.insertElement(uuid, tagName, shortName, m_filePath, depth, fullPath, value);
        // qDebug() << "Inserting:" << tagName << shortName << uuid << fullPath;
    }

    QString shortName = collectChildText(el, "SHORT-NAME");

    if(shortName.isEmpty()){
        parentPath = pathPrefix;
    }else{
        parentPath = pathPrefix + "/" + shortName;  
        db.insertElement(QUuid::createUuid().toString(QUuid::WithoutBraces),
                         "", "justPath", m_filePath, depth, parentPath, "");
    }

    QDomNodeList children = el.childNodes();
    for (int i = 0; i < children.count(); i++) {
        if (children.at(i).isElement()) {
            indexElementToDb(db, children.at(i).toElement(),
                             depth + 1, parentPath);
        }
    }
}

QString ArxmlParser::collectChildText(const QDomElement &el, const QString &tagName)
{
    QDomNodeList children = el.childNodes();
    for (int i = 0; i < children.count(); i++) {
        if (children.at(i).isElement()
            && children.at(i).toElement().tagName() == tagName) {
            return children.at(i).toElement().text().trimmed();
        }
    }
    return QString();
}
