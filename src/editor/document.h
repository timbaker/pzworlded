/*
 * Copyright 2012, Tim Baker <treectrl@users.sf.net>
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

#ifndef DOCUMENT_H
#define DOCUMENT_H

#include <QObject>

class BaseGraphicsView;
class CellDocument;
class PathDocument;
class WorldDocument;

class QUndoStack;

/**
  * Base class for documents.
  */
class Document : public QObject
{
    Q_OBJECT

public:
    enum DocumentType {
        CellDocType,
        WorldDocType,
        PathDocType
    };

public:
    explicit Document(DocumentType type);

    DocumentType type() const { return mType; }
    bool isCellDocument() const { return mType == CellDocType; }
    bool isWorldDocument() const { return mType == WorldDocType; }
    bool isPathDocument() const { return mType == PathDocType; }

    CellDocument *asCellDocument();
    WorldDocument *asWorldDocument();
    PathDocument *asPathDocument();

    void setView(BaseGraphicsView *view) { mView = view; }
    BaseGraphicsView *view() const { return mView; }

    QUndoStack *undoStack() const { return mUndoStack; }
    virtual bool isModified() const;

    virtual void setFileName(const QString &fileName) = 0;
    virtual const QString &fileName() const = 0;
    virtual bool save(const QString &filePath, QString &error) = 0;

    QString suffix() const;

protected:
    QUndoStack *mUndoStack;

private:
    DocumentType mType;
    BaseGraphicsView *mView;
};

#endif // DOCUMENT_H
