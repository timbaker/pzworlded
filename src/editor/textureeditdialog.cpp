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

    connect(ui->xFit, SIGNAL(clicked()), SLOT(xFit()));
    connect(ui->yFit, SIGNAL(clicked()), SLOT(yFit()));

    connect(ui->xShift, SIGNAL(valueChanged(int)), SLOT(xShiftChanged(int)));
    connect(ui->yShift, SIGNAL(valueChanged(int)), SLOT(yShiftChanged(int)));

    connect(ui->rotation, SIGNAL(valueChanged(double)), SLOT(rotationChanged(double)));

    connect(ui->textureCombo, SIGNAL(activated(int)), SLOT(textureComboActivated(int)));

    connect(ui->alignPath, SIGNAL(toggled(bool)), SLOT(alignChanged()));

    connect(ui->stroke, SIGNAL(valueChanged(double)), SLOT(strokeChanged(double)));
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

        ui->xScale->setValue(mPath->texture().mScale.width());
        ui->yScale->setValue(mPath->texture().mScale.height());

        if (mPath->texture().mTexture) {
            ui->xShift->setRange(-mPath->texture().mTexture->mSize.width(),
                                 mPath->texture().mTexture->mSize.width());
            ui->yShift->setRange(-mPath->texture().mTexture->mSize.height(),
                                 mPath->texture().mTexture->mSize.height());
        }

        ui->xShift->setValue(mPath->texture().mTranslation.x());
        ui->yShift->setValue(mPath->texture().mTranslation.y());

        ui->rotation->setValue(mPath->texture().mRotation);

        if (ui->textureCombo->count() == 0) {
            ui->textureCombo->addItem(QLatin1String("<none>"));
            foreach (WorldTexture *wtex, path->layer()->wlevel()->world()->textureList()) {
                ui->textureCombo->addItem(wtex->mName);
            }
        }

        int index = 0;
        if (mPath->texture().mTexture)
            index = path->layer()->wlevel()->world()->textureList().indexOf(mPath->texture().mTexture) + 1;
        ui->textureCombo->setCurrentIndex(index);

        ui->alignWorld->setChecked(mPath->texture().mAlignWorld);
        ui->alignPath->setChecked(!mPath->texture().mAlignWorld);

        ui->stroke->setValue(mPath->strokeWidth());

        mSynching = false;
    }
}

void TextureEditDialog::xScaleChanged(double scale)
{
    if (!mPath || mSynching) return;
    scale = qBound(0.01, scale, 100.0);
    mPath->texture().mScale.setWidth(scale);
    emit ffsItChangedYo(mPath);
}

void TextureEditDialog::yScaleChanged(double scale)
{
    if (!mPath || mSynching) return;
    scale = qBound(0.01, scale, 100.0);
    mPath->texture().mScale.setHeight(scale);
    emit ffsItChangedYo(mPath);
}

void TextureEditDialog::xFit()
{
    if (!mPath || !mPath->texture().mTexture) return;
    QRectF pathRect = mPath->bounds();
    if (pathRect.width() <= 0) return;
    qreal scale = (pathRect.width() * 64.0) / mPath->texture().mTexture->mSize.width();
#if 1
    ui->xScale->setValue(scale);
#else
    mPath->texture().mScale.setWidth(scale);
    emit ffsItChangedYo(mPath);
#endif
}

void TextureEditDialog::yFit()
{
    if (!mPath || !mPath->texture().mTexture) return;
    QRectF pathRect = mPath->bounds();
    if (pathRect.height() <= 0) return;
    qreal scale = (pathRect.height() * 64.0) / mPath->texture().mTexture->mSize.height();
#if 1
    ui->yScale->setValue(scale);
#else
    mPath->texture().mScale.setHeight(scale);
    emit ffsItChangedYo(mPath);
#endif
}

void TextureEditDialog::xShiftChanged(int shift)
{
    if (!mPath || mSynching) return;
    if (shift == ui->xShift->minimum() || shift == ui->xShift->maximum())
        shift = 0;
    mPath->texture().mTranslation.setX(shift);
    emit ffsItChangedYo(mPath);
}

void TextureEditDialog::yShiftChanged(int shift)
{
    if (!mPath || mSynching) return;
    if (shift == ui->yShift->minimum() || shift == ui->yShift->maximum())
        shift = 0;
    mPath->texture().mTranslation.setY(shift);
    emit ffsItChangedYo(mPath);
}

void TextureEditDialog::rotationChanged(double rotation)
{
    if (!mPath || mSynching) return;
    mPath->texture().mRotation = rotation;
    emit ffsItChangedYo(mPath);
}

void TextureEditDialog::textureComboActivated(int index)
{
    if (!mPath || mSynching) return;
    if (index == 0)
        mPath->texture().mTexture = 0;
    else
        mPath->texture().mTexture = mPath->layer()->wlevel()->world()->textureList().at(index - 1);
    emit ffsItChangedYo(mPath);
}

void TextureEditDialog::alignChanged()
{
    if (!mPath || mSynching) return;
    mPath->texture().mAlignWorld = ui->alignWorld->isChecked();
    emit ffsItChangedYo(mPath);
}

void TextureEditDialog::strokeChanged(double thickness)
{
    if (!mPath || mSynching) return;
    thickness = qMax(0.0, thickness);
    mPath->setStrokeWidth(thickness);
    emit ffsItChangedYo(mPath);
}

