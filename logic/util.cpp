#include "util.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <iostream>

const QMap<LogProp, QString> LogPropStr = {
	{LogProp::Timestamp , "Timestamp"},
	{LogProp::Type, "Type"},
	{LogProp::Status, "Status"},
	{LogProp::Command, "Command"},
	{LogProp::CommandValue, "CommandValue"},
	{LogProp::Message, "Message"}
};

const QMap<LogType, QString> LogTypeStr = {
	{LogType::CmdReq, "CmdReq"},
	{LogType::CmdResp, "CmdResp"},
	{LogType::Generic, "Generic"},
	{LogType::CanMsg, "CanMsg"},
	{LogType::UdsReqMsg, "UdsReqMsg"},
	{LogType::UdsRespMsg, "UdsRespMsg"},
	{LogType::CmdReqThrow, "CmdReqThrow"},
	{LogType::CmdRespThrow, "CmdRespThrow"},
	{LogType::GenericThrow, "GenericThrow"}
};

const QMap<LogSt, QString> LogStStr = {
	{LogSt::Ok, "Ok"},
	{LogSt::Nok, "Nok"},
	{LogSt::Warn, "Warn"},
	{LogSt::Unknown, "Unknown"}
};

namespace Util {
	class Log {
	public:
		Log(LogType type) :
			type(type)
		{}
		void add(LogProp prop, const QString &s) {
			this->jsonObj[LogPropStr[prop]] = s;
		}
		void print() {
			switch(type) {
			case LogType::CmdReqThrow:
			case LogType::CmdRespThrow:
			case LogType::GenericThrow:
				throw std::runtime_error(toStr().toUtf8().constData());
				break;
			default:
				std::cout << toStr().toStdString() << std::endl;
				break;
			}
		}
	private:
		QJsonObject jsonObj;
		LogType type;
		QString toStr(void) {
			return QJsonDocument(this->jsonObj).toJson() + ",";
		}
	};

	uint64_t startTime = QDateTime::currentMSecsSinceEpoch();

	QString fromMapToCmd(const QMap<QString, QString> &mapRef)
	{
		QString retStr = "";

		QJsonObject jsonObj;
		for (const QString &keyRef : mapRef.keys()) {
			jsonObj[keyRef] = mapRef[keyRef];
		}
		QJsonDocument jsonDoc(jsonObj);
		retStr = jsonDoc.toJson(QJsonDocument::Compact);

		retStr.replace(",", ", ");

		return retStr;
	}

	QMap<QString, QString> fromCmdToMap(const QString &jsonStrRef)
	{
		QMap<QString, QString> map;

		QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonStrRef.toUtf8());
		if (!jsonDoc.isObject()) {
			return map; // Return empty map if JSON is invalid or not an object
		}

		QJsonObject jsonObj = jsonDoc.object();
		for (auto it = jsonObj.begin(); it != jsonObj.end(); ++it) {
			map[it.key()] = it.value().toString();
		}

		return map;
	}

	void log(LogType type, const QString &msgRef)
	{
		Log logObj(type);
		logObj.add(LogProp::Message, msgRef);
		logObj.print();
	}

	void log(LogType type, LogSt st, const CmdDef::Cmd &cmdRef, const QString &cmdValueRef, const QString &msgRef)
	{
		Log logObj(type);
		logObj.add(LogProp::Timestamp, QString::number(getTimeStamp()));
		logObj.add(LogProp::Type, LogTypeStr[type]);
		logObj.add(LogProp::Status, LogStStr[st]);
		logObj.add(LogProp::Command, cmdRef.name);
		logObj.add(LogProp::CommandValue, cmdValueRef);
		logObj.add(LogProp::Message, msgRef);
		logObj.print();

	}
	void log(LogType type, LogSt st, const CmdDef::Cmd &cmdRef, const QString &msgRef)
	{
		Log logObj(type);
		logObj.add(LogProp::Timestamp, QString::number(getTimeStamp()));
		logObj.add(LogProp::Type, LogTypeStr[type]);
		logObj.add(LogProp::Status, LogStStr[st]);
		logObj.add(LogProp::Command, cmdRef.name);
		logObj.add(LogProp::Message, msgRef);
		logObj.print();
	}
	void log(LogType type, LogSt st, const QString &msgRef)
	{
		Log logObj(type);
		logObj.add(LogProp::Timestamp, QString::number(getTimeStamp()));
		logObj.add(LogProp::Type, LogTypeStr[type]);
		logObj.add(LogProp::Status, LogStStr[st]);
		logObj.add(LogProp::Message, msgRef);
		logObj.print();
	}

	uint64_t getTimeStamp() 
	{
		return QDateTime::currentMSecsSinceEpoch() - startTime;
	}

	QString getFileName()
	{
		return  QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
	}
}
