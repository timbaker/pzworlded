#ifndef GENERATELOTSDIALOG_H
#define GENERATELOTSDIALOG_H

#include <QDialog>

namespace Ui {
class GenerateLotsDialog;
}

class WorldDocument;

class GenerateLotsDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit GenerateLotsDialog(WorldDocument *worldDoc, QWidget *parent = 0);
    ~GenerateLotsDialog();

private slots:
    void exportBrowse();
    void spawnBrowse();
    void accept();
    void apply();

private:
    bool validate();

private:
    Ui::GenerateLotsDialog *ui;
    WorldDocument *mWorldDoc;
    QString mExportDir;
    QString mZombieSpawnMap;
};

#endif // GENERATELOTSDIALOG_H
