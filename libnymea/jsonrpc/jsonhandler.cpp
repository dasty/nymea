/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                         *
 *  Copyright (C) 2019 Michael Zanetti <michael.zanetti@nymea.io>          *
 *                                                                         *
 *  This file is part of nymea.                                            *
 *                                                                         *
 *  nymea is free software: you can redistribute it and/or modify          *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation, version 2 of the License.                *
 *                                                                         *
 *  nymea is distributed in the hope that it will be useful,               *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *  GNU General Public License for more details.                           *
 *                                                                         *
 *  You should have received a copy of the GNU General Public License      *
 *  along with nymea. If not, see <http://www.gnu.org/licenses/>.          *
 *                                                                         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "jsonhandler.h"

#include "loggingcategories.h"

#include <QDebug>
#include <QDateTime>

JsonHandler::JsonHandler(QObject *parent) : QObject(parent)
{
    qRegisterMetaType<QVariant::Type>();
    registerEnum<BasicType>();
}

QVariantMap JsonHandler::jsonEnums() const
{
    return m_enums;
}

QVariantMap JsonHandler::jsonFlags() const
{
    return m_flags;
}

QVariantMap JsonHandler::jsonObjects() const
{
    return m_objects;
}

QVariantMap JsonHandler::jsonMethods() const
{
    return m_methods;
}

QVariantMap JsonHandler::jsonNotifications() const
{
    return m_notifications;
}

QString JsonHandler::objectRef(const QString &objectName)
{
    return "$ref:" + objectName;
}

JsonHandler::BasicType JsonHandler::variantTypeToBasicType(QVariant::Type variantType)
{
    switch (variantType) {
    case QVariant::Uuid:
        return Uuid;
    case QVariant::String:
        return String;
    case QVariant::StringList:
        return StringList;
    case QVariant::Int:
        return Int;
    case QVariant::UInt:
        return Uint;
    case QVariant::Double:
        return Double;
    case QVariant::Bool:
        return Bool;
    case QVariant::Color:
        return Color;
    case QVariant::Time:
        return Time;
    case QVariant::Map:
        return Object;
    case QVariant::DateTime:
        return Uint; // DateTime is represented as time_t
    default:
        return Variant;
    }
}

QVariant::Type JsonHandler::basicTypeToVariantType(JsonHandler::BasicType basicType)
{
    switch (basicType) {
    case Uuid:
        return QVariant::Uuid;
    case String:
        return QVariant::String;
    case StringList:
        return QVariant::StringList;
    case Int:
        return QVariant::Int;
    case Uint:
        return QVariant::UInt;
    case Double:
        return QVariant::Double;
    case Bool:
        return QVariant::Bool;
    case Color:
        return QVariant::Color;
    case Time:
        return QVariant::Time;
    case Object:
        return QVariant::Map;
    case Variant:
        return QVariant::Invalid;
    }
    return QVariant::Invalid;
}

void JsonHandler::registerObject(const QString &name, const QVariantMap &object)
{
    m_objects.insert(name, object);
}

void JsonHandler::registerMethod(const QString &name, const QString &description, const QVariantMap &params, const QVariantMap &returns, bool /*deprecated*/)
{
    QVariantMap methodData;
    methodData.insert("description", description);
    methodData.insert("params", params);
    methodData.insert("returns", returns);
//    methodData.insert("deprecated", deprecated);

    m_methods.insert(name, methodData);
}

void JsonHandler::registerNotification(const QString &name, const QString &description, const QVariantMap &params, bool /*deprecated*/)
{
    QVariantMap notificationData;
    notificationData.insert("description", description);
    notificationData.insert("params", params);
//    notificationData.insert("deprecated", deprecated);

    m_notifications.insert(name, notificationData);
}

JsonReply *JsonHandler::createReply(const QVariantMap &data) const
{
    return JsonReply::createReply(const_cast<JsonHandler*>(this), data);
}

JsonReply *JsonHandler::createAsyncReply(const QString &method) const
{
    return JsonReply::createAsyncReply(const_cast<JsonHandler*>(this), method);
}

void JsonHandler::registerObject(const QMetaObject &metaObject)
{
    QString className = QString(metaObject.className()).split("::").last();
    QVariantMap description;
    for (int i = 0; i < metaObject.propertyCount(); i++) {
        QMetaProperty metaProperty = metaObject.property(i);
        QString name = metaProperty.name();
        if (name == "objectName") {
            continue; // Skip QObject's objectName property
        }
        if (metaProperty.isUser()) {
            name.prepend("o:");
        }
        QVariant typeName;
        if (metaProperty.type() == QVariant::UserType) {
            if (metaProperty.typeName() == QStringLiteral("QVariant::Type")) {
                typeName = QString("$ref:BasicType");
            } else if (QString(metaProperty.typeName()).startsWith("QList")) {
                QString elementType = QString(metaProperty.typeName()).remove("QList<").remove(">");
                QVariant::Type variantType = QVariant::nameToType(elementType.toUtf8());
                typeName = QVariantList() << enumValueName(variantTypeToBasicType(variantType));
            } else {
                typeName = QString("$ref:%1").arg(QString(metaProperty.typeName()).split("::").last());
            }
        } else if (metaProperty.isEnumType()) {
            typeName = QString("$ref:%1").arg(QString(metaProperty.typeName()).split("::").last());
        } else if (metaProperty.isFlagType()) {
            typeName = QVariantList() << "$ref:" + m_flagsEnums.value(metaProperty.name());
        } else if (metaProperty.type() == QVariant::List) {
            typeName = QVariantList() << enumValueName(Variant);
        } else {
            typeName = enumValueName(variantTypeToBasicType(metaProperty.type()));
        }
        description.insert(name, typeName);
    }
    m_objects.insert(className, description);
    m_metaObjects.insert(className, metaObject);
}

QVariant JsonHandler::pack(const QMetaObject &metaObject, const void *value) const
{
    QString className = QString(metaObject.className()).split("::").last();
    if (m_listMetaObjects.contains(className)) {
        QVariantList ret;
        QMetaProperty countProperty = metaObject.property(metaObject.indexOfProperty("count"));
        QMetaObject entryMetaObject = m_metaObjects.value(m_listEntryTypes.value(className));
        int count = countProperty.readOnGadget(value).toInt();
        QMetaMethod getMethod = metaObject.method(metaObject.indexOfMethod("get(int)"));
        for (int i = 0; i < count; i++) {
            QVariant entry;
            getMethod.invokeOnGadget(const_cast<void*>(value), Q_RETURN_ARG(QVariant, entry), Q_ARG(int, i));
            ret.append(pack(entryMetaObject, entry.data()));
        }
        return ret;
    }

    if (m_metaObjects.contains(className)) {
        QVariantMap ret;
        for (int i = 0; i < metaObject.propertyCount(); i++) {
            QMetaProperty metaProperty = metaObject.property(i);

            // Skip QObject's objectName property
            if (metaProperty.name() == QStringLiteral("objectName")) {
                continue;
            }

            QVariant propertyValue = metaProperty.readOnGadget(value);
            // If it's optional and empty, we may skip it
            if (metaProperty.isUser() && (!propertyValue.isValid() || propertyValue.isNull())) {
                continue;
            }

            // Pack flags
            if (metaProperty.isFlagType()) {
                QString flagName = QString(metaProperty.typeName()).split("::").last();
                Q_ASSERT_X(m_metaFlags.contains(flagName), this->metaObject()->className(), QString("Cannot pack %1. %2 is not registered in this handler.").arg(className).arg(flagName).toUtf8());
                QMetaEnum metaFlag = m_metaFlags.value(flagName);
                int flagValue = propertyValue.toInt();
                QStringList flags;
                for (int i = 0; i < metaFlag.keyCount(); i++) {
                    if ((metaFlag.value(i) & flagValue) > 0) {
                        flags.append(metaFlag.key(i));
                    }
                }
                ret.insert(metaProperty.name(), flags);
                continue;
            }

            // Pack enums
            if (metaProperty.isEnumType()) {
                QString enumName = QString(metaProperty.typeName()).split("::").last();
                Q_ASSERT_X(m_metaEnums.contains(enumName), this->metaObject()->className(), QString("Cannot pack %1. %2 is not registered in this handler.").arg(className).arg(metaProperty.typeName()).toUtf8());
                QMetaEnum metaEnum = m_metaEnums.value(enumName);
                ret.insert(metaProperty.name(), metaEnum.key(propertyValue.toInt()));
                continue;
            }

            // Basic type/Variant type
            if (metaProperty.typeName() == QStringLiteral("QVariant::Type")) {
                QMetaEnum metaEnum = QMetaEnum::fromType<BasicType>();
                ret.insert(metaProperty.name(), metaEnum.key(variantTypeToBasicType(propertyValue.template value<QVariant::Type>())));
                continue;
            }

            // Our own objects
            if (metaProperty.type() == QVariant::UserType) {
                QString propertyTypeName = QString(metaProperty.typeName()).split("::").last();
                if (m_listMetaObjects.contains(propertyTypeName)) {
                    QMetaObject entryMetaObject = m_listMetaObjects.value(propertyTypeName);
                    QVariant packed = pack(entryMetaObject, propertyValue.data());
                    if (!metaProperty.isUser() || packed.toList().count() > 0) {
                        ret.insert(metaProperty.name(), packed);
                    }
                    continue;
                }

                if (m_metaObjects.contains(propertyTypeName)) {
                    QMetaObject entryMetaObject = m_metaObjects.value(propertyTypeName);
                    QVariant packed = pack(entryMetaObject, propertyValue.data());
                    int isValidIndex = entryMetaObject.indexOfMethod("isValid()");
                    bool isValid = true;
                    if (isValidIndex >= 0) {
                        QMetaMethod isValidMethod = entryMetaObject.method(isValidIndex);
                        isValidMethod.invokeOnGadget(propertyValue.data(), Q_RETURN_ARG(bool, isValid));
                    }
                    if (isValid || !metaProperty.isUser()) {
                        ret.insert(metaProperty.name(), packed);
                    }
                    continue;
                }

                // Manually converting QList<int>... Only QVariantList is known to the meta system
                if (propertyTypeName.startsWith("QList<int>")) {
                    qWarning() << "Packing list" << metaProperty.name() << propertyValue.toList();
                    QVariantList list;
                    foreach (int entry, propertyValue.value<QList<int>>()) {
                        list << entry;
                    }
                    if (!list.isEmpty() || !metaProperty.isUser()) {
                        ret.insert(metaProperty.name(), list);
                    }
                    continue;
                }

                Q_ASSERT_X(false, this->metaObject()->className(), QString("Unregistered property type: %1").arg(propertyTypeName).toUtf8());
                qCWarning(dcJsonRpc()) << "Cannot pack property of unregistered object type" << propertyTypeName;
                continue;
            }

            // Standard properties, QString, int etc...
            // Special treatment for QDateTime (converting to time_t)
            if (metaProperty.type() == QVariant::DateTime) {
                QDateTime dateTime = propertyValue.toDateTime();
                if (metaProperty.isUser() && dateTime.toTime_t() == 0) {
                    continue;
                }
                propertyValue = propertyValue.toDateTime().toTime_t();
            } else if (metaProperty.type() == QVariant::Time) {
                propertyValue = propertyValue.toTime().toString("hh:mm");
            }
            ret.insert(metaProperty.name(), propertyValue);
        }
        return ret;
    }

    Q_ASSERT_X(false, this->metaObject()->className(), QString("Unregistered object type: %1").arg(className).toUtf8());
    qCWarning(dcJsonRpc()) << "Cannot pack object of unregistered type" << className;
    return QVariant();    
}

QVariant JsonHandler::unpack(const QMetaObject &metaObject, const QVariant &value) const
{
    QString typeName = QString(metaObject.className()).split("::").last();

    // If it's a list object, loop over count
    if (m_listMetaObjects.contains(typeName)) {
        qWarning() << "** Unpacking" << typeName;
        if (value.type() != QVariant::List) {
            qCWarning(dcJsonRpc()) << "Cannot unpack" << typeName << ". Value is not in list format:" << value;
            return QVariant();
        }

        QVariantList list = value.toList();

        int typeId = QMetaType::type(metaObject.className());
        void* ptr = QMetaType::create(typeId);
        Q_ASSERT_X(typeId != 0, this->metaObject()->className(), QString("Cannot handle unregistered meta type %1").arg(metaObject.className()).toUtf8());

        QMetaObject entryMetaObject = m_metaObjects.value(m_listEntryTypes.value(typeName));
        QMetaMethod putMethod = metaObject.method(metaObject.indexOfMethod("put(QVariant)"));

        foreach (const QVariant &variant, list) {
            QVariant value = unpack(entryMetaObject, variant);
            qWarning() << "Putting" << value << putMethod.name() << ptr << typeId;
            putMethod.invokeOnGadget(ptr, Q_ARG(QVariant, value));
        }

        QVariant ret = QVariant(typeId, ptr);
        QMetaType::destroy(typeId, ptr);
        return ret;
    }

    // if it's an object, loop over all properties
    if (m_metaObjects.contains(typeName)) {
        qWarning() << "*** Unpacking" << typeName;
        QVariantMap map = value.toMap();
        int typeId = QMetaType::type(metaObject.className());
        Q_ASSERT_X(typeId != 0, this->metaObject()->className(), QString("Cannot handle unregistered meta type %1").arg(typeName).toUtf8());
        void* ptr = QMetaType::create(typeId);
        for (int i = 0; i < metaObject.propertyCount(); i++) {
            QMetaProperty metaProperty = metaObject.property(i);
            if (metaProperty.name() == QStringLiteral("objectName")) {
                continue;
            }
            if (!metaProperty.isWritable()) {
                continue;
            }
            if (!metaProperty.isUser()) {
                Q_ASSERT_X(map.contains(metaProperty.name()), this->metaObject()->className(), QString("Missing property %1 in map.").arg(metaProperty.name()).toUtf8());
            }

            if (map.contains(metaProperty.name())) {

                QString propertyTypeName = QString(metaProperty.typeName()).split("::").last();
                QVariant variant = map.value(metaProperty.name());

                // recurse into child lists
                if (m_listMetaObjects.contains(propertyTypeName)) {
                    QMetaObject propertyMetaObject = m_listMetaObjects.value(propertyTypeName);
                    qWarning() << "Entering list object" << propertyTypeName << propertyMetaObject.className();
                    metaProperty.writeOnGadget(ptr, unpack(propertyMetaObject, variant));
                    continue;
                }

                // recurse into child objects
                if (m_metaObjects.contains(propertyTypeName)) {
                    QMetaObject propertyMetaObject = m_metaObjects.value(propertyTypeName);
                    metaProperty.writeOnGadget(ptr, unpack(propertyMetaObject, variant));
                    continue;
                }

                if (metaProperty.typeName() == QStringLiteral("QList<int>")) {
                    QList<int> intList;
                    foreach (const QVariant &val, variant.toList()) {
                        intList.append(val.toInt());
                    }
                    metaProperty.writeOnGadget(ptr, QVariant::fromValue(intList));
                    continue;
                }

                // Special treatment for QDateTime (convert from time_t)
                if (metaProperty.type() == QVariant::DateTime) {
                    variant = QDateTime::fromTime_t(variant.toUInt());
                } else if (metaProperty.type() == QVariant::Time) {
                    variant = QTime::fromString(variant.toString(), "hh:mm");
                }

                // For basic properties just write the veriant as is
                metaProperty.writeOnGadget(ptr, variant);
            }

        }
        QVariant ret = QVariant(typeId, ptr);
        QMetaType::destroy(typeId, ptr);
        return ret;
    }

    return QVariant();
}



