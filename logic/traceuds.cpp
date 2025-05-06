#include "traceuds.h"
#include "util.h"
#include <QDateTime>

TraceUds::TraceUds(QObject *parent) :
	QObject{parent},
	logFilePtr{nullptr}
{

}

void TraceUds::open(const QString &logDirPathRef)
{
	QString logFileName = Util::getFileName() + ".json";
	this->logFilePath = logDirPathRef + "/" + logFileName;
	// Open the log file in append mode
	this->logFilePtr = new QFile(logFilePath);
	if (!this->logFilePtr->open(QIODevice::Append | QIODevice::Text)) {
		Util::log(
			LogType::GenericThrow,
			LogSt::Nok,
			"Failed to open trace file: " + logFilePath
		);
	}
	QString s = "{\"traceEvents\":[\n";
	this->logFilePtr->write(s.toUtf8());
	this->logFilePtr->flush();
}

void TraceUds::close()
{
	if (this->logFilePtr == nullptr) {
		return;
	}
	QString s = "]}\n";
	this->logFilePtr->write(s.toUtf8());
	this->logFilePtr->flush();
	this->logFilePtr->close();
	delete this->logFilePtr;
	this->logFilePtr = nullptr;

	Util::log(LogType::Generic, LogSt::Ok, "Trace file closed: " + this->logFilePath);

	this->logFilePath = "";
}

void TraceUds::writeJsonItem(
	bool isBegin,
	bool isReq,
	uint8_t sid,
	const QString &name,
	const QString &hexStr,
	const QString &detail,
	uint64_t byteIdx
) {
	if(this->logFilePtr == nullptr) {
		return;
	}

	QString formatStr =
		"{\"cat\":\"%1\", "
		"\"pid\":%2, "
		"\"tid\":%3, "
		"\"ts\":%4,"
		"\"ph\":\"%5\", "
		"\"name\":\"%6\", "
		"\"id\":%7, "
		"\"args\": "
		"{\"name\":\"%8\", "
		"\"detail\":\"%9\", "
		"\"hex\": \"%10\", "
		"\"timestamp\": \"%11\""
		"}},\n";

	QString catStr = isReq ? "Reeq" : "Resp";
	QString sidStr = QString("\"0x%1\"").arg(sid, 0, 16, QChar(' '));
	QString isBeginStr = isBegin ? "b" : "e";
	QString byteIdxStr = QString("%1").arg(byteIdx, 0, 10, QChar(' '));
	QString timestampStr =
		QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") +
		QString("_") +
		QString::number(QDateTime::currentMSecsSinceEpoch() % 1000);

	QString s = formatStr
	.arg(catStr) // 1
	.arg(0) // 2
	.arg(0) // 3
	.arg(byteIdxStr) // 4
	.arg(isBeginStr) // 5
	.arg(name) // 6
	.arg(sidStr) // 7
	.arg(name) // 8
	.arg(detail) // 9
	.arg(hexStr) // 10
	.arg(timestampStr) // 11
	;

	this->logFilePtr->write(s.toUtf8());
}

void TraceUds::onUdsPacketReceived(
	bool isReq,
	const QVector<UdsInfo> &packetInfoRef
)
{
	static uint64_t byteIdx = 0;

	// there is nothing to log
	if(packetInfoRef.length() == 0) {
		return;
	}
	if(packetInfoRef[0].hex.length() == 0) {
		return;
	}
	if(this->logFilePtr == nullptr) {
		return;
	}

	uint8_t sid = packetInfoRef[0].hex[0];
	QString name = "Raw";

	if(packetInfoRef.length() >= 2) {
		name = packetInfoRef[1].name;
	}

	if(isReq) {
		name += " Reeq";
	} else {
		name += " Resp";
	}

	writeJsonItem(
		true,
		isReq,
		sid,
		name,
		packetInfoRef[0].getHexStr(),
		name,
		byteIdx
	);

	for(int i = 1; i < packetInfoRef.length(); ++i) {
		uint16_t numOfBytes = packetInfoRef[i].hex.length();

		writeJsonItem(
			true,
			isReq,
			sid,
			packetInfoRef[i].getHexStr(),
			packetInfoRef[i].getHexStr(),
			packetInfoRef[i].name,
			byteIdx
		);
		writeJsonItem(
			false,
			isReq,
			sid,
			packetInfoRef[i].getHexStr(),
			packetInfoRef[i].getHexStr(),
			packetInfoRef[i].name,
			byteIdx + numOfBytes
		);

		writeJsonItem(
			true,
			isReq,
			sid,
			packetInfoRef[i].name,
			packetInfoRef[i].getHexStr(),
			packetInfoRef[i].name,
			byteIdx
		);
		writeJsonItem(
			false,
			isReq,
			sid,
			packetInfoRef[i].name,
			packetInfoRef[i].getHexStr(),
			packetInfoRef[i].name,
			byteIdx + numOfBytes
		);

		byteIdx += numOfBytes;
	}

	writeJsonItem(
		false,
		isReq,
		sid,
		name,
		packetInfoRef[0].getHexStr(),
		name,
		byteIdx
	);

	this->logFilePtr->flush();
}
