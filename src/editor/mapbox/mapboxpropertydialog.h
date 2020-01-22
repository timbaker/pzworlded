#ifndef MAPBOXPROPERTYDIALOG_H
#define MAPBOXPROPERTYDIALOG_H

#include <QDialog>

namespace Ui {
class MapboxPropertyDialog;
}

class MapBoxFeature;

class MapboxPropertyDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MapboxPropertyDialog(QWidget *parent = nullptr);
    ~MapboxPropertyDialog();

    void setProperty(const QString& key, const QString& value);

    QString getKey();
    QString getValue();

private:
    Ui::MapboxPropertyDialog *ui;
};

#endif // MAPBOXPROPERTYDIALOG_H
