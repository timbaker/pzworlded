/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "textureeditdialog.h"
#include "ui_textureeditdialog.h"

#include "documentmanager.h"
#include "mainwindow.h"
#include "path.h"
#include "pathdocument.h"
#include "pathworld.h"
#include "texturebrowsedialog.h"
#include "worldchanger.h"

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
    mSynching(false),
    mDocument(0)
{
    ui->setupUi(this);

    setWindowFlags(windowFlags() | Qt::Tool);

#if 1
    ui->textureLabel->setStyleSheet(QLatin1String("QLabel { background-color : black }"));
#endif

    connect(ui->xScale, SIGNAL(valueChanged(double)), SLOT(xScaleChanged(double)));
    connect(ui->yScale, SIGNAL(valueChanged(double)), SLOT(yScaleChanged(double)));
    connect(ui->xFit, SIGNAL(clicked()), SLOT(xFit()));
    connect(ui->yFit, SIGNAL(clicked()), SLOT(yFit()));
    connect(ui->xShift, SIGNAL(valueChanged(int)), SLOT(xShiftChanged(int)));
    connect(ui->yShift, SIGNAL(valueChanged(int)), SLOT(yShiftChanged(int)));
    connect(ui->rotation, SIGNAL(valueChanged(double)), SLOT(rotationChanged(double)));
    connect(ui->browseTexture, SIGNAL(clicked()), SLOT(browseTexture()));
    connect(ui->alignPath, SIGNAL(toggled(bool)), SLOT(alignChanged()));
    connect(ui->stroke, SIGNAL(valueChanged(double)), SLOT(strokeChanged(double)));

    connect(DocumentManager::instance(), SIGNAL(currentDocumentChanged(Document*)),
            SLOT(documentChanged(Document*)));

    connect(qApp, SIGNAL(focusChanged(QWidget*,QWidget*)), SLOT(focusChanged(QWidget*,QWidget*)));

    documentChanged(DocumentManager::instance()->currentDocument());
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

        setValue(ui->xScale, mPath->texture().mScale.width());
        setValue(ui->yScale, mPath->texture().mScale.height());

        if (mPath->texture().mTexture) {
            ui->xShift->setRange(-mPath->texture().mTexture->mSize.width(),
                                 mPath->texture().mTexture->mSize.width());
            ui->yShift->setRange(-mPath->texture().mTexture->mSize.height(),
                                 mPath->texture().mTexture->mSize.height());
        }

        setValue(ui->xShift, mPath->texture().mTranslation.x());
        setValue(ui->yShift, mPath->texture().mTranslation.y());

        setValue(ui->rotation, mPath->texture().mRotation);

        if (WorldTexture *wtex = mPath->texture().mTexture) {
            if (!mPixmaps.contains(wtex->mFileName))
                mPixmaps[wtex->mFileName] = QPixmap::fromImage(QImage(wtex->mFileName).scaled(128, 128, Qt::KeepAspectRatio));
            ui->textureLabel->setPixmap(mPixmaps[wtex->mFileName]);
            ui->textureName->setText(wtex->mName);
        } else {
            ui->textureLabel->clear();
            ui->textureName->clear();
        }

        ui->alignWorld->setChecked(mPath->texture().mAlignWorld);
        ui->alignPath->setChecked(!mPath->texture().mAlignWorld);

        setValue(ui->stroke, mPath->strokeWidth());

        mSynching = false;
    } else {
        ui->xScale->setValue(1.0);
        ui->yScale->setValue(1.0);
        ui->xShift->setValue(0);
        ui->yShift->setValue(0);
        ui->rotation->setValue(0.0);
        ui->textureLabel->clear();
        ui->textureName->clear();
        ui->stroke->setValue(0.0);
    }

    setEnabled(mPath != 0);
}

void TextureEditDialog::documentChanged(Document *doc)
{
    setPath(0);

    if (mDocument)
        mDocument->changer()->disconnect(this);

    mDocument = doc ? doc->asPathDocument() : 0;

    if (mDocument) {
        connect(mDocument->changer(), SIGNAL(beforeRemovePathSignal(WorldPathLayer*,int,WorldPath*)),
                SLOT(beforeRemovePath(WorldPathLayer*,int,WorldPath*)));
        connect(mDocument->changer(), SIGNAL(afterSetPathTextureParamsSignal(WorldPath*,PathTexture)),
                SLOT(afterSetPathTextureParameters(WorldPath*,PathTexture)));
        connect(mDocument->changer(), SIGNAL(afterSetPathStrokeSignal(WorldPath*,qreal)),
                SLOT(afterSetPathStroke(WorldPath*,qreal)));
    }
}

void TextureEditDialog::xScaleChanged(double scale)
{
    if (!mPath || mSynching) return;
    scale = qBound(0.01, scale, 100.0);
    PathTexture pt = mPath->texture();
    pt.mScale.setWidth(scale);
    mDocument->changer()->beginUndoCommand(mDocument->undoStack());
    mDocument->changer()->doSetPathTextureParams(mPath, pt);
    mDocument->changer()->endUndoCommand();
}

void TextureEditDialog::yScaleChanged(double scale)
{
    if (!mPath || mSynching) return;
    scale = qBound(0.01, scale, 100.0);
    PathTexture pt = mPath->texture();
    pt.mScale.setHeight(scale);
    mDocument->changer()->beginUndoCommand(mDocument->undoStack());
    mDocument->changer()->doSetPathTextureParams(mPath, pt);
    mDocument->changer()->endUndoCommand();
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
    PathTexture pt = mPath->texture();
    pt.mTranslation.setX(shift);
    mDocument->changer()->beginUndoCommand(mDocument->undoStack());
    mDocument->changer()->doSetPathTextureParams(mPath, pt);
    mDocument->changer()->endUndoCommand();
}

void TextureEditDialog::yShiftChanged(int shift)
{
    if (!mPath || mSynching) return;
    if (shift == ui->yShift->minimum() || shift == ui->yShift->maximum())
        shift = 0;
    PathTexture pt = mPath->texture();
    pt.mTranslation.setY(shift);
    mDocument->changer()->beginUndoCommand(mDocument->undoStack());
    mDocument->changer()->doSetPathTextureParams(mPath, pt);
    mDocument->changer()->endUndoCommand();
}

void TextureEditDialog::rotationChanged(double rotation)
{
    if (!mPath || mSynching) return;
    PathTexture pt = mPath->texture();
    pt.mRotation = rotation;
    mDocument->changer()->beginUndoCommand(mDocument->undoStack());
    mDocument->changer()->doSetPathTextureParams(mPath, pt);
    mDocument->changer()->endUndoCommand();
}

void TextureEditDialog::browseTexture()
{
    if (!mPath) return;
    TextureBrowseDialog::instance()->setTextures(mPath->layer()->wlevel()->world()->textureList());
    TextureBrowseDialog::instance()->setCurrentTexture(mPath->texture().mTexture);
    if (TextureBrowseDialog::instance()->exec() != QDialog::Accepted)
        return;
    WorldTexture *tex = TextureBrowseDialog::instance()->chosen();

    PathTexture pt = mPath->texture();
    pt.mTexture = tex;
    mDocument->changer()->beginUndoCommand(mDocument->undoStack());
    mDocument->changer()->doSetPathTextureParams(mPath, pt);
    mDocument->changer()->endUndoCommand();

    if (!mPixmaps.contains(tex->mFileName))
        mPixmaps[tex->mFileName] = QPixmap::fromImage(QImage(tex->mFileName).scaled(128, 128, Qt::KeepAspectRatio));
    ui->textureLabel->setPixmap(mPixmaps[tex->mFileName]);
    ui->textureName->setText(tex->mName);
}

void TextureEditDialog::alignChanged()
{
    if (!mPath || mSynching) return;
    PathTexture pt = mPath->texture();
    pt.mAlignWorld = ui->alignWorld->isChecked();
    mDocument->changer()->beginUndoCommand(mDocument->undoStack());
    mDocument->changer()->doSetPathTextureParams(mPath, pt);
    mDocument->changer()->endUndoCommand();
}

void TextureEditDialog::strokeChanged(double thickness)
{
    if (!mPath || mSynching) return;
    thickness = qMax(0.0, thickness);
    mDocument->changer()->beginUndoCommand(mDocument->undoStack());
    mDocument->changer()->doSetPathStroke(mPath, thickness);
    mDocument->changer()->endUndoCommand();
}

void TextureEditDialog::beforeRemovePath(WorldPathLayer *layer, int index, WorldPath *path)
{
    Q_UNUSED(layer)
    Q_UNUSED(index)
    if (path == mPath)
        setPath(0);
}

void TextureEditDialog::afterSetPathTextureParameters(WorldPath *path, const PathTexture &params)
{
    if (path == mPath)
        setPath(mPath);
}

void TextureEditDialog::afterSetPathStroke(WorldPath *path, qreal oldStroke)
{
    Q_UNUSED(oldStroke)
    if (path == mPath)
        setPath(mPath);
}

void TextureEditDialog::focusChanged(QWidget *prev, QWidget *curr)
{
    if (mFocusDoubleSpinBox && prev == mFocusDoubleSpinBox) {
        mFocusDoubleSpinBox->setValue(mDelayedDoubleValue);
        mFocusDoubleSpinBox = 0;
    }
    if (mFocusSpinBox && prev == mFocusSpinBox) {
        mFocusSpinBox->setValue(mDelayedIntValue);
        mFocusSpinBox = 0;
    }
}

void TextureEditDialog::setValue(QDoubleSpinBox *w, double value)
{
    if (w->hasFocus()) {
        mDelayedDoubleValue = value;
        mFocusDoubleSpinBox = w;
        return;
    }
    w->setValue(value);
}

void TextureEditDialog::setValue(QSpinBox *w, int value)
{
    if (w->hasFocus()) {
        mDelayedIntValue = value;
        mFocusSpinBox = w;
        return;
    }
    w->setValue(value);
}

