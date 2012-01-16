/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: http://www.qt-project.org/
**
** This file is part of the QtAddOn.JsonDb module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef JSONDB_INDEX_H
#define JSONDB_INDEX_H

#include <QObject>
#include <QJSEngine>
#include <QPointer>
#include <QStringList>

#include <qjsonarray.h>
#include <qjsonobject.h>
#include <qjsonvalue.h>

#include "jsondbobject.h"

#include "jsondb-global.h"
#include "objectkey.h"
#include "qmanagedbtreetxn.h"
#include "qbtreecursor.h"

QT_BEGIN_HEADER

class QManagedBtree;

QT_BEGIN_NAMESPACE_JSONDB

class JsonDbBtreeStorage;

class JsonDbIndex : public QObject
{
    Q_OBJECT
public:
    JsonDbIndex(const QString &fileName, const QString &propertyName, const QString &type,
                QObject *parent = 0);
    ~JsonDbIndex();

    QString propertyName() const { return mPropertyName; }
    QStringList fieldPath() const { return mPath; }
    QString propertyType() const { return mPropertyType; }

    QManagedBtree *bdb();

    bool setPropertyFunction(const QString &propertyFunction);
    void indexObject(const ObjectKey &objectKey, JsonDbObject &object, quint32 stateNumber);
    void deindexObject(const ObjectKey &objectKey, JsonDbObject &object, quint32 stateNumber);

    quint32 stateNumber() const;

    QManagedBtreeTxn begin();
    bool commit(quint32);
    bool abort();
    bool clearData();

    void checkIndex();
//    bool checkValidity(const QMap<QString, QJsonValue> &objects,
//                       const QMap<quint32, QString> &keyUuids,
//                       const QMap<QString, quint32> &uuidKeys,
//                       JsonDbBtreeStorage *storage);
    bool open();
    void close();

private:
    QList<QJsonValue> indexValues(JsonDbObject &object);

private slots:
    void propertyValueEmitted(QJSValue);

private:
    QString mFileName;
    QString mPropertyName;
    QStringList mPath;
    QString mPropertyType;
    quint32 mStateNumber;
    QScopedPointer<QManagedBtree> mBdb;
    QJSEngine *mScriptEngine;
    QJSValue   mPropertyFunction;
    QList<QJsonValue> mFieldValues;
    QManagedBtreeTxn mWriteTxn;
};

class JsonDbIndexCursor
{
public:
    JsonDbIndexCursor(JsonDbIndex *index);

    bool seek(const QJsonValue &value);
    bool seekRange(const QJsonValue &value);

    bool first();
    bool current(QJsonValue &key, ObjectKey &value);
    bool currentKey(QJsonValue &key);
    bool currentValue(ObjectKey &value);
    bool next();
    bool prev();

private:
    QBtreeCursor mCursor;
    JsonDbIndex *mIndex;

    JsonDbIndexCursor(const JsonDbIndexCursor&);
};

class IndexSpec {
public:
    QString propertyName;
    QStringList path;
    QString propertyType;
    QString objectType;
    bool    lazy;
    QPointer<JsonDbIndex> index;
};

QT_END_NAMESPACE_JSONDB

QT_END_HEADER

#endif
