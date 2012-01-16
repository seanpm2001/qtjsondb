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

#include "jsondb-proxy.h"
#include "jsondb-strings.h"

QT_BEGIN_NAMESPACE_JSONDB

extern bool gDebug;

JsonDbProxy::JsonDbProxy( const JsonDbOwner *owner, JsonDb *jsonDb, QObject *parent )
  : QObject(parent)
  , mOwner(owner)
  , mJsonDb(jsonDb)
{
}
JsonDbProxy::~JsonDbProxy()
{
}

QVariantMap JsonDbProxy::find(QVariantMap object)
{
    QJsonObject bson = QJsonObject::fromVariantMap(object);
    return mJsonDb->find(mOwner, bson).toVariantMap();
}
QVariantMap JsonDbProxy::create(QVariantMap object )
{
    JsonDbObject bson = QJsonObject::fromVariantMap(object);
    return mJsonDb->create(mOwner, bson).toVariantMap();
}
QVariantMap JsonDbProxy::update(QVariantMap object )
{
    JsonDbObject bson = QJsonObject::fromVariantMap(object);
    return mJsonDb->update(mOwner, bson).toVariantMap();
}
QVariantMap JsonDbProxy::remove(QVariantMap object )
{
    JsonDbObject bson = QJsonObject::fromVariantMap(object);
    return mJsonDb->remove(mOwner, bson).toVariantMap();
}
QVariantMap JsonDbProxy::notification(QString query, QStringList actions, QString script)
{
    QJsonArray actionsList;
    foreach (const QString &action, actions)
        actionsList.append(action);

    JsonDbObject notificationObject;
    notificationObject.insert(JsonDbString::kTypeStr, JsonDbString::kNotificationTypeStr);
    notificationObject.insert(JsonDbString::kQueryStr, query);
    notificationObject.insert(JsonDbString::kActionsStr, actionsList);
    notificationObject.insert("script", script);
    return mJsonDb->create(mOwner, notificationObject).toVariantMap();
}

QVariantMap JsonDbProxy::createList(QVariantList list)
{
    JsonDbObjectList blist;
    for (int i = 0; i < list.size(); i++) {
        QJsonValue bson = QJsonValue::fromVariant(list[i]);
        blist.append(bson.toObject());
    }
    return mJsonDb->createList(mOwner, blist).toVariantMap();
}

QVariantMap JsonDbProxy::updateList(QVariantList list)
{
    JsonDbObjectList blist;
    for (int i = 0; i < list.size(); i++) {
        QJsonValue bson = QJsonValue::fromVariant(list[i]);
        blist.append(bson.toObject());
    }
    return mJsonDb->removeList(mOwner, blist).toVariantMap();
}

QVariantMap JsonDbProxy::removeList(QVariantList list)
{
    JsonDbObjectList blist;
    for (int i = 0; i < list.size(); i++) {
        QJsonValue bson = QJsonValue::fromVariant(list[i]);
        blist.append(bson.toObject());
    }
    return mJsonDb->removeList(mOwner, blist).toVariantMap();
}

JsonDbMapProxy::JsonDbMapProxy( const JsonDbOwner *owner, JsonDb *jsonDb, QObject *parent )
  : QObject(parent)
  , mOwner(owner)
  , mJsonDb(jsonDb)
{
}
JsonDbMapProxy::~JsonDbMapProxy()
{
}

void JsonDbMapProxy::emitViewObject(const QString &key, const QJSValue &v)
{
    QJSValue object = v.engine()->newObject();
    object.setProperty("key", key);
    object.setProperty("value", v);
    emit viewObjectEmitted(object);
}

void JsonDbMapProxy::lookup(const QString &key, const QJSValue &value, const QJSValue &context)
{
    QJSValue query = value.engine()->newObject();
    query.setProperty("index", key);
    query.setProperty("value", value);

    emit lookupRequested(query, context);
}

void JsonDbMapProxy::lookupWithType(const QString &key, const QJSValue &value, const QJSValue &objectType, const QJSValue &context)
{
    QJSValue query = value.engine()->newObject();
    query.setProperty("index", key);
    query.setProperty("value", value);
    query.setProperty("objectType", objectType);
    emit lookupRequested(query, context);
}

JsonDbJoinProxy::JsonDbJoinProxy( const JsonDbOwner *owner, JsonDb *jsonDb, QObject *parent )
  : QObject(parent)
  , mOwner(owner)
  , mJsonDb(jsonDb)
{
}
JsonDbJoinProxy::~JsonDbJoinProxy()
{
}

void JsonDbJoinProxy::create(const QJSValue &v)
{
    emit viewObjectEmitted(v);
}

void JsonDbJoinProxy::lookup(const QJSValue &spec, const QJSValue &context)
{
    emit lookupRequested(spec, context);
}

Console::Console()
{
}

void Console::log(const QString &s)
{
    qDebug() << s;
}

void Console::debug(const QString &s)
{
//    if (gDebug)
        qDebug() << s;
}

QT_END_NAMESPACE_JSONDB
