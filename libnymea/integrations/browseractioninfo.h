/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2013 - 2020, nymea GmbH
* Contact: contact@nymea.io
*
* This file is part of nymea.
* This project including source code and documentation is protected by
* copyright law, and remains the property of nymea GmbH. All rights, including
* reproduction, publication, editing and translation, are reserved. The use of
* this project is subject to the terms of a license agreement to be concluded
* with nymea GmbH in accordance with the terms of use of nymea GmbH, available
* under https://nymea.io/license
*
* GNU Lesser General Public License Usage
* Alternatively, this project may be redistributed and/or modified under the
* terms of the GNU Lesser General Public License as published by the Free
* Software Foundation; version 3. This project is distributed in the hope that
* it will be useful, but WITHOUT ANY WARRANTY; without even the implied
* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this project. If not, see <https://www.gnu.org/licenses/>.
*
* For any further details and any questions please contact us under
* contact@nymea.io or see our FAQ/Licensing Information on
* https://nymea.io/license/faq
*
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef BROWSERACTIONINFO_H
#define BROWSERACTIONINFO_H

#include <QObject>

#include "thing.h"
#include "types/browseraction.h"

class ThingManager;

class BrowserActionInfo : public QObject
{
    Q_OBJECT
public:
    explicit BrowserActionInfo(Thing* thing, ThingManager *thingManager, const BrowserAction &browserAction, QObject *parent, quint32 timeout = 0);

    Thing* thing() const;
    BrowserAction browserAction() const;

    bool isFinished() const;
    Thing::ThingError status() const;
    QString displayMessage() const;
    QString translatedDisplayMessage(const QLocale &locale);

signals:
    void finished();
    void aborted();

public slots:
    void finish(Thing::ThingError status, const QString &displayMessage = QString());

private:
    Thing *m_thing = nullptr;
    BrowserAction m_browserAction;

    bool m_finished = false;
    Thing::ThingError m_status = Thing::ThingErrorNoError;
    QString m_displayMessage;

    ThingManager *m_thingManager = nullptr;
};

#endif // BROWSERACTIONINFO_H
