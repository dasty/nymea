/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                         *
 *  Copyright (C) 2015 Simon Stuerz <simon.stuerz@guh.guru>                *
 *  Copyright (C) 2014 Michael Zanetti <michael_zanetti@gmx.net>           *
 *                                                                         *
 *  This file is part of guh.                                              *
 *                                                                         *
 *  Guh is free software: you can redistribute it and/or modify            *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation, version 2 of the License.                *
 *                                                                         *
 *  Guh is distributed in the hope that it will be useful,                 *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *  GNU General Public License for more details.                           *
 *                                                                         *
 *  You should have received a copy of the GNU General Public License      *
 *  along with guh. If not, see <http://www.gnu.org/licenses/>.            *
 *                                                                         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "devicepluginmock.h"
#include "httpdaemon.h"

#include "plugin/device.h"
#include "devicemanager.h"
#include "plugininfo.h"

#include <QDebug>
#include <QColor>
#include <QStringList>


DevicePluginMock::DevicePluginMock()
{
}

DevicePluginMock::~DevicePluginMock()
{
}

DeviceManager::HardwareResources DevicePluginMock::requiredHardware() const
{
    return DeviceManager::HardwareResourceTimer;
}

DeviceManager::DeviceError DevicePluginMock::discoverDevices(const DeviceClassId &deviceClassId, const ParamList &params)
{
    if (deviceClassId == mockDeviceClassId || deviceClassId == mockDeviceAutoDeviceClassId) {
        qCDebug(dcMockDevice) << "starting mock discovery:" << params;
        m_discoveredDeviceCount = params.paramValue("resultCount").toInt();
        QTimer::singleShot(1000, this, SLOT(emitDevicesDiscovered()));
        return DeviceManager::DeviceErrorAsync;
    } else if (deviceClassId == mockPushButtonDeviceClassId) {
        qCDebug(dcMockDevice) << "starting mock push button discovery:" << params;
        m_discoveredDeviceCount = params.paramValue("resultCount").toInt();
        QTimer::singleShot(1000, this, SLOT(emitPushButtonDevicesDiscovered()));
        return DeviceManager::DeviceErrorAsync;
    }
    return DeviceManager::DeviceErrorDeviceClassNotFound;
}

DeviceManager::DeviceSetupStatus DevicePluginMock::setupDevice(Device *device)
{
    if (device->deviceClassId() == mockDeviceClassId || device->deviceClassId() == mockDeviceAutoDeviceClassId) {
        qCDebug(dcMockDevice) << "Mockdevice created returning true"
                              << device->paramValue("name").toString()
                              << device->paramValue("httpport").toInt()
                              << device->paramValue("async").toBool()
                              << device->paramValue("broken").toBool();

        if (device->paramValue("broken").toBool()) {
            qCWarning(dcMockDevice) << "This device is intentionally broken.";
            return DeviceManager::DeviceSetupStatusFailure;
        }

        HttpDaemon *daemon = new HttpDaemon(device, this);
        m_daemons.insert(device, daemon);

        if (!daemon->isListening()) {
            qCWarning(dcMockDevice) << "HTTP port opening failed.";
            return DeviceManager::DeviceSetupStatusFailure;
        }

        connect(daemon, &HttpDaemon::triggerEvent, this, &DevicePluginMock::triggerEvent);
        connect(daemon, &HttpDaemon::setState, this, &DevicePluginMock::setState);

        if (device->paramValue("async").toBool()) {
            m_asyncSetupDevices.append(device);
            QTimer::singleShot(1000, this, SLOT(emitDeviceSetupFinished()));
            return DeviceManager::DeviceSetupStatusAsync;
        }
        return DeviceManager::DeviceSetupStatusSuccess;
    } else if (device->deviceClassId() == mockPushButtonDeviceClassId) {
        qCDebug(dcMockDevice) << "Setup PushButton Mock device" << device->params();
        return DeviceManager::DeviceSetupStatusSuccess;
    }
    return DeviceManager::DeviceSetupStatusFailure;
}

void DevicePluginMock::deviceRemoved(Device *device)
{
    delete m_daemons.take(device);
}

void DevicePluginMock::startMonitoringAutoDevices()
{
    foreach (Device *device, myDevices()) {
        if (device->deviceClassId() == mockDeviceAutoDeviceClassId) {
            return; // We already have a Auto Mock device... do nothing.
        }
    }

    DeviceDescriptor mockDescriptor(mockDeviceAutoDeviceClassId, "Mock Device (Auto created)");

    ParamList params;
    qsrand(QDateTime::currentMSecsSinceEpoch());
    int port = 4242 + (qrand() % 1000);
    Param param("httpport", port);
    params.append(param);
    mockDescriptor.setParams(params);

    QList<DeviceDescriptor> deviceDescriptorList;
    deviceDescriptorList.append(mockDescriptor);

    emit autoDevicesAppeared(mockDeviceAutoDeviceClassId, deviceDescriptorList);
}

DeviceManager::DeviceSetupStatus DevicePluginMock::confirmPairing(const PairingTransactionId &pairingTransactionId, const DeviceClassId &deviceClassId, const ParamList &params, const QString &secret)
{
    Q_UNUSED(params)
    Q_UNUSED(secret)

    qCDebug(dcMockDevice) << "Confirm pairing";

    if (deviceClassId != mockPushButtonDeviceClassId) {
        qCWarning(dcMockDevice) << "Invalid deviceclassId -> no pairing possible with this device";
        return DeviceManager::DeviceSetupStatusFailure;
    }
    if (!m_pushbuttonPressed) {
        qCDebug(dcMockDevice) << "PushButton not pressed yet!";
        return DeviceManager::DeviceSetupStatusFailure;
    }

    m_pairingId = pairingTransactionId;
    QTimer::singleShot(1000, this, SLOT(onPushButtonPairingFinished()));
    return DeviceManager::DeviceSetupStatusAsync;
}

QList<ParamType> DevicePluginMock::configurationDescription() const
{
    QList<ParamType> params;
    ParamType mockParam1("configParamInt", QVariant::Int, 42);
    mockParam1.setLimits(1, 50);
    params.append(mockParam1);

    ParamType mockParam2("configParamBool", QVariant::Bool, true);
    params.append(mockParam2);

    return params;
}

DeviceManager::DeviceError DevicePluginMock::executeAction(Device *device, const Action &action)
{
    if (device->deviceClassId() == mockDeviceClassId || device->deviceClassId() == mockDeviceAutoDeviceClassId) {
        if (!myDevices().contains(device))
            return DeviceManager::DeviceErrorDeviceNotFound;

        if (action.actionTypeId() == mockAsyncActionTypeId || action.actionTypeId() == mockAsyncFailingActionTypeId) {
            m_asyncActions.append(qMakePair<Action, Device*>(action, device));
            QTimer::singleShot(1000, this, SLOT(emitActionExecuted()));
            return DeviceManager::DeviceErrorAsync;
        }

        if (action.actionTypeId() == mockFailingActionTypeId)
            return DeviceManager::DeviceErrorSetupFailed;

        m_daemons.value(device)->actionExecuted(action.actionTypeId());
        return DeviceManager::DeviceErrorNoError;
    } else if (device->deviceClassId() == mockPushButtonDeviceClassId) {
        if (action.actionTypeId() == colorActionTypeId) {
            QString colorString = action.param("color").value().toString();
            QColor color(colorString);
            if (!color.isValid()) {
                qCWarning(dcMockDevice) << "Invalid color parameter";
                return DeviceManager::DeviceErrorInvalidParameter;
            }
            device->setStateValue(colorStateTypeId, colorString);
            return DeviceManager::DeviceErrorNoError;
        } else if (action.actionTypeId() == percentageActionTypeId) {
            device->setStateValue(percentageStateTypeId, action.param("percentage").value().toInt());
            return DeviceManager::DeviceErrorNoError;
        }
        return DeviceManager::DeviceErrorActionTypeNotFound;
    }
    return DeviceManager::DeviceErrorDeviceClassNotFound;
}

void DevicePluginMock::setState(const StateTypeId &stateTypeId, const QVariant &value)
{
    HttpDaemon *daemon = qobject_cast<HttpDaemon*>(sender());
    if (!daemon)
        return;

    Device *device = m_daemons.key(daemon);
    device->setStateValue(stateTypeId, value);
}

void DevicePluginMock::triggerEvent(const EventTypeId &id)
{
    HttpDaemon *daemon = qobject_cast<HttpDaemon*>(sender());
    if (!daemon)
        return;

    Device *device = m_daemons.key(daemon);

    Event event(id, device->id());

    qCDebug(dcMockDevice) << "Emitting event " << event.eventTypeId();
    emit emitEvent(event);
}

void DevicePluginMock::emitDevicesDiscovered()
{
    QList<DeviceDescriptor> deviceDescriptors;

    if (m_discoveredDeviceCount > 0) {
        DeviceDescriptor d1(mockDeviceClassId, "Mock Device 1 (Discovered)", "55555");
        ParamList params;
        Param name("name", "Discovered Mock Device 1");
        Param httpParam("httpport", "55555");
        params.append(name);
        params.append(httpParam);
        d1.setParams(params);
        deviceDescriptors.append(d1);
    }

    if (m_discoveredDeviceCount > 1) {
        DeviceDescriptor d2(mockDeviceClassId, "Mock Device 2 (Discovered)", "55556");
        ParamList params;
        Param name("name", "Discovered Mock Device 2");
        Param httpParam("httpport", "55556");
        params.append(name);
        params.append(httpParam);
        d2.setParams(params);
        deviceDescriptors.append(d2);
    }

    emit devicesDiscovered(mockDeviceClassId, deviceDescriptors);
}

void DevicePluginMock::emitPushButtonDevicesDiscovered()
{
    QList<DeviceDescriptor> deviceDescriptors;

    if (m_discoveredDeviceCount > 0) {
        DeviceDescriptor d1(mockDeviceClassId, "Mock Device (Push Button)", "1");
        ParamList params;
        Param name("name", "PushButton Mock Device 1");
        params.append(name);
        d1.setParams(params);
        deviceDescriptors.append(d1);
    }

    if (m_discoveredDeviceCount > 1) {
        DeviceDescriptor d2(mockDeviceClassId, "Mock Device (Push Button)", "2");
        ParamList params;
        Param name("name", "PushButton Mock Device 2");
        params.append(name);
        d2.setParams(params);
        deviceDescriptors.append(d2);
    }
    emit devicesDiscovered(mockPushButtonDeviceClassId, deviceDescriptors);

    m_pushbuttonPressed = false;
    QTimer::singleShot(3000, this, SLOT(onPushButtonPressed()));
    qDebug() << "Start PushButton timer (will be pressed in 3 second)";

}

void DevicePluginMock::onPushButtonPressed()
{
    qCDebug(dcMockDevice) << "PushButton pressed (automatically)";
    m_pushbuttonPressed = true;
}

void DevicePluginMock::emitDeviceSetupFinished()
{
    qCDebug(dcMockDevice) << "Emitting setup finised";
    Device *device = m_asyncSetupDevices.takeFirst();
    if (device->paramValue("broken").toBool()) {
        emit deviceSetupFinished(device, DeviceManager::DeviceSetupStatusFailure);
    } else {
        emit deviceSetupFinished(device, DeviceManager::DeviceSetupStatusSuccess);
    }
}

void DevicePluginMock::emitActionExecuted()
{
    QPair<Action, Device*> action = m_asyncActions.takeFirst();
    if (action.first.actionTypeId() == mockAsyncActionTypeId) {
        m_daemons.value(action.second)->actionExecuted(action.first.actionTypeId());
        emit actionExecutionFinished(action.first.id(), DeviceManager::DeviceErrorNoError);
    } else if (action.first.actionTypeId() == mockAsyncFailingActionTypeId) {
        emit actionExecutionFinished(action.first.id(), DeviceManager::DeviceErrorSetupFailed);
    }
}

void DevicePluginMock::onPushButtonPairingFinished()
{
    qCDebug(dcMockDevice) << "Pairing PushButton Device finished";
    emit pairingFinished(m_pairingId, DeviceManager::DeviceSetupStatusSuccess);
}
