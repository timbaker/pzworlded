#include "textureeditdialog.h"
#include "ui_textureeditdialog.h"

#include "mainwindow.h"
#include "path.h"

TextureEditDialog *TextureEditDialog::mInstance = 0;

TextureEditDialog *TextureEditDialog::instance()
{
    if (!mInstance)
        mInstance = new TextureEditDialog(MainWindow::instance());
    return mInstance;
}

void TextureEditDialog::deleteInstance()
{
    delete mInstance;
}

void TextureEditDialog::setVisible(bool visible)
{
    QSettings settings;
    settings.beginGroup(QLatin1String("TextureEditDialog"));

    if (visible) {
        QByteArray geom = settings.value(QLatin1String("geometry")).toByteArray();
        if (!geom.isEmpty())
            restoreGeometry(geom);
    }

    QDialog::setVisible(visible);

    if (!visible) {
        settings.setValue(QLatin1String("geometry"), saveGeometry());
    }
    settings.endGroup();
}

TextureEditDialog::TextureEditDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TextureEditDialog),
    mPath(0)
{
    ui->setupUi(this);

    setWindowFlags(windowFlags() | Qt::Tool);

    connect(ui->xScale, SIGNAL(valueChanged(double)), SLOT(xScaleChanged(double)));
    connect(ui->yScale, SIGNAL(valueChanged(double)), SLOT(yScaleChanged(double)));

    connect(ui->xShift, SIGNAL(valueChanged(int)), SLOT(xShiftChanged(int)));
    connect(ui->yShift, SIGNAL(valueChanged(int)), SLOT(yShiftChanged(int)));

    connect(ui->rotation, SIGNAL(valueChanged(double)), SLOT(rotationChanged(double)));
}

TextureEditDialog::~TextureEditDialog()
{
    delete ui;
}

void TextureEditDialog::setPath(WorldPath *path)
{
    mPath = path;

    if (mPath) {
        ui->xScale->setValue(mPath->mTexture.mScale.width());
        ui->yScale->setValue(mPath->mTexture.mScale.height());

        ui->xShift->setValue(mPath->mTexture.mTranslation.x());
        ui->yShift->setValue(mPath->mTexture.mTranslation.y());

        ui->rotation->setValue(mPath->mTexture.mRotation);
    }
}

void TextureEditDialog::xScaleChanged(double scale)
{
    if (!mPath) return;
    scale = qBound(0.01, scale, 100.0);
    mPath->mTexture.mScale.setWidth(scale);
    emit ffsItChangedYo(mPath);
}

void TextureEditDialog::yScaleChanged(double scale)
{
    if (!mPath) return;
    scale = qBound(0.01, scale, 100.0);
    mPath->mTexture.mScale.setHeight(scale);
    emit ffsItChangedYo(mPath);
}

void TextureEditDialog::xShiftChanged(int shift)
{
    if (!mPath) return;
    mPath->mTexture.mTranslation.setX(shift);
    emit ffsItChangedYo(mPath);
}

void TextureEditDialog::yShiftChanged(int shift)
{
    if (!mPath) return;
    mPath->mTexture.mTranslation.setY(shift);
    emit ffsItChangedYo(mPath);
}

void TextureEditDialog::rotationChanged(double rotation)
{
    if (!mPath) return;
    mPath->mTexture.mRotation = rotation;
    emit ffsItChangedYo(mPath);
}

