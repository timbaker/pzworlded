/*
 * Copyright 2024, Tim Baker <treectrl@users.sf.net>
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

#ifndef EXPORTLOTSPROGRESSDIALOG_H
#define EXPORTLOTSPROGRESSDIALOG_H

#include <QDialog>

namespace Ui {
class ExportLotsProgressDialog;
}

class QGraphicsRectItem;

class ExportLotsProgressDialog : public QDialog
{
    Q_OBJECT

public:
    enum class CellStatus
    {
        Missing,
        Pending,
        Exported,
        Failed,
    };

    explicit ExportLotsProgressDialog(QWidget *parent = nullptr);
    ~ExportLotsProgressDialog();

    void setWorldSize(int widthInCells, int heightInCells);
    void setCellStatus(int cellX, int cellY, CellStatus status);
    void setPrompt(const QString &str);

    void resizeEvent(QResizeEvent *event) override;

signals:
    void cancelled();

private slots:
    void cancel();

private:
    Ui::ExportLotsProgressDialog *ui;
    int mWidthInCells;
    int mHeightInCells;
    QVector<QGraphicsRectItem*> mCellItems;
};

#endif // EXPORTLOTSPROGRESSDIALOG_H
