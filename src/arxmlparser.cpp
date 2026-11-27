#include "arxmlparser.h"
#include "arxmldatabase.h"
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QDomNodeList>
#include <QUuid>

namespace {
    bool hasElementChild(const QDomElement &el) {
        const QDomNodeList ch = el.childNodes();
        for(int i=0;i<ch.count();++i)
            if (ch.at(i).isElement()) return true;
        return false;
    }

    QString firstChildText(const QDomElement &el, const QString &tag)
    {
        const QDomNodeList ch = el.childNodes();
        for (int i = 0; i < ch.count(); ++i)
        {
            if (ch.at(i).isElement() && ch.at(i).toElement().tagName() == tag)
                return ch.at(i).toElement().text().trimmed();
        }
        return QString();
    }

    // 统计叶子(有文本的末端)数量，用于“已折叠 N 项”
    int countLeafEntries(const QDomElement &el)
    {
        if (!hasElementChild(el))
            return 1;
        int n = 0;
        const QDomNodeList ch = el.childNodes();
        for (int i = 0; i < ch.count(); ++i)
            if (ch.at(i).isElement())
                n += countLeafEntries(ch.at(i).toElement());
        return n;
    }

    void dumpReadableRec(const QDomElement &el,
                         const QString &prefix, bool isLast, bool isRoot,
                         QString &out)
    {
        const QString tag = el.tagName();

        // 组织标签文本
        QString label;
        if (!hasElementChild(el))
        { // 叶子：tag : text
            const QString text = el.text().trimmed();
            label = text.isEmpty() ? tag : tag + " : " + text;
        }
        else
        { // 容器：tag  (shortName)
            const QString sn = firstChildText(el, "SHORT-NAME");
            label = sn.isEmpty() ? tag : tag + "  (" + sn + ")";
        }

        const QString connector = isRoot ? QString() : (isLast ? "└─ " : "├─ ");
        out += prefix + connector + label;

        // ADMIN-DATA 折叠成一行
        if (tag == "ADMIN-DATA")
        {
            out += QString("  …（元数据，已折叠 %1 项）\n").arg(countLeafEntries(el));
            return;
        }
        out += "\n";

        // 收集并递归子元素
        QList<QDomElement> kids;
        const QDomNodeList ch = el.childNodes();
        for (int i = 0; i < ch.count(); ++i)
            if (ch.at(i).isElement())
                kids.append(ch.at(i).toElement());

        const QString childPrefix = isRoot ? QString()
                                           : prefix + (isLast ? "   " : "│  ");
        for (int i = 0; i < kids.size(); ++i)
            dumpReadableRec(kids.at(i), childPrefix, i == kids.size() - 1, false, out);
    }
}

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

QString ArxmlParser::dumpReadable(const QDomElement &el)
{
    QString out;
    if (el.isNull()) return out;
    dumpReadableRec(el, QString(), true, true, out);
    return out;
}

QString ArxmlParser::dumpReadableByPath(const QString &path)
{
    QDomElement el = findByPath(path);
    if (el.isNull())
        return QString("(not found: %1)").arg(path);
    return dumpReadable(el);
}