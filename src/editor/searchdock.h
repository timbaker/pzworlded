#ifndef SEARCHDOCK_H
#define SEARCHDOCK_H

#include <QDockWidget>
#include <QListWidget>

class Document;
class CellDocument;
class WorldCell;
class WorldDocument;

namespace Ui {
class SearchDock;
}

class SearchResults
{
public:
    void reset() {
        objectType.clear();
        cells.clear();
        selectedCell = nullptr;
    }

    QString objectType;
    QList<WorldCell*> cells;
    WorldCell* selectedCell;
};

class SearchDock : public QDockWidget
{
    Q_OBJECT
public:
    explicit SearchDock(QWidget *parent = 0);

    void changeEvent(QEvent *e);
    void retranslateUi();

    void setDocument(Document *doc);
    void clearDocument();
    WorldDocument* worldDocument();

private slots:
    void comboActivated1(int index);
    void comboActivated2(int index);
    void search();
    void setList(SearchResults* results);
    void listSelectionChanged();
    void listActivated(const QModelIndex& index);
    void documentAboutToClose(int index, Document *doc);

private:
    Ui::SearchDock* ui;
    CellDocument* mCellDoc;
    WorldDocument* mWorldDoc;
    QMap<Document*,SearchResults*> mResults;
    bool mSynching;
};

#endif // SEARCHDOCK_H
