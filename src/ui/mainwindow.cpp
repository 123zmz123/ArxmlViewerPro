#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "../arxmlparser.h"
#include "../arxmldatabase.h"
#include "../arxmlutils.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QStandardItemModel>
#include <QTreeView>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_db(new ArxmlDatabase)
{
    ui->setupUi(this);
    loadStyleSheet();

    m_searchInput = ui->searchInput;
    m_fileTree    = ui->fileTree;
    m_treeView    = ui->arxmlTree;

    // 左侧：文件列表
    m_fileModel = new QStandardItemModel(this);
    m_fileModel->setHorizontalHeaderLabels({"File"});
    m_fileTree->setModel(m_fileModel);
    m_fileTree->setHeaderHidden(false);
    m_fileTree->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // 右侧：XML 树
    m_treeModel = new QStandardItemModel(this);
    m_treeModel->setHorizontalHeaderLabels({"Name", "Tag", "Value", "Depth"});
    m_treeView->setModel(m_treeModel);
    m_treeView->setAlternatingRowColors(true);
    m_treeView->setAnimated(true);
    m_treeView->setSortingEnabled(true);

    // splitter 比例：左侧 1/4，右侧 3/4
    ui->splitter->setStretchFactor(0, 1);
    ui->splitter->setStretchFactor(1, 3);

    statusBar()->showMessage("Ready");

    // 信号槽
    connect(ui->searchButton, &QPushButton::clicked, this, &MainWindow::onSearch);
    connect(ui->searchInput,  &QLineEdit::returnPressed, this, &MainWindow::onSearch);
    connect(ui->actionOpen,   &QAction::triggered, this, &MainWindow::onOpenFolder);
    connect(ui->actionExit,   &QAction::triggered, this, &MainWindow::close);
    connect(ui->actionRefresh, &QAction::triggered, this, [this]() {
        if (!m_parsers.isEmpty())
            openDirectory(m_currentDir);
    });
    connect(m_fileTree, &QTreeView::clicked, this, &MainWindow::onFileClicked);
}

MainWindow::~MainWindow()
{
    qDeleteAll(m_parsers);
    delete m_db;
    delete ui;
}

void MainWindow::loadStyleSheet()
{
    QFile qss("src/ui/style.qss");
    if (qss.open(QFile::ReadOnly)) {
        setStyleSheet(qss.readAll());
        qss.close();
    } else {
        qWarning() << "Cannot load style.qss";
    }
}

void MainWindow::onOpenFolder()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Open ARXML Folder");
    if (dir.isEmpty()) return;
    openDirectory(dir);
}

void MainWindow::openDirectory(const QString &dirPath)
{
    m_currentDir = dirPath;

    statusBar()->showMessage("Indexing...");
    qApp->processEvents();

    // 清理旧数据
    qDeleteAll(m_parsers);
    m_parsers.clear();
    m_treeModel->removeRows(0, m_treeModel->rowCount());

    if (!m_db->init(":memory:")) {
        statusBar()->showMessage("DB init failed");
        return;
    }

    QStringList files = ArxmlUtils::collectArxmlFiles(dirPath);
    qDebug() << "Found" << files.size() << ".arxml files";

    // 清旧 parser
    qDeleteAll(m_parsers);
    m_parsers.clear();
    m_fileModel->removeRows(0, m_fileModel->rowCount());
    m_treeModel->removeRows(0, m_treeModel->rowCount());

    for (const QString &filePath : files) {
        auto *parser = new ArxmlParser;
        if (!parser->parseFile(filePath)) {
            qWarning() << "Failed to parse:" << filePath;
            delete parser;
            continue;
        }

        m_db->indexDocument(*parser);
        m_parsers.insert(filePath, parser);

        // 左侧文件列表
        QString fileName = QFileInfo(filePath).fileName();
        QStandardItem *fileItem = new QStandardItem(fileName);
        fileItem->setData(filePath, Qt::UserRole);
        fileItem->setFlags(fileItem->flags() & ~Qt::ItemIsEditable);
        m_fileModel->appendRow(fileItem);
    }

    statusBar()->showMessage(
        QString("Loaded %1 files").arg(files.size()));
}

void MainWindow::onFileClicked(const QModelIndex &index)
{
    QString filePath = m_fileModel->data(index, Qt::UserRole).toString();
    if (filePath.isEmpty()) return;

    auto it = m_parsers.find(filePath);
    if (it == m_parsers.end()) return;

    // 清右侧树，重建
    m_treeModel->removeRows(0, m_treeModel->rowCount());
    buildTreeFromParser(**it, filePath);
    m_treeView->expandAll();

    statusBar()->showMessage(
        QString("%1 | %2 elements").arg(QFileInfo(filePath).fileName())
                                   .arg(m_treeModel->rowCount()));
}

void MainWindow::buildTreeFromParser(ArxmlParser &parser, const QString &filePath)
{
    QDomElement root = parser.root();
    if (root.isNull()) return;

    QString fileName = QFileInfo(filePath).fileName();
    QStandardItem *fileItem = new QStandardItem(fileName);
    fileItem->setData(filePath, Qt::UserRole + 2);
    fileItem->setFlags(fileItem->flags() & ~Qt::ItemIsEditable);
    m_treeModel->invisibleRootItem()->appendRow(fileItem);

    // 仿照 ArxmlParser::traverse() 风格
    std::function<void(const QDomElement &, QStandardItem *)> walk =
        [&](const QDomElement &el, QStandardItem *parent) {
            QString tag  = el.tagName();
            QString uuid = el.attribute("UUID");

            // 分类子节点：是否有子元素
            QDomNodeList children = el.childNodes();
            int elemCount = 0;
            for (int i = 0; i < children.count(); i++) {
                if (children.at(i).isElement())
                    elemCount++;
            }

            // 跳过纯容器标签，但继续递归子元素
            static const QStringList skipTags = {"AR-PACKAGE",
                "AR-PACKAGES", "ELEMENTS", "CONTAINERS", "SUB-CONTAINERS",
                "PARAMETERS", "REFERENCES", "LITERALS", "ECUC-CONTAINER-VALUES"
            };

            if (skipTags.contains(tag)) {
                for (int i = 0; i < children.count(); i++) {
                    QDomNode child = children.at(i);
                    if (child.isElement())
                        walk(child.toElement(), parent);
                }
                return;
            }

            QString display;
            if (elemCount == 0) {
                // 叶子节点：取文本
                QString text = el.text().trimmed();
                if (text.isEmpty())
                    display = QString("<%1/>").arg(tag);
                else
                    display = text;
            } else {
                // 容器：显示标签名
                display = tag;
            }

            QStandardItem *item = new QStandardItem(display);
            item->setData(uuid, Qt::UserRole);
            item->setData(tag,  Qt::UserRole + 1);
            item->setData(filePath, Qt::UserRole + 2);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            parent->appendRow(item);

            // Tag 列 + Value 列
            QString shortName = ArxmlParser::collectChildText(el, "SHORT-NAME");
            QString val       = ArxmlParser::collectChildText(el, "VALUE");
            QStandardItem *tagCol = new QStandardItem(tag);
            QStandardItem *valCol = new QStandardItem(val);
            parent->setChild(item->row(), 1, tagCol);
            parent->setChild(item->row(), 2, valCol);

            // 递归子元素
            for (int i = 0; i < children.count(); i++) {
                QDomNode child = children.at(i);
                if (child.isElement())
                    walk(child.toElement(), item);
            }
        };

    QDomNodeList topChildren = root.childNodes();
    for (int i = 0; i < topChildren.count(); i++) {
        if (topChildren.at(i).isElement())
            walk(topChildren.at(i).toElement(), fileItem);
    }

    if (fileItem->rowCount() == 0) {
        m_treeModel->invisibleRootItem()->removeRow(fileItem->row());
    }
}

void MainWindow::onSearch()
{
    QString keyword = m_searchInput->text().trimmed();
    if (keyword.isEmpty()) return;

    QSqlQuery q = m_db->searchByShortName(keyword);
    QStringList foundUuids;
    while (q.next())
        foundUuids.append(q.value("uuid").toString());

    statusBar()->showMessage(
        QString("Found %1 matches for \"%2\"").arg(foundUuids.size()).arg(keyword));

    if (foundUuids.isEmpty()) return;

    // 高亮匹配项
    QSet<QString> matchSet(foundUuids.begin(), foundUuids.end());

    std::function<void(QStandardItem *)> highlight =
        [&](QStandardItem *item) {
            QString uuid = item->data(Qt::UserRole).toString();
            if (!uuid.isEmpty() && matchSet.contains(uuid)) {
                m_treeView->scrollTo(item->index());
                m_treeView->setCurrentIndex(item->index());
                QModelIndex idx = item->index();
                while (idx.isValid()) {
                    m_treeView->expand(idx);
                    idx = idx.parent();
                }
            }
            for (int r = 0; r < item->rowCount(); r++)
                highlight(item->child(r));
        };

    for (int r = 0; r < m_treeModel->rowCount(); r++)
        highlight(m_treeModel->item(r));
}