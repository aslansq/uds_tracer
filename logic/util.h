#ifndef UTIL_H
#define UTIL_H

#include <QObject>
#include <QMap>
#include <QString>
#include "cmddef.h"

enum class LogType {
	CmdReq,
	CmdResp,
	Generic,
	CanMsg,
	UdsReqMsg,
	UdsRespMsg,
	CmdReqThrow,
	CmdRespThrow,
	GenericThrow
};

enum class LogSt {
	Ok,
	Nok,
	Warn,
	Unknown
};

enum class LogProp {
	Timestamp,
	Type,
	Status,
	Command,
	CommandValue,
	Message
};

extern const QMap<LogProp, QString> LogPropStr;
extern const QMap<LogType, QString> LogTypeStr;
extern const QMap<LogSt, QString> LogStStr;

namespace Util
{
	QString fromMapToCmd(const QMap<QString, QString> &mapRef);
	QMap<QString, QString> fromCmdToMap(const QString &jsonStrRef);

	uint64_t getTimeStamp();
	QString getFileName();

	void log(LogType type, LogSt st, const CmdDef::Cmd &cmdRef, const QString &cmdValueRef, const QString &msgRef);
	void log(LogType type, LogSt st, const CmdDef::Cmd &cmdRef, const QString &msgRef);
	void log(LogType type, LogSt st, const QString &msgRef);
	void log(LogType type, const QString &msgRef);
} // namespace name


#endif // UTIL_H
