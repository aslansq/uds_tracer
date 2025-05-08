/**
 * @defgroup traceuds_h
 * @{
 * @file traceuds.h
 * @brief This is the main way to trace UDS packets.
 * It is used to log UDS packets in JSON format. JSON format is supported by Perfetto.
 */

#ifndef TRACEUDS_H
#define TRACEUDS_H

#include <QObject>
#include <QFile>
#include "uds.h"

class TraceUds : public QObject
{
	Q_OBJECT
public:
	explicit TraceUds(QObject *parent = nullptr);
	void open(const QString &logDirPathRef);
	void close();
public slots:
	void onUdsPacketReceived(
		bool isReq,
		const QString &rawCanMsgStrRef,
		const QVector<UdsInfo> &packetInfoRef
	);
private:
	QFile *logFilePtr;
	QString logFilePath;
	void writeJsonItem(
		bool isBegin,
		bool isReq,
		uint8_t sid,
		const QString &name,
		const QString &hexStr,
		const QString &detail,
		uint64_t byteIdx,
		const QString &rawStrRef
	);
};

#endif // TRACEUDS_H

/// @}
