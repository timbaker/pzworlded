#include "textureeditdialog.h"
#include "ui_textureeditdialog.h"

#include "mainwindow.h"
#include "path.h"
#include "pathworld.h"

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

TextureEditDialog::TextureEditDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TextureEditDialog),
    mPath(0),
    mSynching(false)
{
    ui->setupUi(this);

    setWindowFlags(windowFlags() | Qt::Tool);

    connect(ui->xScale, SIGNAL(valueChanged(double)), SLOT(xScaleChanged(double)));
    connect(ui->yScale, SIGNAL(valueChanged(double)), SLOT(yScaleChanged(double)));

    connect(ui->xShift, SIGNAL(valueChanged(int)), SLOT(xShiftChanged(int)));
    connect(ui->yShift, SIGNAL(valueChanged(int)), SLOT(yShiftChanged(int)));

    connect(ui->rotation, SIGNAL(valueChanged(double)), SLOT(rotationChanged(double)));

    connect(ui->textureCombo, SIGNAL(activated(int)), SLOT(textureComboActivated(int)));

    connect(ui->alignPath, SIGNAL(toggled(bool)), SLOT(alignChanged()));
}

TextureEditDialog::~TextureEditDialog()
{
    delete ui;
}

void TextureEditDialog::setVisible(bool visible)
{
    if (visible == isVisible()) return;

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

void TextureEditDialog::setPath(WorldPath *path)
{
    mPath = path;

    if (mPath) {
        mSynching = true;

        ui->xScale->setValue(mPath->mTexture.mScale.width());
        ui->yScale->setValue(mPath->mTexture.mScale.height());

        if (mPath->mTexture.mTexture) {
            ui->xShift->setRange(-mPath->mTexture.mTexture->mSize.width(),
                                 mPath->mTexture.mTexture->mSize.width());
            ui->yShift->setRange(-mPath->mTexture.mTexture->mSize.height(),
                                 mPath->mTexture.mTexture->mSize.height());
        }

        ui->xShift->setValue(mPath->mTexture.mTranslation.x());
        ui->yShift->setValue(mPath->mTexture.mTranslation.y());

        ui->rotation->setValue(mPath->mTexture.mRotation);

        if (ui->textureCombo->count() == 0) {
            ui->textureCombo->addItem(QLatin1String("<none>"));
            foreach (WorldTexture *wtex, path->mLayer->wlevel()->world()->textureList()) {
                ui->textureCombo->addItem(wtex->mName);
            }
        }

        int index = 0;
        if (mPath->mTexture.mTexture)
            index = path->mLayer->wlevel()->world()->textureList().indexOf(mPath->mTexture.mTexture) + 1;
        ui->textureCombo->setCurrentIndex(index);

        ui->alignWorld->setChecked(mPath->mTexture.mAlignWorld);
        ui->alignPath->setChecked(!mPath->mTexture.mAlignWorld);

        mSynching = false;
    }
}

void TextureEditDialog::xScaleChanged(double scale)
{
    if (!mPath || mSynching) return;
    scale = qBound(0.01, scale, 100.0);
    mPath->mTexture.mScale.setWidth(scale);
    emit ffsItChangedYo(mPath);
}

void TextureEditDialog::yScaleChanged(double scale)
{
    if (!mPath || mSynching) return;
    scale = qBound(0.01, scale, 100.0);
    mPath->mTexture.mScale.setHeight(scale);
    emit ffsItChangedYo(mPath);
}

void TextureEditDialog::xShiftChanged(int shift)
{
    if (!mPath || mSynching) return;
    if (shift == ui->xShift->minimum() || shift == ui->xShift->maximum())
        shift = 0;
    mPath->mTexture.mTranslation.setX(shift);
    emit ffsItChangedYo(mPath);
}

void TextureEditDialog::yShiftChanged(int shift)
{
    if (!mPath || mSynching) return;
    if (shift == ui->yShift->minimum() || shift == ui->yShift->maximum())
        shift = 0;
    mPath->mTexture.mTranslation.setY(shift);
    emit ffsItChangedYo(mPath);
}

void TextureEditDialog::rotationChanged(double rotation)
{
    if (!mPath || mSynching) return;
    mPath->mTexture.mRotation = rotation;
    emit ffsItChangedYo(mPath);
}

void TextureEditDialog::textureComboActivated(int index)
{
    if (!mPath || mSynching) return;
    if (index == 0)
        mPath->mTexture.mTexture = 0;
    else
        mPath->mTexture.mTexture = mPath->mLayer->wlevel()->world()->textureList().at(index - 1);
    emit ffsItChangedYo(mPath);
}

void TextureEditDialog::alignChanged()
{
    if (!mPath || mSynching) return;
    mPath->mTexture.mAlignWorld = ui->alignWorld->isChecked();
    emit ffsItChangedYo(mPath);
}

