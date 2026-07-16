#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>
#include <QPoint>

class ArxmlParser;
class ArxmlDatabase;
class QDomElement;
class QStandardItem;
class QTreeView;
class QLineEdit;
class QPushButton;
class QStandardItemModel;

namespace Ui { class MainWindow; }

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void openDirectory(const QString &dirPath);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private slots:
    void onSearch();
    void onOpenFolder();
    void onFileClicked(const QModelIndex &index);
    void onTreeContextMenu(const QPoint &pos);
    void onBack();
    void onForward();

private:
    struct NavPosition {
        QString filePath;
        QString curPath;
    };

    void buildTreeFromParser(ArxmlParser &parser, const QString &filePath);
    void walkDomNode(const QDomElement &el, QStandardItem *parent,
                     const QString &pathPrefix, const QString &filePath);
    void setupTitleBar();
    void loadStyleSheet();
    void recordCurrentPosition();
    void navigateToPosition(const NavPosition &pos);

    Ui::MainWindow *ui;
    QString m_currentDir;
    QString m_currentFilePath;

    // 自定义标题栏
    QWidget *m_titleBar;
    QPoint m_dragPos;

    QTreeView *m_fileTree;
    QTreeView *m_treeView;
    QLineEdit *m_searchInput;
    QStandardItemModel *m_fileModel;
    QStandardItemModel *m_treeModel;

    QList<NavPosition> m_backStack;
    QList<NavPosition> m_forwardStack;

    ArxmlDatabase *m_db;
    QMap<QString, ArxmlParser *> m_parsers;
};

#endif // MAINWINDOW_H