#ifndef TMXTOBMPDIALOG_H
#define TMXTOBMPDIALOG_H

#include <QDialog>

class WorldDocument;

namespace Ui {
class TMXToBMPDialog;
}

class TMXToBMPDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TMXToBMPDialog(WorldDocument *worldDoc, QWidget *parent = 0);
    ~TMXToBMPDialog();

private slots:
    void buildingBrowse();
    void accept();
    void apply();

private:
    bool validate();
    void toSettings();

private:
    Ui::TMXToBMPDialog *ui;
    WorldDocument *mWorldDoc;
    QString mBldgFile;
};

#endif // TMXTOBMPDIALOG_H
