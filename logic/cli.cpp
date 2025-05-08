#include <QCoreApplication>
#include <QDebug>
#include <QCommandLineParser>
#include <QDir>
#include <QDateTime>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
#include <QJsonValue>
#include <iostream>
#include "util.h"
#include "cli.h"
#include "cobs.h"

Cli::Cli(QObject *parent) :
	QObject(parent),
	name("UdsTracerCli"),
	version("1.0.0"),
	uds(),
	reqCanId(0),
	respCanId(0),
	reqRawCanIsoTp(),
	respRawCanIsoTp(),
	traceUds(),
	isCanConnected(false),
	canLogFilePtr(nullptr),
	canLogFilePath(""),
	cmd()
{
	zeroOutIsoTp();
	connect(&this->cmd, &Cmd::configAllLoaded, this, &Cli::configAllLoaded);
	connect(&this->cmd, &Cmd::canEventOccured, this, &Cli::onCanEventOccured);
	connect(this, &Cli::canMsgReceived, this, &Cli::onCanMsgReceived);
	connect(
		this,
		&Cli::udsPacketReceived,
		&this->traceUds,
		&TraceUds::onUdsPacketReceived
	);
}

void Cli::init(int argc, char **argvPtrPtr)
{
	handleCliArgs(argc, argvPtrPtr);
}

void Cli::openCanLogFile(const QString &logDirPathRef)
{
	if (this->canLogFilePtr != nullptr) {
		Util::log(LogType::Generic, LogSt::Warn, "Trace file closed: " + this->canLogFilePath);
		this->canLogFilePtr->close();
		delete canLogFilePtr;
	}

	if(QDir(logDirPathRef).exists()) {
		this->canLogFilePath = logDirPathRef + "/" + Util::getFileName() + ".cobs";
		this->canLogFilePtr = new QFile(this->canLogFilePath);
		if (this->canLogFilePtr->open(QIODevice::WriteOnly)) {
			Util::log(LogType::Generic, LogSt::Ok, "CAN log file opened: " + this->canLogFilePath);
		} else {
			Util::log(LogType::CmdRespThrow, LogSt::Nok, "Failed to open CAN log file: " + this->canLogFilePath);
			delete this->canLogFilePtr;
			this->canLogFilePtr = nullptr;
		}
	} else {
		Util::log(LogType::GenericThrow, LogSt::Nok, "Log directory does not exist: " + logDirPathRef);
	}
}

void Cli::zeroOutIsoTp()
{
	memset(
		(void *)&this->reqIsoTp,
		0,
		sizeof(this->reqIsoTp)
	);
	memset(
		(void *)&this->respIsoTp,
		0,
		sizeof(this->respIsoTp)
	);
	memset(
		(void *)&this->reqSendBfrArr,
		0,
		sizeof(this->reqSendBfrArr)
	);
	memset(
		(void *)&this->reqRecvBfrArr,
		0,
		sizeof(this->reqRecvBfrArr)
	);
	memset(
		(void *)&this->respSendBfrArr,
		0,
		sizeof(this->respSendBfrArr)
	);
	memset(
		(void *)&this->respRecvBfrArr,
		0,
		sizeof(this->respRecvBfrArr)
	);
}

void Cli::onCanEventOccured(CanEvent event)
{
	switch(event) {
	case CanEvent::Connected:
		emit canConnectionEvented(true);
		{
			const ConfigAll &cfgAll = this->cmd.getConfigAll();
			openCanLogFile(cfgAll.tracer.getLogDirPath());
			this->traceUds.open(cfgAll.tracer.getLogDirPath());

			zeroOutIsoTp();

			this->reqCanId = static_cast<uint32_t>(cfgAll.tracer.getReqIdHex().toUInt(nullptr, 16));
			this->respCanId = static_cast<uint32_t>(cfgAll.tracer.getRespIdHex().toUInt(nullptr, 16));

			this->reqIsoTp.init(
				0x12, // not important we are not going to send anything
				this->reqSendBfrArr,
				sizeof(this->reqSendBfrArr),
				this->reqRecvBfrArr,
				sizeof(this->reqRecvBfrArr)
			);
			this->respIsoTp.init(
				0x13, // not important we are not going to send anything
				this->respSendBfrArr,
				sizeof(this->respSendBfrArr),
				this->respRecvBfrArr,
				sizeof(this->respRecvBfrArr)
			);


			Util::log(LogType::Generic, LogSt::Ok, "ISOTP handles initialized successfully.");
		}
		break;
	case CanEvent::Disconnected:
		emit canConnectionEvented(false);

		this->traceUds.close();

		if(this->canLogFilePtr != nullptr) {
			Util::log(LogType::Generic, LogSt::Ok, "Can log file closed: " + this->canLogFilePath);
			this->canLogFilePtr->close();
			delete this->canLogFilePtr;
			this->canLogFilePtr = nullptr;
			this->canLogFilePath = "";
		}

		break;
	case CanEvent::MessageReceived:
		Can *canIntPtr = this->cmd.getCanInterface();
		while(!canIntPtr->rxQueue.isEmpty()) {
			CanMsg canMsg = canIntPtr->rxQueue.dequeue();
			emit canMsgReceived(canMsg);
		}
		break;
	}
}

void Cli::loadCommands(const QString &filePathRef)
{
	if (!filePathRef.isEmpty()) {
		QFile file(filePathRef);
		if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
			QTextStream in(&file);
			QJsonParseError parseError;
			QString content = in.readAll();
			content = content.trimmed();
			QJsonDocument jsonDoc = QJsonDocument::fromJson(content.toUtf8(), &parseError);

			if (parseError.error != QJsonParseError::NoError) {
				Util::log(LogType::GenericThrow, LogSt::Nok, "JSON parse error: " + parseError.errorString()
				);
				return;
			}

			if (!jsonDoc.isArray()) {
				Util::log(LogType::GenericThrow, LogSt::Nok, "Expected a JSON array in the file.");
				return;
			}

			QJsonArray jsonArray = jsonDoc.array();
			for (const QJsonValue &value : jsonArray) {
				if (value.isObject()) {
					QJsonObject jsonObj = value.toObject();
					if (jsonObj.isEmpty()) {
						Util::log(LogType::Generic, LogSt::Warn, "Empty JSON object in array.");
						continue;
					}
					QString jsonString = QString(QJsonDocument(jsonObj).toJson(QJsonDocument::Compact));
					Util::log(LogType::Generic, LogSt::Ok, "Loaded JSON object: " + jsonString);
					QMap<QString, QString> cmdMap = Util::fromCmdToMap(jsonString);
					commandMap(cmdMap);
				} else {
					Util::log(LogType::GenericThrow, LogSt::Nok, "Invalid JSON object in array.");
				}
			}
			file.close();
		} else {
			Util::log(LogType::GenericThrow, LogSt::Nok, "Could not open file for reading: " + filePathRef);
		}
	} else {
		Util::log(LogType::GenericThrow, LogSt::Nok, "No file name provided for loading commands.");
	}
}

void Cli::showCommand(void)
{
	for(const QString &key : CmdDef::allCommands.keys()) {
		const CmdDef::Cmd &cmdRef = *CmdDef::allCommands[key];
		qDebug() << key.leftJustified(11, ' ') << CmdDef::valueTypeNames[cmdRef.valueType];
	}
	std::exit(0);
}

void Cli::handleCliArgs(int argc, char **argvPtrPtr)
{
	QCommandLineParser parser;
	parser.setApplicationDescription(this->name);
	parser.addHelpOption();
	parser.addVersionOption();

	QCommandLineOption loadCommandsOption(QStringList() << "l" << "loadCommands",
			"Load commands from a specified file.", "file");
	parser.addOption(loadCommandsOption);

	QCommandLineOption cliOption(QStringList() << "c" << "cli",
			"Cli Mode");
	parser.addOption(cliOption);

	QCommandLineOption showOption(QStringList() << "s" << "show",
			"Show commands");
	parser.addOption(showOption);

	QStringList arguments;
	for (int i = 0; i < argc; ++i) {
		arguments << QString::fromUtf8(argvPtrPtr[i]);
	}
	parser.process(arguments);

	if (parser.isSet(loadCommandsOption)) {
		QString fileName = parser.value(loadCommandsOption);
		loadCommands(fileName);
	}

	if (parser.isSet(cliOption)) {
		this->libMode = false;
		QThread* inputThread = createInputThread();
		inputThread->start();
	} else {
		this->libMode = true;
	}

	if (parser.isSet(showOption)) {
		showCommand();
	}
}

void Cli::commandMap(const QMap<QString, QString> &cmdMapRef)
{
	emit commandReceived(Util::fromMapToCmd(cmdMapRef));
	QMap<QString, QString> permittedCmds;

	for(const QString &keyRef : cmdMapRef.keys()) {
		if(CmdDef::isExecPermitted(keyRef, cmdMapRef[keyRef], this->isCanConnected)) {
			permittedCmds.insert(keyRef, cmdMapRef[keyRef]);
		}
	}

	this->cmd.commandMap(permittedCmds);
}

void Cli::commandMapWThrow(const QMap<QString, QString> &cmdMapRef)
{
	emit commandReceived(Util::fromMapToCmd(cmdMapRef));
	QMap<QString, QString> permittedCmds;

	for(const QString &keyRef : cmdMapRef.keys()) {
		if(CmdDef::isExecPermitted(keyRef, cmdMapRef[keyRef], this->isCanConnected)) {
			permittedCmds.insert(keyRef, cmdMapRef[keyRef]);
		}
	}
	this->cmd.commandMapWThrow(permittedCmds);
}

void Cli::onCanMsgReceived(const CanMsg &canMsgRef)
{
	uint8_t localArr[0xffff] = {0};
	uint16_t outSize = 0;
	IsoTpRet isoTpRet = IsoTpRet::OK;
	QVector<uint8_t> localVector;

	QString s = Can::getMsgStr(canMsgRef);

	if(this->canLogFilePtr != nullptr) {
		uint8_t rawCanMsg[sizeof(CanMsg)];
		char c = 0;
		size_t rawCanMsgSize = Can::getRawCanMsg(canMsgRef, rawCanMsg, sizeof(rawCanMsg));
		uint8_t encodedRaw[sizeof(CanMsg) * 2];
		cobs_encode_result result = cobs_encode(encodedRaw, sizeof(encodedRaw), rawCanMsg, rawCanMsgSize);
		if (result.out_len > 0) {
			this->canLogFilePtr->write(reinterpret_cast<const char *>(encodedRaw), result.out_len);
			this->canLogFilePtr->flush();
		}
		this->canLogFilePtr->write(reinterpret_cast<const char *>(&c), 1);
	}

	if(canMsgRef.id == this->reqCanId) {
		this->reqRawCanIsoTp.append(canMsgRef);
		this->reqIsoTp.on_can_message(canMsgRef.data, canMsgRef.dataLength);
	} else if(canMsgRef.id == this->respCanId) {
		this->respRawCanIsoTp.append(canMsgRef);
		this->respIsoTp.on_can_message(canMsgRef.data, canMsgRef.dataLength);
	}

	for(int i = 0; i < 10; ++i) {
		this->reqIsoTp.poll();
		outSize = 0;
		isoTpRet = this->reqIsoTp.receive(localArr, (uint16_t)sizeof(localArr), &outSize);

		if(isoTpRet == IsoTpRet::OK) {
			localVector.clear();
			for(int i = 0; i < (int)outSize; ++i) {
				localVector.append(localArr[i]);
			}
			udsReqMsg(localVector);
			this->reqRawCanIsoTp.clear();
		}

		this->respIsoTp.poll();
		outSize = 0;
		isoTpRet = this->respIsoTp.receive(localArr, (uint16_t)sizeof(localArr), &outSize);

		if(isoTpRet == IsoTpRet::OK) {
			localVector.clear();
			for(int i = 0; i < (int)outSize; ++i) {
				localVector.append(localArr[i]);
			}
			udsRespMsg(localVector);
			this->respRawCanIsoTp.clear();
		}
	}
}

void Cli::udsReqMsg(const QVector<uint8_t> &data)
{
	if(data.length() == 0) {
		return;
	}
	QVector<UdsInfo> packetInfo;
	QString s = "";
	
	this->uds.getReqInfo(data, packetInfo);
	
	for(int i = 0; i < this->reqRawCanIsoTp.length() && i < 2; ++i) {
		s += Can::getMsgStr(this->reqRawCanIsoTp[i]) + "\\n";
	}
	s = this->reqRawCanIsoTp.length() > 2 ? (s + "\\n...") : s;

	// printing is too slow, comment out if you really need it
	//Util::log(
	//	LogType::CanMsg,
	//	LogSt::Ok,
	//	s
	//);
	Util::log(
		LogType::UdsReqMsg,
		LogSt::Ok,
		packetInfo[0].getHexStr(10)
	);
	emit udsPacketReceived(true, s, packetInfo);
}
void Cli::udsRespMsg(const QVector<uint8_t> &data)
{
	if(data.length() == 0) {
		return;
	}
	QVector<UdsInfo> packetInfo;
	QString s = "";
	this->uds.getRespInfo(data, packetInfo);
	for(int i = 0; i < this->respRawCanIsoTp.length() && i < 2; ++i) {
		s += Can::getMsgStr(this->respRawCanIsoTp[i]) + "\\n";
	}
	s = this->respRawCanIsoTp.length() > 2 ? (s + "\\n...") : s;
	// printing is too slow, comment out if you really need it
	//Util::log(
	//	LogType::CanMsg,
	//	LogSt::Ok,
	//	s
	//);
	Util::log(
		LogType::UdsRespMsg,
		LogSt::Ok,
		packetInfo[0].getHexStr(10)
	);
	emit udsPacketReceived(false, s, packetInfo);
}

QThread* Cli::createInputThread(void)
{
	QThread* inputThread = QThread::create([this]() {
		while (true) {
			QString input;
			std::cout << "Enter command: ";
			QTextStream(stdin).readLineInto(&input);

			commandMap(Util::fromCmdToMap(input));
		}
	});
	return inputThread;
}
