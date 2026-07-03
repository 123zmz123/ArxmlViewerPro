#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>

class ArxmlParser;
class ArxmlDatabase;
class QDomElement;
class QStandardItem;
class QTreeView;
class QLineEdit;
class QStandardItemModel;

namespace Ui { class MainWindow; }

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void openDirectory(const QString &dirPath);

private slots:
    void onSearch();
    void onOpenFolder();
    void onFileClicked(const QModelIndex &index);

private:
    void buildTreeFromParser(ArxmlParser &parser, const QString &filePath);
    void walkDomNode(const QDomElement &el, QStandardItem *parent,
                     const QString &pathPrefix, const QString &filePath);
    void loadStyleSheet();

    Ui::MainWindow *ui;
    QString m_currentDir;

    QTreeView *m_fileTree;
    QTreeView *m_treeView;
    QLineEdit *m_searchInput;
    QStandardItemModel *m_fileModel;
    QStandardItemModel *m_treeModel;

    ArxmlDatabase *m_db;
    QMap<QString, ArxmlParser *> m_parsers;
};

#endif // MAINWINDOW_H