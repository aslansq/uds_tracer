/**
 * @defgroup util_h
 * @{
 * @file util.h
 * @brief Ideally standard output should be only used through these util log functions.
 * Purpose of this functions is that, we could set output to xml,json... does not matter.
 * If somebody wants to script it, he can do it easily.
 */
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
	CmdReqThrow,  //!< throws runtime error with log
	CmdRespThrow, //!< throws runtime error with log
	GenericThrow  //!< throws runtime error with log
};

enum class LogSt {
	Ok,
	Nok,
	Warn,
	Unknown
};

/// @brief Loggin properties. Each one is mapped to a string which is used for json/xml.
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

/// @}
