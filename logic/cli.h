#ifndef CLI_H
#define CLI_H

#include <QObject>
#include <QString>
#include <QThread>
#include <QFile>
#include <QElapsedTimer>
#include "cmd.h"
#include "isotp.hpp"
#include "uds.h"
#include "traceuds.h"

class Cli : public QObject
{
	Q_OBJECT
public:
	const QString name;
	const QString version;
	Cli(QObject *parent);
	void init(int argc, char **argvPtrPtr);
signals:
	void commandReceived(const QString &cmdStrRef);
	void configAllLoaded(const ConfigAll &cfgAllRef);
	void canMsgReceived(const CanMsg &canMsgRef);
	void udsMsgReceived(bool isReq, const QVector<uint8_t> &udsMsgRef);
	void canConnectionEvented(bool isConnected);
	void udsPacketReceived(
		bool isReq,
		const QString &rawCanMsgStrRef,
		const QVector<UdsInfo> &packetInfoRef
	);
public slots:
	void commandMap(const QMap<QString, QString> &cmdMapRef);
	void commandMapWThrow(const QMap<QString, QString> &cmdMapRef);
	void onCanMsgReceived(const CanMsg &canMsgRef);
private:
	Uds uds;
	IsoTp reqIsoTp;
	IsoTp respIsoTp;
	uint8_t reqSendBfrArr[16];
	uint8_t reqRecvBfrArr[10240];
	uint8_t respSendBfrArr[16];
	uint8_t respRecvBfrArr[10240];
	uint32_t reqCanId;
	uint32_t respCanId;
	QVector<CanMsg> reqRawCanIsoTp;
	QVector<CanMsg> respRawCanIsoTp;
	TraceUds traceUds;
	bool isCanConnected;
	bool libMode;
	QFile *canLogFilePtr;
	QString canLogFilePath;
	Cmd cmd;
	void udsReqMsg(const QVector<uint8_t> &data);
	void udsRespMsg(const QVector<uint8_t> &data);
	QThread* createInputThread(void);
	void handleCliArgs(int argc, char **argvPtrPtr);
	void openCanLogFile(const QString &logDirPathRef);
	void loadCommands(const QString &filePathRef);
	void zeroOutIsoTp(void);
	void showCommand(void);
private slots:
	void onCanEventOccured(CanEvent event);
};

#endif // CLI_H
