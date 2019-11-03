/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                         *
 *  Copyright (C) 2017 Michael Zanetti <michael.zanetti@guh.io>            *
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

#ifndef TOKENINFO_H
#define TOKENINFO_H

#include <QUuid>
#include <QDateTime>
#include <QMetaType>

namespace nymeaserver {

class TokenInfo
{
    Q_GADGET
    Q_PROPERTY(QUuid id READ id)
    Q_PROPERTY(QString username READ username)
    Q_PROPERTY(QDateTime creationTime READ creationTime)
    Q_PROPERTY(QString deviveName READ deviceName)

public:
    TokenInfo();
    TokenInfo(const QUuid &id, const QString &username, const QDateTime &creationTime, const QString &deviceName);

    QUuid id() const;
    QString username() const;
    QDateTime creationTime() const;
    QString deviceName() const;

private:
    QUuid m_id;
    QString m_username;
    QDateTime m_creationTime;
    QString m_deviceName;
};

}
Q_DECLARE_METATYPE(nymeaserver::TokenInfo)

#endif // TOKENINFO_H
