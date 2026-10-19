#include <QBuffer>
#include <QNetworkDatagram>
#include <QNetworkInterface>

#include "qosctcpinterface.h"

QOscTcpInterface::QOscTcpInterface(QObject *parent) :
		QOscInterface(parent) {
	// Set up processing
	QObject::connect(&socket, &QAbstractSocket::readyRead, this, &QOscTcpInterface::readReady);

	// Set up some basic signals
	QObject::connect(&socket, &QAbstractSocket::connected, this, &QOscTcpInterface::connected);
	QObject::connect(&socket, &QAbstractSocket::disconnected, this, &QOscTcpInterface::disconnected);

	QObject::connect(&socket, &QAbstractSocket::errorOccurred, this, [this](QAbstractSocket::SocketError socketError) {
		qWarning() << "Socket Error occurred (" << socketError << "): " << socket.errorString();
	});

	QObject::connect(&socket, &QAbstractSocket::connected, this, [] {
		qWarning("Connected to host.");
	});
	QObject::connect(&socket, &QAbstractSocket::disconnected, this, [] {
		qWarning("Disconnected from host.");
	});

	// Reconnect logic
	QObject::connect(&reconnectTimer, &QTimer::timeout, this, &QOscTcpInterface::rebind);
	reconnectTimer.setInterval(5000);

	QObject::connect(&socket, &QAbstractSocket::connected, &reconnectTimer, &QTimer::stop);
	QObject::connect(&socket, &QAbstractSocket::disconnected, this, [this] {
		rebind();
		reconnectTimer.start();
	});

	// Clear buffers on disconnect
	QObject::connect(&socket, &QAbstractSocket::disconnected, this, [this] {
		frameBuffer.clear();
		packetStarted = false;
		trailingEscape = false;
	});
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
	}
}

quint16 QOscTcpInterface::getRemotePort() const { return remotePort; }

void QOscTcpInterface::setRemotePort(quint16 p) {
	if (p != remotePort) {
		remotePort = p;
		emit remotePortChanged(p);
	}
}

void QOscTcpInterface::rebind() {
	qWarning("Rebinding interface.");

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
	frame.append(SlipEnd);

	// Write the data itself
	for (char byte : data) {
		switch (byte) {
			// Escape End and Esc bytes since those mean things
			case SlipEnd:
				frame.append(SlipEsc);
				frame.append(SlipEscEnd);
				break;

			case SlipEsc:
				frame.append(SlipEsc);
				frame.append(SlipEscEsc);
				break;

			default:
				frame.append(byte);
				break;
		}
	}

	// Write the end byte
	frame.append(SlipEnd);

	socket.write(frame);
	emit messageSent();
}

void QOscTcpInterface::readReady() {
	QByteArray byteDataToRead = socket.read(socket.bytesAvailable());

	if (trailingEscape) {
		byteDataToRead.prepend(SlipEsc);
		trailingEscape = false;
	}

	// Iterate through the data, find packets, and do things with them
	for (qsizetype i = 0; i < byteDataToRead.size(); ++i) {
		char byte = byteDataToRead.at(i);
		if (packetStarted) {
			switch (byte) {
				case SlipEnd: {
					// In the case that a zero-byte packet is found, this usually means that the
					// end of a packet was recieved but the start was missed, so this just resets
					// things when that happens.
					if (frameBuffer.isEmpty()) {
						frameBuffer.resize(0);

						break;
					}

					switch (QOsc::detectType(frameBuffer)) {
						case QOsc::OscMessage: {
							auto msg = QOscMessage::read(frameBuffer);
							if (msg.isValid()) {
								processMessage(msg, socket.peerAddress());
								emit messageReceived(msg, socket.peerAddress());
							}
							break;
						}

						case QOsc::OscBundle: {
							auto bundle = QOscBundle::read(frameBuffer);
							if (bundle.isValid()) {
								processBundle(bundle, socket.peerAddress());
								emit bundleReceived(bundle, socket.peerAddress());
							}
							break;
						}

						default:
							break;
					}

					frameBuffer.resize(0);
					packetStarted = false;

					break;
				}

				case SlipEsc: {
					// Found an escape character, we need to figure out what to do with it and then
					// update the frameBuffer accordingly

					if (i == byteDataToRead.size() - 1) {
						// If we have a trailing escape character, set a flag so we can come back
						// to it once we have more data
						trailingEscape = true;
						break;
					}

					char escapedByte = byteDataToRead.at(i + 1);

					switch (escapedByte) {
						case SlipEscEnd:
							frameBuffer.append(SlipEnd);
							break;

						case SlipEscEsc:
							frameBuffer.append(SlipEsc);
							break;
					}

					// Skip the next character since it was escaped
					++i;
					break;
				}

				default: {
					frameBuffer.append(byte);
					break;
				}
			}
		} else if (byte == SlipEnd) {
			// Found a SlipEnd byte, we should start reading data now
			packetStarted = true;
		}
	}
}
