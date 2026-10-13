#ifndef QOSCTCPINTERFACE_H
#define QOSCTCPINTERFACE_H

#include "qoscinterface.h"
#include <QTcpSocket>
#include <QTimer>

class QOSC_EXPORT QOscTcpInterface :  public QOscInterface
{
	Q_OBJECT
	
	Q_PROPERTY(QString remoteAddr READ getRemoteAddress WRITE setRemoteAddress NOTIFY remoteAddressChanged)
	Q_PROPERTY(quint16 remotePort READ getRemotePort WRITE setRemotePort NOTIFY remotePortChanged)
	
public:
	QOscTcpInterface(QObject* parent = nullptr);
	~QOscTcpInterface() override;
	
	QString getRemoteAddress() const;
	void setRemoteAddress(const QString& addr);
	
	quint16 getRemotePort() const;
	void setRemotePort(quint16 p);
	
signals:
	void remoteAddressChanged(const QString& addr);
	void remotePortChanged(quint16 port);
	
	void connected();
	void disconnected();

protected:
	void sendData(const QByteArray& data) override;
	
private:
	static const char SlipEnd = '\xC0';
	static const char SlipEsc = '\xDB';
	static const char SlipEscEnd = '\xDC';
	static const char SlipEscEsc = '\xDD';

	void rebind();

	QHostAddress remoteAddr = QHostAddress("127.0.0.1");
	quint16      remotePort = 0;
	QTcpSocket   socket;
	QTimer reconnectTimer;

	QByteArray frameBuffer;
	bool packetStarted = false;
	bool trailingEscape = false;

private slots:
	void readReady();
};

#endif // QOSCUDPINTERFACE_H


