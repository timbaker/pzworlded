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

#include "documentmanager.h"

#include "celldocument.h"
#include "cellscene.h"
#include "document.h"
#include "heightmapdocument.h"
#include "mapmanager.h"
#include "worlddocument.h"

#include <QDebug>
#include <QFileInfo>
#include <QUndoGroup>

DocumentManager *DocumentManager::mInstance = NULL;

DocumentManager *DocumentManager::instance()
{
    if (!mInstance)
        mInstance = new DocumentManager();
    return mInstance;
}

void DocumentManager::deleteInstance()
{
    delete mInstance;
}

DocumentManager::DocumentManager(QObject *parent)
    : QObject(parent)
    , mUndoGroup(new QUndoGroup(this))
    , mCurrent(0)
    , mFailedToAdd(false)
{
}

DocumentManager::~DocumentManager()
{
}

void DocumentManager::addDocument(Document *doc)
{
    // Group CellDocuments with their associated WorldDocument.
    // This is desirable when there are multiple WorldDocuments open.
    int insertAt = mDocuments.size();
    if (CellDocument *cellDoc = doc->asCellDocument()) {
        WorldDocument *worldDoc = cellDoc->worldDocument();
        insertAt = -1;
        int n = 0;
        foreach (Document *walk, mDocuments) {
            if (walk == worldDoc)
                insertAt = n + 1;
            else if (CellDocument *cd2 = walk->asCellDocument()) {
                if (cd2->worldDocument() == worldDoc)
                    insertAt = n + 1;
            }
            ++n;
        }
        Q_ASSERT(insertAt > 0);
    }
    mDocuments.insert(insertAt, doc);

    mUndoGroup->addStack(doc->undoStack());
    mFailedToAdd = false;
    emit documentAdded(doc);
    if (mFailedToAdd) {
        closeDocument(insertAt);
        return;
    }
    setCurrentDocument(doc);
}

void DocumentManager::closeDocument(int index)
{
    Q_ASSERT(index >= 0 && index < mDocuments.size());

    // If the document being closed is a WorldDocument, then force all
    // associated CellDocuments to close first.
    if (WorldDocument *worldDoc = mDocuments[index]->asWorldDocument()) {
        for (int i = mDocuments.size() - 1; i >= 0; i--) {
            if (CellDocument *cellDoc = mDocuments[i]->asCellDocument()) {
                if (cellDoc->worldDocument() == worldDoc)
                    closeDocument(i);
            }
        }
        // Re-get the index of the document to close
        index = indexOf(worldDoc);
    }

    Document *doc = mDocuments.takeAt(index);
    emit documentAboutToClose(index, doc);
    if (doc == mCurrent) {
        if (mDocuments.size())
            setCurrentDocument(mDocuments.first());
        else
            setCurrentDocument(0);
    }
//    mUndoGroup->removeStack(doc->undoStack());
    delete doc;
}

void DocumentManager::closeDocument(Document *doc)
{
    closeDocument(mDocuments.indexOf(doc));
}

void DocumentManager::closeCurrentDocument()
{
    if (mCurrent)
        closeDocument(indexOf(mCurrent));
}

void DocumentManager::closeAllDocuments()
{
    while (documentCount())
        closeCurrentDocument();
}

int DocumentManager::findDocument(const QString &fileName)
{
    const QString canonicalFilePath = QFileInfo(fileName).canonicalFilePath();
    if (canonicalFilePath.isEmpty()) // file doesn't exist
        return -1;

    for (int i = 0; i < mDocuments.size(); ++i) {
        QFileInfo fileInfo(mDocuments.at(i)->fileName());
        if (fileInfo.canonicalFilePath() == canonicalFilePath)
            return i;
    }

    return -1;
}

CellDocument *DocumentManager::findDocument(WorldCell *cell)
{
    foreach (Document *doc, mDocuments) {
        if (CellDocument *cellDoc = doc->asCellDocument()) {
            if (cellDoc->cell() == cell)
                return cellDoc;
        }
    }
    return 0;
}

HeightMapDocument *DocumentManager::findHMDocument(WorldDocument *worldDoc)
{
    foreach (Document *doc, mDocuments) {
        if (HeightMapDocument *hmDoc = doc->asHeightMapDocument()) {
            if (hmDoc->worldDocument() == worldDoc)
                return hmDoc;
        }
    }
    return 0;
}

void DocumentManager::setCurrentDocument(int index)
{
    Q_ASSERT(index >= -1 && index < mDocuments.size());
    setCurrentDocument((index >= 0) ? mDocuments.at(index) : 0);
}

void DocumentManager::setCurrentDocument(Document *doc)
{
    Q_ASSERT(!doc || mDocuments.contains(doc));

    if (doc == mCurrent)
        return;

    qDebug() << "current document was " << mCurrent << " is becoming " << doc;

    if (mCurrent) {
    }

    mCurrent = doc;

    if (mCurrent) {
        mUndoGroup->setActiveStack(mCurrent->undoStack());
    }

    emit currentDocumentChanged(doc);
}

Document *DocumentManager::documentAt(int index) const
{
    Q_ASSERT(index >= 0 && index < mDocuments.size());
    return mDocuments.at(index);
}

void DocumentManager::setFailedToAdd()
{
    mFailedToAdd = true;
}

bool DocumentManager::failedToAdd()
{
    return mFailedToAdd;
}
