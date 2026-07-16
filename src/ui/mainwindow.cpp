#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "../arxmlparser.h"
#include "../arxmldatabase.h"
#include "../arxmlutils.h"
#include "arxmlcolors.h"

#include <QDebug>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QShortcut>
#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QStandardItemModel>
#include <QTreeView>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_db(new ArxmlDatabase)
    , m_titleBar(nullptr)
{
    ui->setupUi(this);

    // 隐藏原生标题栏
    setWindowFlags(Qt::FramelessWindowHint);
    ui->menubar->setVisible(false);

    // 清除 centralwidget 布局边距，让标题栏紧贴上边沿
    QVBoxLayout *mainLayout = qobject_cast<QVBoxLayout *>(ui->centralwidget->layout());
    if (mainLayout)
        mainLayout->setContentsMargins(0, 0, 0, 0);

    setupTitleBar();
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
    m_treeModel->setHorizontalHeaderLabels({"Name", "Tag"});
    m_treeView->setModel(m_treeModel);
    m_treeView->setAlternatingRowColors(true);
    m_treeView->setAnimated(true);
    m_treeView->setSortingEnabled(true);
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_treeView->header()->setStretchLastSection(true);

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

    // 双击切换完整/简略路径
    connect(m_treeView, &QTreeView::doubleClicked, this, [this](const QModelIndex &idx) {
        QString full = idx.data(Qt::UserRole + 4).toString();
        if (full.isEmpty() || !full.contains("/")) return;

        QString cur = idx.data(Qt::DisplayRole).toString();
        m_treeModel->setData(idx,
            cur.startsWith("...") ? full : (".../" + full.section('/', -1)),
            Qt::DisplayRole);
    });

    // 右键菜单
    connect(m_treeView, &QTreeView::customContextMenuRequested, this, &MainWindow::onTreeContextMenu);

    // 创建独立 QAction 用于快捷键
    QAction *backAction  = new QAction("← Back", this);
    QAction *forwardAction = new QAction("Forward →", this);
    backAction->setShortcut(QKeySequence("Alt+Left"));
    forwardAction->setShortcut(QKeySequence("Alt+Right"));
    connect(backAction, &QAction::triggered, this, &MainWindow::onBack);
    connect(forwardAction, &QAction::triggered, this, &MainWindow::onForward);
    addAction(backAction);
    addAction(forwardAction);
}

MainWindow::~MainWindow()
{
    qDeleteAll(m_parsers);
    delete m_db;
    delete ui;
}

void MainWindow::setupTitleBar()
{
    m_titleBar = new QWidget(this);
    m_titleBar->setObjectName("titleBar");
    m_titleBar->setFixedHeight(36);

    // Project 菜单按钮
    QPushButton *projectBtn = new QPushButton("☰ Project");
    projectBtn->setObjectName("titleProjectBtn");

    // Navigate 菜单按钮
    QPushButton *navBtn = new QPushButton("Navigate");
    navBtn->setObjectName("titleNavBtn");

    QPushButton *minBtn = new QPushButton("─");
    minBtn->setObjectName("titleMinBtn");
    QPushButton *maxBtn = new QPushButton("□");
    maxBtn->setObjectName("titleMaxBtn");
    QPushButton *closeBtn = new QPushButton("✕");
    closeBtn->setObjectName("titleCloseBtn");

    QHBoxLayout *layout = new QHBoxLayout(m_titleBar);
    layout->setContentsMargins(8, 0, 4, 0);
    layout->setSpacing(4);
    layout->addWidget(projectBtn);
    layout->addWidget(navBtn);
    layout->addStretch();
    layout->addWidget(minBtn);
    layout->addWidget(maxBtn);
    layout->addWidget(closeBtn);

    // 插入到 centralWidget 上方
    QVBoxLayout *mainLayout = qobject_cast<QVBoxLayout *>(ui->centralwidget->layout());
    if (mainLayout)
        mainLayout->insertWidget(0, m_titleBar);

    // Project 菜单
    QMenu *projectMenu = new QMenu(this);
    projectMenu->addAction(ui->actionOpen);
    projectMenu->addAction(ui->actionRefresh);
    projectMenu->addSeparator();
    projectMenu->addAction(ui->actionExit);
    connect(projectBtn, &QPushButton::clicked, this, [projectMenu, projectBtn]() {
        projectMenu->exec(projectBtn->mapToGlobal(QPoint(0, projectBtn->height())));
    });

    // Navigate 菜单
    QMenu *navMenu = new QMenu(this);
    QAction *backAction2  = navMenu->addAction("← Back");
    QAction *forwardAction2 = navMenu->addAction("Forward →");
    backAction2->setShortcut(QKeySequence("Alt+Left"));
    forwardAction2->setShortcut(QKeySequence("Alt+Right"));
    connect(backAction2, &QAction::triggered, this, &MainWindow::onBack);
    connect(forwardAction2, &QAction::triggered, this, &MainWindow::onForward);
    connect(navBtn, &QPushButton::clicked, this, [navMenu, navBtn]() {
        navMenu->exec(navBtn->mapToGlobal(QPoint(0, navBtn->height())));
    });

    // 窗口控制
    connect(minBtn, &QPushButton::clicked, this, &MainWindow::showMinimized);
    connect(maxBtn, &QPushButton::clicked, this, [this]() {
        isMaximized() ? showNormal() : showMaximized();
    });
    connect(closeBtn, &QPushButton::clicked, this, &MainWindow::close);
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (m_titleBar && m_titleBar->rect().contains(event->pos())) {
        m_dragPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
    }
    QMainWindow::mousePressEvent(event);
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton && !m_dragPos.isNull()) {
        move(event->globalPosition().toPoint() - m_dragPos);
    }
    QMainWindow::mouseMoveEvent(event);
}

void MainWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (m_titleBar && m_titleBar->rect().contains(event->pos())) {
        isMaximized() ? showNormal() : showMaximized();
    }
    QMainWindow::mouseDoubleClickEvent(event);
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
    m_backStack.clear();
    m_forwardStack.clear();
    m_currentFilePath.clear();

    QString dbPath = dirPath + "/.index.db";
    if (!m_db->init(dbPath)) {
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

    int idx = 0;
    for (const QString &filePath : files) {
        idx++;
        statusBar()->showMessage(
            QString("Indexing %1/%2: %3").arg(idx).arg(files.size()).arg(QFileInfo(filePath).fileName()));
        qApp->processEvents();

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

    m_currentFilePath = filePath;

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

    QDomNodeList topChildren = root.childNodes();
    for (int i = 0; i < topChildren.count(); i++) {
        if (topChildren.at(i).isElement())
            walkDomNode(topChildren.at(i).toElement(), fileItem, "", filePath);
    }

    if (fileItem->rowCount() == 0)
        m_treeModel->invisibleRootItem()->removeRow(fileItem->row());
}

void MainWindow::walkDomNode(const QDomElement &el, QStandardItem *parent,
                              const QString &pathPrefix, const QString &filePath)
{
    QString tag  = el.tagName();
    QString uuid = el.attribute("UUID");

    QDomNodeList children = el.childNodes();
    int elemCount = 0;
    for (int i = 0; i < children.count(); i++) {
        if (children.at(i).isElement())
            elemCount++;
    }

    QString shortName = ArxmlParser::collectChildText(el, "SHORT-NAME");
    QString nameText = el.text().trimmed();

    // 纯容器：无 UUID 且无 SHORT-NAME → 跳过但继续递归
    if (uuid.isEmpty() && shortName.isEmpty() && elemCount > 0) {
        for (int i = 0; i < children.count(); i++) {
            QDomNode child = children.at(i);
            if (child.isElement())
                walkDomNode(child.toElement(), parent, pathPrefix, filePath);
        }
        return;
    }

    // 构建当前节点路径（无 SHORT-NAME 则路径不变）
    QString curPath;
    if(shortName.isEmpty()){
        curPath = pathPrefix;
    }else{
        curPath = pathPrefix + "/" + shortName;
    }

    QString display;
    QString fullText;

    if (tag == "SHORT-NAME")
        display = "◆";
    else if (elemCount == 0) {
        QString text = el.text().trimmed();
        if (text.contains("/")) {
            display  = ".../" + text.section('/', -1);
            fullText = text;
        } else {
            display  = text.isEmpty() ? tag : text;
            fullText = text;
        }
    } else {
        display = shortName.isEmpty() ? tag : shortName;
    }

    QStandardItem *item = new QStandardItem(display);
    item->setData(uuid, Qt::UserRole);
    item->setData(tag,  Qt::UserRole + 1);
    item->setData(filePath, Qt::UserRole + 2);
    item->setData(curPath, Qt::UserRole + 3);
    item->setData(fullText, Qt::UserRole + 4);
    item->setToolTip(curPath);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);

    if (tag == "SHORT-NAME")
        item->setForeground(QColor(ARXML_COLOR_ANCHOR));
    else if (elemCount > 0)
        item->setForeground(QColor(ARXML_COLOR_CONTAINER));
    else if (!uuid.isEmpty())
        item->setForeground(QColor(ARXML_COLOR_UUID_LEAF));
    else if (tag.contains("REF"))
        item->setForeground(QColor(ARXML_COLOR_REFERENCE));
    else
        item->setForeground(QColor(ARXML_COLOR_PLAIN_LEAF));

    parent->appendRow(item);

    QStandardItem *tagCol = new QStandardItem(tag);
    parent->setChild(item->row(), 1, tagCol);

    for (int i = 0; i < children.count(); i++) {
        QDomNode child = children.at(i);
        if (child.isElement())
            walkDomNode(child.toElement(), item, curPath, filePath);
    }
}

void MainWindow::onSearch()
{
    QString keyword = m_searchInput->text().trimmed();

    // file: 指令：过滤左侧文件列表
    if (keyword.startsWith("file:")) {
        QString pattern = keyword.mid(5).trimmed();
        if (pattern.isEmpty()) {
            // 恢复全部显示
            for (int r = 0; r < m_fileModel->rowCount(); r++)
                m_fileTree->setRowHidden(r, QModelIndex(), false);
            statusBar()->showMessage("Showing all files");
            return;
        }
        int visible = 0;
        for (int r = 0; r < m_fileModel->rowCount(); r++) {
            bool match = m_fileModel->item(r)->text().contains(pattern, Qt::CaseInsensitive);
            m_fileTree->setRowHidden(r, QModelIndex(), !match);
            if (match) visible++;
        }
        statusBar()->showMessage(QString("Filter: file contains \"%1\" (%2 files)").arg(pattern).arg(visible));
        return;
    }

    if (keyword.isEmpty()) {
        // 空搜索 → 恢复全部文件 + 清状态
        for (int r = 0; r < m_fileModel->rowCount(); r++)
            m_fileTree->setRowHidden(r, QModelIndex(), false);
        statusBar()->showMessage("Ready");
        return;
    }

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

void MainWindow::onTreeContextMenu(const QPoint &pos)
{
    QModelIndex idx = m_treeView->indexAt(pos);
    if (!idx.isValid()) return;

    QString fullText = idx.data(Qt::UserRole + 4).toString();
    bool isPath = fullText.contains("/");

    QMenu menu;
    QAction *navAction = menu.addAction("Navigate to Reference");
    navAction->setEnabled(isPath);

    QAction *copyAction = menu.addAction("Copy Path");
    copyAction->setEnabled(true);

    QAction *chosen = menu.exec(m_treeView->viewport()->mapToGlobal(pos));
    if (chosen == navAction && isPath) {
        // 调试：查询 DB 并显示结果
        QSqlQuery q = m_db->searchByFullPath(fullText, "%");
        if (!q.next()) {
            QMessageBox::information(this, "DB Query",
                                     "No match found for:\n" + fullText);
            return;
        }

        QStringList fileList;
        struct PathInfo { QString filePath; QString fullPath; };
        QList<PathInfo> pathInfos;
        do {
            if (q.value("short_name").toString() == "justPath") {
                QString fp = q.value("file_path").toString();
                QString path = q.value("full_path").toString();
                fileList << QFileInfo(fp).fileName() + "  →  " + path;
                pathInfos.append({fp, path});
            }
        } while (q.next());

        if (fileList.isEmpty()) {
            QMessageBox::information(this, "Navigate", "No match found.");
            return;
        }

        // 选目标
        QString chosen;
        int chosenIdx = 0;
        if (fileList.size() == 1) {
            chosen = fileList[0];
        } else {
            bool ok;
            chosen = QInputDialog::getItem(this, "Navigate to Reference",
                                           "Select target:", fileList, 0, false, &ok);
            if (!ok) return;
        }
        chosenIdx = fileList.indexOf(chosen);
        if (chosenIdx < 0) return;

        QString targetFile   = pathInfos[chosenIdx].filePath;
        QString targetPath   = pathInfos[chosenIdx].fullPath;

        // 记录当前位置用于回退
        recordCurrentPosition();

        // 切换到目标文件
        for (int r = 0; r < m_fileModel->rowCount(); r++) {
            if (m_fileModel->item(r)->data(Qt::UserRole).toString() == targetFile) {
                m_fileTree->setCurrentIndex(m_fileModel->index(r, 0));
                onFileClicked(m_fileModel->index(r, 0));
                break;
            }
        }

        // 在右侧树中匹配路径
        std::function<QModelIndex(QStandardItem *, const QString &)> findByPath =
            [&](QStandardItem *item, const QString &path) -> QModelIndex {
                if (item->data(Qt::UserRole + 3).toString() == path)
                    return item->index();
                for (int r = 0; r < item->rowCount(); r++) {
                    QModelIndex found = findByPath(item->child(r), path);
                    if (found.isValid()) return found;
                }
                return QModelIndex();
            };

        QModelIndex targetIdx;
        for (int r = 0; r < m_treeModel->rowCount(); r++) {
            targetIdx = findByPath(m_treeModel->item(r), targetPath);
            if (targetIdx.isValid()) break;
        }

        if (targetIdx.isValid()) {
            m_treeView->scrollTo(targetIdx);
            m_treeView->setCurrentIndex(targetIdx);
            statusBar()->showMessage(
                QString("Navigated to %1").arg(QFileInfo(targetFile).fileName()));
        }
    } else if (chosen == copyAction) {
        QString curPath = idx.data(Qt::UserRole + 3).toString();
        QApplication::clipboard()->setText(curPath);
    }
}

void MainWindow::recordCurrentPosition()
{
    QModelIndex curIdx = m_treeView->currentIndex();
    QString curPath = curIdx.isValid() ? curIdx.data(Qt::UserRole + 3).toString() : "";
    m_backStack.append({m_currentFilePath, curPath});
    m_forwardStack.clear();  // 新跳转覆盖前进历史
}

void MainWindow::navigateToPosition(const NavPosition &pos)
{
    // 切换到文件
    for (int r = 0; r < m_fileModel->rowCount(); r++) {
        if (m_fileModel->item(r)->data(Qt::UserRole).toString() == pos.filePath) {
            m_fileTree->setCurrentIndex(m_fileModel->index(r, 0));
            onFileClicked(m_fileModel->index(r, 0));
            break;
        }
    }

    // 定位路径
    if (!pos.curPath.isEmpty()) {
        std::function<QModelIndex(QStandardItem *, const QString &)> findByPath =
            [&](QStandardItem *item, const QString &path) -> QModelIndex {
                if (item->data(Qt::UserRole + 3).toString() == path)
                    return item->index();
                for (int r = 0; r < item->rowCount(); r++) {
                    QModelIndex found = findByPath(item->child(r), path);
                    if (found.isValid()) return found;
                }
                return QModelIndex();
            };

        QModelIndex targetIdx;
        for (int r = 0; r < m_treeModel->rowCount(); r++) {
            targetIdx = findByPath(m_treeModel->item(r), pos.curPath);
            if (targetIdx.isValid()) break;
        }

        if (targetIdx.isValid()) {
            m_treeView->scrollTo(targetIdx);
            m_treeView->setCurrentIndex(targetIdx);
        }
    }

    m_currentFilePath = pos.filePath;
}

void MainWindow::onBack()
{
    if (m_backStack.isEmpty()) return;

    // 当前位置入前进栈
    QModelIndex curIdx = m_treeView->currentIndex();
    QString curPath = curIdx.isValid() ? curIdx.data(Qt::UserRole + 3).toString() : "";
    m_forwardStack.append({m_currentFilePath, curPath});

    NavPosition pos = m_backStack.takeLast();
    navigateToPosition(pos);
    statusBar()->showMessage(QString("Back to %1").arg(QFileInfo(pos.filePath).fileName()));
}

void MainWindow::onForward()
{
    if (m_forwardStack.isEmpty()) return;

    // 当前位置入后退栈
    QModelIndex curIdx = m_treeView->currentIndex();
    QString curPath = curIdx.isValid() ? curIdx.data(Qt::UserRole + 3).toString() : "";
    m_backStack.append({m_currentFilePath, curPath});

    NavPosition pos = m_forwardStack.takeLast();
    navigateToPosition(pos);
    statusBar()->showMessage(QString("Forward to %1").arg(QFileInfo(pos.filePath).fileName()));
}