#include "arxmlparser.h"
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QDomNodeList>

bool ArxmlParser::parseFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Cannot open file:" << filePath;
        return false;
    }

    if (!m_doc.setContent(&file)) {
        qWarning() << "Failed to parse XML";
        return false;
    }

    qDebug() << "Parsing:" << QFileInfo(filePath).fileName();
    QDomElement root = m_doc.documentElement();
    traverse(root, 0);

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