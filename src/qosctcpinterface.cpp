#include <QBuffer>
#include <QNetworkDatagram>
#include <QNetworkInterface>

#include "qosctcpinterface.h"

QOscTcpInterface::QOscTcpInterface(QObject *parent) :
		QOscInterface(parent) {
	QObject::connect(&socket, &QAbstractSocket::readyRead, this,
			&QOscTcpInterface::readReady);
	QObject::connect(&socket, &QAbstractSocket::connected, this,
			&QOscTcpInterface::connected);

	// Reconnect logic
	QObject::connect(&reconnectTimer, &QTimer::timeout, this,
			&QOscTcpInterface::rebind);
	reconnectTimer.setInterval(5000);
	reconnectTimer.start();

	QObject::connect(&socket, &QTcpSocket::connected, &reconnectTimer,
			&QTimer::stop);

	QObject::connect(&socket, &QTcpSocket::connected, this, [&] {
		qWarning("Connected to host.");
	});
	QObject::connect(&socket, &QTcpSocket::disconnected, this, [&] {
		qWarning("Disconnected from host.");
		reconnectTimer.start();
	});

	QObject::connect(&socket, &QTcpSocket::errorOccurred, this, [&](QAbstractSocket::SocketError socketError) {
		qWarning() << "Socket Error occurred (" << socketError << "): " << socket.errorString();
	});

	rebind();
}

QOscTcpInterface::~QOscTcpInterface() {}

QString QOscTcpInterface::getRemoteAddress() const {
	return remoteAddr.toString();
}

void QOscTcpInterface::setRemoteAddress(const QString &addr) {
	QHostAddress hostAddr(addr);

	if (hostAddr != remoteAddr) {
		remoteAddr = hostAddr;
		emit remoteAddressChanged(addr);
		rebind();
	}
}

quint16 QOscTcpInterface::getRemotePort() const { return remotePort; }

void QOscTcpInterface::setRemotePort(quint16 p) {
	if (p != remotePort) {
		remotePort = p;
		emit remotePortChanged(p);
		rebind();
	}
}

void QOscTcpInterface::rebind() {
	if (socket.isValid()) {
		socket.disconnectFromHost();

		if (socket.state() != QAbstractSocket::UnconnectedState) {
			socket.waitForDisconnected();
		}
	}

	socket.connectToHost(remoteAddr, remotePort);
}

void QOscTcpInterface::sendData(const QByteArray &data) {
	// Create a buffer to hold the packet's data
	QByteArray frame;
	frame.reserve(data.size() + 2);

	// Write the start byte
	frame.append(char(0xC0));

	// Write the data itself
	for (char byte : data) {
		switch (static_cast<unsigned char>(byte)) {
			// Escape 0xC0 and 0xDB bytes since those mean things
			case 0xC0:
				frame.append(char(0xDB));
				frame.append(char(0xDC));
				break;

			case 0xDB:
				frame.append(char(0xDB));
				frame.append(char(0xDD));
				break;

			default:
				frame.append(byte);
				break;
		}
	}

	// Write the end byte
	frame.append(char(0xC0));

	socket.write(frame);
	emit messageSent();
}

void QOscTcpInterface::readReady() {
	QByteArray byteDataToRead = socket.peek(socket.bytesAvailable());

	// Iterate through the data, find packets, and do things with them
	bool packetStarted = false;
	qint64 startIndex = 0;
	qint64 endIndex = 0;
	for (qint64 i = 0; i < byteDataToRead.length(); i++) {
		bool byteFound =
				QByteArrayView(byteDataToRead).slice(i, 1).contains('\xC0');
		if (byteFound) {
			// In the case that a zero-byte packet is found, this usually means
			// that the end of a packet was recieved but the start was missed,
			// so this just resets things when that happens.
			if (startIndex == i - 1) {
				packetStarted = false;
			}

			if (!packetStarted) {
				packetStarted = true;
				startIndex = i;
			} else {
				packetStarted = false;
				endIndex = i;

				QByteArray data =
						QByteArray(QByteArrayView(byteDataToRead)
										.slice(startIndex + 1, endIndex - startIndex - 1));

				switch (QOsc::detectType(data)) {
					case QOsc::OscMessage: {
						auto msg = QOscMessage::read(data);
						if (msg.isValid()) {
							processMessage(msg, socket.peerAddress());
							emit messageReceived(msg, socket.peerAddress());
						}
						break;
					}

					case QOsc::OscBundle: {
						auto bundle = QOscBundle::read(data);
						if (bundle.isValid()) {
							processBundle(bundle, socket.peerAddress());
							emit bundleReceived(bundle, socket.peerAddress());
						}
						break;
					}

					default:
						break;
				}
			}
		}
	}

	// Discard the data that has already been processed
	socket.skip(endIndex);
}
