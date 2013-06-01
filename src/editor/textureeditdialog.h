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

#ifndef TEXTUREEDITDIALOG_H
#define TEXTUREEDITDIALOG_H

#include <QDialog>

#include <QMap>

class Document;
class PathDocument;
class PathTexture;
class WorldPath;
class WorldPathLayer;
class WorldTexture;

class QDoubleSpinBox;
class QSpinBox;

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
    void documentChanged(Document *doc);

    void xScaleChanged(double scale);
    void yScaleChanged(double scale);

    void xFit();
    void yFit();

    void xShiftChanged(int shift);
    void yShiftChanged(int shift);

    void rotationChanged(double rotation);

    void browseTexture();

    void alignChanged();

    void strokeChanged(double thickness);

    void beforeRemovePath(WorldPathLayer *layer, int index, WorldPath *path);
    void afterSetPathTextureParameters(WorldPath *path, const PathTexture &params);
    void afterSetPathStroke(WorldPath *path, qreal oldStroke);

    void focusChanged(QWidget *prev, QWidget *curr);

private:
    void setValue(QDoubleSpinBox *w, double value);
    void setValue(QSpinBox *w, int value);

private:
    Q_DISABLE_COPY(TextureEditDialog)
    static TextureEditDialog *mInstance;
    explicit TextureEditDialog(QWidget *parent = 0);
    ~TextureEditDialog();

    Ui::TextureEditDialog *ui;

    WorldPath *mPath;
    bool mSynching;
    QMap<QString,QPixmap> mPixmaps;

    QDoubleSpinBox *mFocusDoubleSpinBox;
    QSpinBox *mFocusSpinBox;
    double mDelayedDoubleValue;
    int mDelayedIntValue;

    PathDocument *mDocument;
};

#endif // TEXTUREEDITDIALOG_H
