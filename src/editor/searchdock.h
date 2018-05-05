#ifndef SEARCHDOCK_H
#define SEARCHDOCK_H

#include <QDockWidget>
#include <QListWidget>

class Document;
class CellDocument;
class WorldDocument;

namespace Ui {
class SearchDock;
}

class SearchDock : public QDockWidget
{
    Q_OBJECT
public:
    explicit SearchDock(QWidget *parent = 0);

    void changeEvent(QEvent *e);
    void retranslateUi();

    void setDocument(Document *doc);
    void clearDocument();

private slots:
    void comboActivated1(int index);
    void comboActivated2(int index);
    void search();
    void listActivated(const QModelIndex& index);

private:
    Ui::SearchDock* ui;
    CellDocument* mCellDoc;
    WorldDocument* mWorldDoc;
};

#endif // SEARCHDOCK_H
