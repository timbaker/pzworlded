#ifndef TEXTUREEDITDIALOG_H
#define TEXTUREEDITDIALOG_H

#include <QDialog>

class WorldPath;

namespace Ui {
class TextureEditDialog;
}

class TextureEditDialog : public QDialog
{
    Q_OBJECT
    
public:
    static TextureEditDialog *instance();
    static void deleteInstance();

    void setVisible(bool visible);

    void setPath(WorldPath *path);
    
private slots:
    void xScaleChanged(double scale);
    void yScaleChanged(double scale);

    void xShiftChanged(int shift);
    void yShiftChanged(int shift);

    void rotationChanged(double rotation);

signals:
    void ffsItChangedYo(WorldPath *path);

private:
    Q_DISABLE_COPY(TextureEditDialog)
    static TextureEditDialog *mInstance;
    explicit TextureEditDialog(QWidget *parent = 0);
    ~TextureEditDialog();

    Ui::TextureEditDialog *ui;

    WorldPath *mPath;
};

#endif // TEXTUREEDITDIALOG_H
