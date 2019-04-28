/*
 * zoomable.cpp
 * Copyright 2009-2010, Thorbj�rn Lindeijer <thorbjorn@lindeijer.nl>
 *
 * This file is part of Tiled.
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

#include "zoomable.h"

#include <QComboBox>
#include <QLineEdit>
#include <QValidator>

#include <cmath>

// NB: These numbers must be non-fractional when multiplied
// by 100, since the editiable combobox does not accept
// decimal points.
static const qreal zoomFactors[] = {
    0.06,
    0.12,
    0.25,
    0.33,
    0.5,
    0.75,
    1.0,
    1.5,
    2.0,
    3.0,
    4.0
};
const int zoomFactorCount = sizeof(zoomFactors) / sizeof(zoomFactors[0]);


static QString scaleToString(qreal scale)
{
    return QString(QLatin1String("%1 %")).arg(int(scale * 100));
}


Zoomable::Zoomable(QObject *parent)
    : QObject(parent)
    , mScale(1)
    , mComboBox(0)
    , mComboRegExp(QLatin1String("^\\s*(\\d+)\\s*%?\\s*$"))
    , mComboValidator(0)
{
    for (int i = 0; i < zoomFactorCount; i++)
        mZoomFactors << ::zoomFactors[i];
}

void Zoomable::setScale(qreal scale)
{
    if (scale == mScale)
        return;

    mScale = scale;

    syncComboBox();

    emit scaleChanged(mScale);
}

bool Zoomable::canZoomIn() const
{
    return mScale < mZoomFactors.last();
}

bool Zoomable::canZoomOut() const
{
    return mScale > mZoomFactors.first();
}

void Zoomable::handleWheelDelta(int delta)
{
    if (delta <= -120) {
        zoomOut();
    } else if (delta >= 120) {
        zoomIn();
    } else {
        // We're dealing with a finer-resolution mouse. Allow it to have finer
        // control over the zoom level.
        qreal factor = 1 + 0.3 * qAbs(qreal(delta) / 8 / 15);
        if (delta < 0)
            factor = 1 / factor;

        qreal scale = qBound(mZoomFactors.first(),
                             mScale * factor,
                             mZoomFactors.back());

        // Round to at most four digits after the decimal point
        setScale(std::floor(scale * 10000 + 0.5) / 10000);
    }
}

void Zoomable::zoomIn()
{
    foreach (qreal scale, mZoomFactors) {
        if (scale > mScale) {
            setScale(scale);
            break;
        }
    }
}

void Zoomable::zoomOut()
{
    for (int i = mZoomFactors.count() - 1; i >= 0; --i) {
        if (mZoomFactors[i] < mScale) {
            setScale(mZoomFactors[i]);
            break;
        }
    }
}

void Zoomable::resetZoom()
{
    setScale(1);
}

void Zoomable::setZoomFactors(const QVector<qreal>& factors)
{
    mZoomFactors = factors;
}

void Zoomable::connectToComboBox(QComboBox *comboBox)
{
    if (mComboBox) {
        mComboBox->disconnect(this);
        if (mComboBox->lineEdit())
            mComboBox->lineEdit()->disconnect(this);
        mComboBox->setValidator(0);
    }

    mComboBox = comboBox;

    if (mComboBox) {
        mComboBox->clear();
        foreach (qreal scale, mZoomFactors)
            mComboBox->addItem(scaleToString(scale), scale);
        syncComboBox();
        connect(mComboBox, qOverload<int>(&QComboBox::activated),
                this, &Zoomable::comboActivated);

        mComboBox->setEditable(true);
        mComboBox->setInsertPolicy(QComboBox::NoInsert);
        connect(mComboBox->lineEdit(), &QLineEdit::editingFinished,
                this, &Zoomable::comboEdited);

        if (!mComboValidator)
            mComboValidator = new QRegExpValidator(mComboRegExp, this);
        mComboBox->setValidator(mComboValidator);
    }
}

void Zoomable::comboActivated(int index)
{
    setScale(mComboBox->itemData(index).toReal());
}

void Zoomable::comboEdited()
{
    int pos = mComboRegExp.indexIn(mComboBox->currentText());
    Q_UNUSED(pos)
    Q_ASSERT(pos != -1);

    qreal scale = qBound(mZoomFactors.first(),
                         qreal(mComboRegExp.cap(1).toDouble() / 100.f),
                         mZoomFactors.last());

    setScale(scale);
}

void Zoomable::syncComboBox()
{
    if (!mComboBox)
        return;

    int index = mComboBox->findData(mScale);
    // For a custom scale, the current index must be set to -1
    mComboBox->setCurrentIndex(index);
    mComboBox->setEditText(scaleToString(mScale));
}
