#include "traceuds.h"
#include "util.h"
#include <QDateTime>

const QByteArray TraceUds::htmlHeader = QByteArrayLiteral(R"(<!DOCTYPE html>
<html lang="en">
<head>
	<meta charset="UTF-8">
	<title>UDS Trace Report</title>
	<style>
		body {
			margin: 0;
			padding: 0;
			min-height: 100vh;
			background: linear-gradient(135deg, #e0eafc 0%, #cfdef3 100%);
			font-family: 'Segoe UI', 'Courier New', Courier, monospace;
		}
		.list-widget {
			max-width: 600px;
			margin: 40px auto;
			background: rgba(255,255,255,0.9);
			border-radius: 12px;
			box-shadow: 0 8px 32px rgba(31,38,135,0.18);
			padding: 24px;
		}
		.list-widget h3 {
			margin: 0 0 24px 0;
			font-size: 2em;
			color: #2d3a4b;
			text-align: center;
		}
		.list-widget ul {
			list-style: none;
			padding: 0;
			margin: 0;
		}
		.list-widget li {
			margin-bottom: 12px;
			padding: 14px 18px;
			border-radius: 8px;
			background: #f5faff;
			color: #2d3a4b;
			font-size: 1em;
			display: flex;
			align-items: flex-start;
		}
		.list-widget li.request {
			background: #cce6ff;
			font-weight: bold;
		}
		.list-widget li.response {
			background: #baffc9;
			font-weight: bold;
			text-align: right;
			justify-content: flex-end;
		}
		.list-widget li:last-child {
			margin-bottom: 0;
		}
		.list-widget li div {
			white-space: pre-line;
		}
	</style>
</head>
<body>

<div class="list-widget">
	<h3>Uds Packet Report</h3>
	<ul>)");


const QByteArray TraceUds::htmlFooter = QByteArrayLiteral(R"(</ul>
</div>

</body>
</html>

)");

TraceUds::TraceUds(QObject *parent) :
	QObject{parent},
	logFilePtr{nullptr},
	htmlFilePtr{nullptr}
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

	logFileName = Util::getFileName() + ".html";
	this->htmlFilePath = logDirPathRef + "/" + logFileName;
	this->htmlFilePtr = new QFile(htmlFilePath);
	if (!this->htmlFilePtr->open(QIODevice::WriteOnly | QIODevice::Text)) {
		Util::log(
			LogType::GenericThrow,
			LogSt::Nok,
			"Failed to open HTML trace file: " + htmlFilePath
		);
	}
	this->htmlFilePtr->write(htmlHeader);
	this->htmlFilePtr->flush();
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

	if (this->htmlFilePtr == nullptr) {
		return;
	}
	this->htmlFilePtr->write(htmlFooter);
	this->htmlFilePtr->flush();
	this->htmlFilePtr->close();
	delete this->htmlFilePtr;
	this->htmlFilePtr = nullptr;

	Util::log(LogType::Generic, LogSt::Ok, "HTML trace file closed: " + this->htmlFilePath);
	this->htmlFilePath = "";
}

void TraceUds::writeJsonItem(
	bool isBegin,
	bool isReq,
	uint8_t sid,
	const QString &name,
	const QString &hexStr,
	const QString &detail,
	uint64_t byteIdx,
	const QString &rawStrRef
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
		"\"timestamp\": \"%11\", "
		"\"raw\": \"%12\""
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
	.arg(rawStrRef) // 12
	;

	this->logFilePtr->write(s.toUtf8());
}

void TraceUds::onUdsPacketReceived(
	bool isReq,
	const QString &rawCanMsgStrRef,
	const QVector<UdsInfo> &packetInfoRef
) {
	jsonUdsPacketHandler(
		isReq,
		rawCanMsgStrRef,
		packetInfoRef
	);
	htmlUdsPacketHandler(
		isReq,
		rawCanMsgStrRef,
		packetInfoRef
	);
}

void TraceUds::addHtmlTrace(bool isReq, QString s)
{
	QString type;

	if(isReq) {
		type = "request";
	} else {
		type = "response";
	}

	QString traceStr =
		"<li class=\"" +
		type +
		"\"><div>" +
		s +
		"</div></li>\n";

	if (this->htmlFilePtr == nullptr) {
		return;
	}
	this->htmlFilePtr->write(traceStr.toUtf8());
}

void TraceUds::htmlUdsPacketHandler(
	bool isReq,
	const QString &rawCanMsgStrRef,
	const QVector<UdsInfo> &packetInfoRef
)
{
	(void)rawCanMsgStrRef;
	if(isReq) {
		QString s = "";
		for(const UdsInfo &info : packetInfoRef) {
			for(uint32_t i = 0; i < (info.hexIdx * 3); ++i) {
				s += "&nbsp;";
			}
			s += info.getHexStr(8) + "<br>";
			if(info.name != "") {
				s += info.name + "<br>";
			}
		}
		s = s.trimmed();
		addHtmlTrace(isReq, s);
	} else {
		uint32_t packetHexStrLen = 0;
		QString s = "";
		if(packetInfoRef.length() != 0) {
			s = packetInfoRef[0].getHexStr(8).trimmed();
		}
		packetHexStrLen = s.length();

		for(int i = 1; i < packetInfoRef.length(); ++i) {
			QString hexStr = packetInfoRef[i].getHexStr(8).trimmed();
			QString paddedHexStr = hexStr;
			int paddingLen = packetHexStrLen - (packetInfoRef[i].hexIdx * 3) - hexStr.length();
			s += "<br>";
			for(int j = 0; j < paddingLen; j++) {
				paddedHexStr += "&nbsp;";
			}
			if (paddedHexStr.endsWith("&nbsp;")) {
				paddedHexStr.chop(1);
			}
			s += paddedHexStr + "<br>";
			s += packetInfoRef[i].name;
		}

		addHtmlTrace(isReq, s);
	}
}

void TraceUds::jsonUdsPacketHandler(
	bool isReq,
	const QString &rawCanMsgStrRef,
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
		byteIdx,
		rawCanMsgStrRef
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
			byteIdx,
			rawCanMsgStrRef
		);
		writeJsonItem(
			false,
			isReq,
			sid,
			packetInfoRef[i].getHexStr(),
			packetInfoRef[i].getHexStr(),
			packetInfoRef[i].name,
			byteIdx + numOfBytes,
			rawCanMsgStrRef
		);

		writeJsonItem(
			true,
			isReq,
			sid,
			packetInfoRef[i].name,
			packetInfoRef[i].getHexStr(),
			packetInfoRef[i].name,
			byteIdx,
			rawCanMsgStrRef
		);
		writeJsonItem(
			false,
			isReq,
			sid,
			packetInfoRef[i].name,
			packetInfoRef[i].getHexStr(),
			packetInfoRef[i].name,
			byteIdx + numOfBytes,
			rawCanMsgStrRef
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
		byteIdx,
		rawCanMsgStrRef
	);

	this->logFilePtr->flush();
}
