/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
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
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QDebug>
#include <QElapsedTimer>
#include <QJSValue>
#include <QJSValueIterator>
#include <QStringList>

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include "jsondbpartition.h"
#include "jsondb-strings.h"
#include "jsondb-error.h"
#include "json.h"

#include "jsondb.h"
#include "jsondbproxy.h"
#include "jsondbobjecttable.h"
#include "jsondbreducedefinition.h"

QT_BEGIN_NAMESPACE_JSONDB

JsonDbReduceDefinition::JsonDbReduceDefinition(JsonDb *jsonDb, const JsonDbOwner *owner, JsonDbPartition *partition,
                                   QJsonObject definition, QObject *parent)
    : QObject(parent)
    , mJsonDb(jsonDb)
    , mOwner(owner)
    , mPartition(partition)
    , mDefinition(definition)
    , mScriptEngine(0)
    , mUuid(mDefinition.value(JsonDbString::kUuidStr).toString())
    , mTargetType(mDefinition.value("targetType").toString())
    , mTargetTable(mPartition->findObjectTable(mTargetType))
    , mSourceType(mDefinition.value("sourceType").toString())
    , mTargetKeyName(mDefinition.contains("targetKeyName") ? mDefinition.value("targetKeyName").toString() : QString("key"))
    , mTargetValueName(mDefinition.contains("targetValueName") ? mDefinition.value("targetValueName").toString() : QString("value"))
    , mSourceKeyName(mDefinition.contains("sourceKeyName") ? mDefinition.value("sourceKeyName").toString() : QString("key"))
    , mSourceKeyNameList(mSourceKeyName.split("."))
{
}

void JsonDbReduceDefinition::initScriptEngine()
{
    if (mScriptEngine)
        return;

    mScriptEngine = new QJSEngine(this);
    Q_ASSERT(!mDefinition.value("add").toString().isEmpty());
    Q_ASSERT(!mDefinition.value("subtract").toString().isEmpty());

    QJSValue globalObject = mScriptEngine->globalObject();
    globalObject.setProperty("console", mScriptEngine->newQObject(new Console()));

    QString script = mDefinition.value("add").toString();
    mAddFunction = mScriptEngine->evaluate(QString("var %1 = (%2); %1;").arg("add").arg(script));

    if (mAddFunction.isError() || !mAddFunction.isCallable()) {
        setError("Unable to parse add function: " + mAddFunction.toString());
        return;
    }

    script = mDefinition.value("subtract").toString();
    mSubtractFunction = mScriptEngine->evaluate(QString("var %1 = (%2); %1;").arg("subtract").arg(script));

    if (mSubtractFunction.isError() || !mSubtractFunction.isCallable())
        setError("Unable to parse subtract function: " + mSubtractFunction.toString());
}

void JsonDbReduceDefinition::releaseScriptEngine()
{
    mAddFunction = QJSValue();
    mSubtractFunction = QJSValue();
    delete mScriptEngine;
    mScriptEngine = 0;
}

void JsonDbReduceDefinition::updateObject(JsonDbObject before, JsonDbObject after)
{
    initScriptEngine();
    Q_ASSERT(mAddFunction.isCallable());

    QJsonValue beforeKeyValue = mSourceKeyName.contains(".")
        ? JsonDb::propertyLookup(before, mSourceKeyNameList)
        : before.value(mSourceKeyName);
    QJsonValue afterKeyValue = mSourceKeyName.contains(".")
        ? JsonDb::propertyLookup(after, mSourceKeyNameList)
        : after.value(mSourceKeyName);

    if (!after.isEmpty() && !before.isEmpty() && (beforeKeyValue != afterKeyValue)) {
        // do a subtract only on the before key
        if (!beforeKeyValue.isUndefined())
            updateObject(before, QJsonObject());

        // and then continue here with the add with the after key
        before = QJsonObject();
    }

    const QJsonValue keyValue(after.isEmpty() ? beforeKeyValue : afterKeyValue);
    if (keyValue.isUndefined())
        return;

    GetObjectsResult getObjectResponse = mTargetTable->getObjects(mTargetKeyName, keyValue, mTargetType);
    if (!getObjectResponse.error.isNull())
        setError(getObjectResponse.error.toString());

    JsonDbObject previousObject;
    QJsonValue previousValue(QJsonValue::Undefined);

    JsonDbObjectList previousResults = getObjectResponse.data;
    for (int k = 0; k < previousResults.size(); ++k) {
        JsonDbObject previous = previousResults.at(k);
        if (previous.value("_reduceUuid").toString() == mUuid) {
            previousObject = previous;
            previousValue = previousObject;
            break;
        }
    }

    QJsonValue value = previousValue;
    if (!before.isEmpty())
        value = subtractObject(keyValue, value, before);
    if (!after.isEmpty())
        value = addObject(keyValue, value, after);

    QJsonObject res;
    // if we had a previous object to reduce
    if (previousObject.contains(JsonDbString::kUuidStr)) {
        // and now the value is undefined
        if (value.isUndefined()) {
            // then remove it
            res = mJsonDb->removeViewObject(mOwner, previousObject, mPartition->name());
        } else {
            //otherwise update it
            JsonDbObject reduced(value.toObject());
            reduced.insert(JsonDbString::kTypeStr, mTargetType);
            reduced.insert(JsonDbString::kUuidStr,
                         previousObject.value(JsonDbString::kUuidStr));
            reduced.insert(JsonDbString::kVersionStr,
                         previousObject.value(JsonDbString::kVersionStr));
            reduced.insert(mTargetKeyName, keyValue);
            reduced.insert("_reduceUuid", mUuid);
            res = mJsonDb->updateViewObject(mOwner, reduced, mPartition->name());
        }
    } else {
        // otherwise create the new object
        JsonDbObject reduced(value.toObject());
        reduced.insert(JsonDbString::kTypeStr, mTargetType);
        reduced.insert(mTargetKeyName, keyValue);
        reduced.insert("_reduceUuid", mUuid);
        res = mJsonDb->createViewObject(mOwner, reduced, mPartition->name());
    }

    if (JsonDb::responseIsError(res))
        setError("Error executing add function: " +
                 res.value(JsonDbString::kErrorStr).toObject().value(JsonDbString::kMessageStr).toString());
}

QJsonValue JsonDbReduceDefinition::addObject(const QJsonValue &keyValue, const QJsonValue &previousValue, JsonDbObject object)
{
    initScriptEngine();
    QJSValue svKeyValue = JsonDb::toJSValue(keyValue, mScriptEngine);
    QJSValue svPreviousValue = JsonDb::toJSValue(previousValue.toObject().value(mTargetValueName), mScriptEngine);
    QJSValue svObject = JsonDb::toJSValue(object, mScriptEngine);

    QJSValueList reduceArgs;
    reduceArgs << svKeyValue << svPreviousValue << svObject;
    QJSValue reduced = mAddFunction.call(reduceArgs);

    if (!reduced.isUndefined() && !reduced.isError()) {
        QJsonObject jsonReduced;
        jsonReduced.insert(mTargetValueName, JsonDb::fromJSValue(reduced));
        return jsonReduced;
    } else {

        if (reduced.isError())
            setError("Error executing add function: " + reduced.toString());

        return QJsonValue(QJsonValue::Undefined);
    }
}

QJsonValue JsonDbReduceDefinition::subtractObject(const QJsonValue &keyValue, const QJsonValue &previousValue, JsonDbObject object)
{
    initScriptEngine();
    Q_ASSERT(mSubtractFunction.isCallable());

    QJSValue svKeyValue = JsonDb::toJSValue(keyValue, mScriptEngine);
    QJSValue svPreviousValue = JsonDb::toJSValue(previousValue.toObject().value(mTargetValueName),
                                                 mScriptEngine);
    QJSValue sv = JsonDb::toJSValue(object, mScriptEngine);

    QJSValueList reduceArgs;
    reduceArgs << svKeyValue << svPreviousValue << sv;
    QJSValue reduced = mSubtractFunction.call(reduceArgs);

    if (!reduced.isUndefined() && !reduced.isError()) {
        QJsonObject jsonReduced;
        jsonReduced.insert(mTargetValueName, JsonDb::fromJSValue(reduced));
        return jsonReduced;
    } else {
        if (reduced.isError())
            setError("Error executing subtract function: " + reduced.toString());
        return QJsonValue(QJsonValue::Undefined);
    }
}

bool JsonDbReduceDefinition::isActive() const
{
    return !mDefinition.contains(JsonDbString::kActiveStr) || mDefinition.value(JsonDbString::kActiveStr).toBool();
}

void JsonDbReduceDefinition::setError(const QString &errorMsg)
{
    mDefinition.insert(JsonDbString::kActiveStr, false);
    mDefinition.insert(JsonDbString::kErrorStr, errorMsg);
    if (JsonDbPartition *partition = mJsonDb->findPartition(mPartition->name())) {
        WithTransaction transaction(partition, "JsonDbReduceDefinition::setError");
        JsonDbObjectTable *objectTable = partition->findObjectTable(JsonDbString::kReduceTypeStr);
        transaction.addObjectTable(objectTable);
        JsonDbObject doc(mDefinition);
        JsonDbObject _delrec;
        partition->getObject(mUuid, _delrec, JsonDbString::kReduceTypeStr);
        partition->updatePersistentObject(_delrec, doc);
        transaction.commit();
    }
}

bool JsonDbReduceDefinition::validateDefinition(const JsonDbObject &reduce, const QSet<QString> viewTypes, QString &message)
{
    message.clear();
    QString targetType = reduce.value("targetType").toString();
    QString sourceType = reduce.value("sourceType").toString();

    if (targetType.isEmpty())
        message = QLatin1Literal("targetType property for Reduce not specified");
    else if (!viewTypes.contains(targetType))
        message = QLatin1Literal("targetType must be of a type that extends View");
    else if (sourceType.isEmpty())
        message = QLatin1Literal("sourceType property for Reduce not specified");
    else if (reduce.value("sourceKeyName").toString().isEmpty())
        message = QLatin1Literal("sourceKeyName property for Reduce not specified");
    else if (reduce.value("add").toString().isEmpty())
        message = QLatin1Literal("add function for Reduce not specified");
    else if (reduce.value("subtract").toString().isEmpty())
        message = QLatin1Literal("subtract function for Reduce not specified");

    return message.isEmpty();
}

QT_END_NAMESPACE_JSONDB
