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

#ifndef DOCUMENTMANAGER_H
#define DOCUMENTMANAGER_H

#include "filesystemwatcher.h"

#include <QObject>
#include <QSet>
#include <QTimer>

class Document;
class CellDocument;
class WorldCell;
class WorldDocument;

namespace Tiled {
class Map;
}

class QUndoGroup;

class DocumentManager : public QObject
{
    Q_OBJECT

public:
    explicit DocumentManager(QObject *parent = nullptr);

    static DocumentManager *instance();
    static void deleteInstance();

    void addDocument(Document *doc);
    void closeDocument(int index);
    void closeDocument(Document *doc);

    void closeCurrentDocument();
    void closeAllDocuments();

    int findDocument(const QString &fileName);
    CellDocument *findDocument(WorldCell *cell);

    void setCurrentDocument(int index);
    void setCurrentDocument(Document *doc);
    Document *currentDocument() const { return mCurrent; }

    const QList<Document*> &documents() const { return mDocuments; }
    int documentCount() const { return mDocuments.size(); }

    Document *documentAt(int index) const;
    int indexOf(Document *doc) { return mDocuments.indexOf(doc); }

    QUndoGroup *undoGroup() const { return mUndoGroup; }

    void setFailedToAdd();
    bool failedToAdd();

signals:
    void currentDocumentChanged(Document *doc);
    void documentAdded(Document *doc);
    void documentAboutToClose(int index, Document *doc);

private slots:
    void fileChanged(const QString &path);
    void fileChangedTimeout();

private:
    ~DocumentManager();

    QList<Document*> mDocuments;
    static DocumentManager *mInstance;
    QUndoGroup *mUndoGroup;
    Document *mCurrent;
    bool mFailedToAdd;

    Tiled::Internal::FileSystemWatcher *mFileSystemWatcher;
    QSet<QString> mChangedFiles;
    QTimer mChangedFilesTimer;
};

#endif // DOCUMENTMANAGER_H
