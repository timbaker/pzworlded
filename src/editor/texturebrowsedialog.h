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

#ifndef TEXTUREBROWSEDIALOG_H
#define TEXTUREBROWSEDIALOG_H

#include <QDialog>

class WorldTexture;

namespace Ui {
class TextureBrowseDialog;
}

class TextureBrowseDialog : public QDialog
{
    Q_OBJECT
    
public:
    static TextureBrowseDialog *instance();
    static void deleteInstance();

    void setTextures(const QList<WorldTexture*> &textures);
    void setCurrentTexture(WorldTexture *tex);

    WorldTexture *chosen();
    
private:
    Q_DISABLE_COPY(TextureBrowseDialog)
    static TextureBrowseDialog *mInstance;
    explicit TextureBrowseDialog(QWidget *parent = 0);
    ~TextureBrowseDialog();

    Ui::TextureBrowseDialog *ui;
};

#endif // TEXTUREBROWSEDIALOG_H
