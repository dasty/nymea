/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                         *
 *  Copyright (C) 2015 Simon Stürz <simon.stuerz@guh.io>                   *
 *  Copyright (C) 2014 Michael Zanetti <michael_zanetti@gmx.net>           *
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

/*!
    \class nymeaserver::SslServer
    \brief This class represents the SSL server for nymead.

    \ingroup server
    \inmodule core

    \inherits TcpServer

    The SSL server allows clients to connect to the JSON-RPC API over an encrypted SSL/TLS connection.

    \sa WebSocketServer, TransportInterface, TcpServer
*/

/*! \fn nymeaserver::SslServer::SslServer(bool sslEnabled, const QSslConfiguration &config, QObject *parent = nullptr)
    Constructs a \l{SslServer} with the given \a sslEnabled, \a config and \a parent.
*/

/*! \fn void nymeaserver::SslServer::clientConnected(QSslSocket *socket);
    This signal is emitted when a new SSL \a socket connected.
*/

/*! \fn void nymeaserver::SslServer::clientDisconnected(QSslSocket *socket);
    This signal is emitted when a \a socket disconnected.
*/

/*! \fn void nymeaserver::SslServer::dataAvailable(QSslSocket *socket, const QByteArray &data);
    This signal is emitted when \a data from \a socket is available.
*/


/*!
    \class nymeaserver::TcpServer
    \brief This class represents the tcp server for nymead.

    \ingroup server
    \inmodule core

    \inherits TransportInterface

    The TCP server allows clients to connect to the JSON-RPC API.

    \sa WebSocketServer, TransportInterface
*/

#include "tcpserver.h"
#include "nymeacore.h"

#include <QDebug>

namespace nymeaserver {

/*! Constructs a \l{TcpServer} with the given \a configuration, \a sslConfiguration and \a parent.
 *
 *  \sa ServerManager
 */
TcpServer::TcpServer(const ServerConfiguration &configuration, const QSslConfiguration &sslConfiguration, QObject *parent) :
    TransportInterface(configuration, parent),
    m_server(nullptr),
    m_sslConfig(sslConfiguration)
{
    m_avahiService = new QtAvahiService(this);
    connect(m_avahiService, &QtAvahiService::serviceStateChanged, this, &TcpServer::onAvahiServiceStateChanged);
}

/*! Destructor of this \l{TcpServer}. */
TcpServer::~TcpServer()
{
    qCDebug(dcTcpServer()) << "Shutting down \"TCP Server\"" << serverUrl().toString();
    stopServer();
}

/*! Returns the URL of this server. */
QUrl TcpServer::serverUrl() const
{
    return QUrl(QString("%1://%2:%3").arg((configuration().sslEnabled ? "nymeas" : "nymea")).arg(configuration().address.toString()).arg(configuration().port));
}

/*! Sending \a data to a list of \a clients.*/
void TcpServer::sendData(const QList<QUuid> &clients, const QByteArray &data)
{
    foreach (const QUuid &client, clients) {
        sendData(client, data);
    }
}

void TcpServer::terminateClientConnection(const QUuid &clientId)
{
    QTcpSocket *client = m_clientList.value(clientId);
    if (client) {
        client->abort();
    }
}

/*! Sending \a data to the client with the given \a clientId.*/
void TcpServer::sendData(const QUuid &clientId, const QByteArray &data)
{
    QTcpSocket *client = nullptr;
    client = m_clientList.value(clientId);
    if (client) {
        client->write(data + '\n');
    } else {
        qCWarning(dcTcpServer()) << "Client" << clientId.toString() << "unknown to this transport";
    }
}

void TcpServer::onClientConnected(QSslSocket *socket)
{
    QUuid clientId = QUuid::createUuid();
    qCDebug(dcTcpServer()) << "New client connected:" << clientId.toString() << "(Remote address:" << socket->peerAddress().toString() << ")";
    m_clientList.insert(clientId, socket);
    emit clientConnected(clientId);
}

void TcpServer::onClientDisconnected(QSslSocket *socket)
{
    QUuid clientId = m_clientList.key(socket);
    qCDebug(dcTcpServer()) << "Client disconnected:" << clientId.toString() << "(Remote address:" << socket->peerAddress().toString() << ")";
    m_clientList.take(clientId);
    emit clientDisconnected(clientId);
}

void TcpServer::onError(QAbstractSocket::SocketError error)
{
    QTcpServer *server = qobject_cast<QTcpServer *>(sender());
    qCWarning(dcTcpServer) << "Server error on" << server->serverAddress().toString() << ":" << error << server->errorString();
    stopServer();
}

void TcpServer::onDataAvailable(QSslSocket * socket, const QByteArray &data)
{
    qCDebug(dcTcpServerTraffic()) << "Emitting data available";
    QUuid clientId = m_clientList.key(socket);
    emit dataAvailable(clientId, data);
}

void TcpServer::onAvahiServiceStateChanged(const QtAvahiService::QtAvahiServiceState &state)
{
    Q_UNUSED(state)
}

void TcpServer::resetAvahiService()
{
    if (m_avahiService)
        m_avahiService->resetService();

    // Note: reversed order
    QHash<QString, QString> txt;
    txt.insert("jsonrpcVersion", JSON_PROTOCOL_VERSION);
    txt.insert("serverVersion", NYMEA_VERSION_STRING);
    txt.insert("manufacturer", "guh GmbH");
    txt.insert("uuid", NymeaCore::instance()->configuration()->serverUuid().toString());
    txt.insert("name", NymeaCore::instance()->configuration()->serverName());
    txt.insert("sslEnabled", configuration().sslEnabled ? "true" : "false");
    if (!m_avahiService->registerService(QString("nymea-tcp-%1").arg(configuration().id), configuration().address, static_cast<quint16>(configuration().port), "_jsonrpc._tcp", txt)) {
        qCWarning(dcTcpServer()) << "Could not register avahi service for" << configuration();
    }
}


/*! Returns true if this \l{TcpServer} could be reconfigured with the given \a config. */
void TcpServer::reconfigureServer(const ServerConfiguration &config)
{
    if (configuration().address == config.address &&
            configuration().port == config.port &&
            configuration().sslEnabled == config.sslEnabled &&
            configuration().authenticationEnabled == config.authenticationEnabled &&
            m_server->isListening())
        return;

    stopServer();
    setConfiguration(config);
    startServer();
}

/*! Sets the name of this server to the given \a serverName. */
void TcpServer::setServerName(const QString &serverName)
{
    m_serverName = serverName;
    resetAvahiService();
}

/*! Returns true if this \l{TcpServer} started successfully.
 *
 * \sa TransportInterface::startServer()
 */
bool TcpServer::startServer()
{
    m_server = new SslServer(configuration().sslEnabled, m_sslConfig);
    if(!m_server->listen(configuration().address, static_cast<quint16>(configuration().port))) {
        qCWarning(dcTcpServer()) << "Tcp server error: can not listen on" << configuration().address.toString() << configuration().port;
        delete m_server;
        m_server = nullptr;
        return false;
    }

    connect(m_server, SIGNAL(clientConnected(QSslSocket *)), SLOT(onClientConnected(QSslSocket *)));
    connect(m_server, SIGNAL(clientDisconnected(QSslSocket *)), SLOT(onClientDisconnected(QSslSocket *)));
    connect(m_server, &SslServer::dataAvailable, this, &TcpServer::onDataAvailable);

    qCDebug(dcTcpServer()) << "Started Tcp server" << serverUrl().toString();
    resetAvahiService();

    return true;
}

/*! Returns true if this \l{TcpServer} stopped successfully.
 *
 * \sa TransportInterface::startServer()
 */
bool TcpServer::stopServer()
{
    if (m_avahiService)
        m_avahiService->resetService();

    if (!m_server)
        return true;

    m_server->close();
    m_server->deleteLater();
    m_server = nullptr;
    return true;
}

/*! This method will be called if a new \a socketDescriptor is about to connect to this SslSocket. */
void SslServer::incomingConnection(qintptr socketDescriptor)
{
    QSslSocket *sslSocket = new QSslSocket(this);

    qCDebug(dcTcpServer()) << "New client socket connection:" << sslSocket;

    connect(sslSocket, &QSslSocket::encrypted, [this, sslSocket](){ emit clientConnected(sslSocket); });
    connect(sslSocket, &QSslSocket::readyRead, this, &SslServer::onSocketReadyRead);
    connect(sslSocket, &QSslSocket::disconnected, this, &SslServer::onClientDisconnected);

    if (!sslSocket->setSocketDescriptor(socketDescriptor)) {
        qCWarning(dcTcpServer()) << "Failed to set SSL socket descriptor.";
        delete sslSocket;
        return;
    }
    if (m_sslEnabled) {
        sslSocket->setSslConfiguration(m_config);
        sslSocket->startServerEncryption();
    } else {
        emit clientConnected(sslSocket);
    }
}

void SslServer::onClientDisconnected()
{
    QSslSocket *socket = static_cast<QSslSocket*>(sender());
    qCDebug(dcTcpServer()) << "Client socket disconnected:" << socket;
    emit clientDisconnected(socket);
    socket->deleteLater();
}

void SslServer::onSocketReadyRead()
{
    QSslSocket *socket = static_cast<QSslSocket*>(sender());
    QByteArray data = socket->readAll();
    qCDebug(dcTcpServerTraffic()) << "Reading socket data:" << data;
    emit dataAvailable(socket, data);
}

}
