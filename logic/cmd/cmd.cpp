#include "cmd.h"
#include "util.h"

Cmd::Cmd(QObject *parent)
	: QObject{parent},
	isThrowEn{false}
{
	this->configAll.generic.setCanType(CanType::Std);
	connect(&this->peakFdCan, &PeakFdCan::eventOccured, this, &Cmd::canEventOccured);
	connect(&this->peakStdCan, &PeakStdCan::eventOccured, this, &Cmd::canEventOccured);
	connect(&this->replayCan, &ReplayCan::eventOccured, this, &Cmd::canEventOccured);
}

void Cmd::callHandleFunctions(const QMap<QString, QString> &cmdMapRef)
{
	warnUnknownCmd(cmdMapRef);
	handleConfigFd(cmdMapRef);
	handleConfigStd(cmdMapRef);
	handleConfigReplay(cmdMapRef);
	handleConfigTracer(cmdMapRef);
	handleFileOp(cmdMapRef);
	handleCanInterface(cmdMapRef);
}

void Cmd::commandMap(const QMap<QString, QString> &cmdMapRef)
{
	this->isThrowEn = false;
	if (cmdMapRef.isEmpty()) {
		Util::log(LogType::Generic, LogSt::Warn, "No command provided");
		return;
	}
	callHandleFunctions(cmdMapRef);
}

void Cmd::commandMapWThrow(const QMap<QString, QString> &cmdMapRef)
{
	this->isThrowEn = true;
	if (cmdMapRef.isEmpty()) {
		Util::log(LogType::Generic, LogSt::Warn, "No command provided");
		return;
	}
	callHandleFunctions(cmdMapRef);
}

Can *Cmd::getCanInterface(void)
{
	Can *retCanPtr = dynamic_cast<Can *>(&this->peakStdCan);
	QString canType = this->configAll.generic.getCanType();

	if(canType == CanType::Fd) {
		retCanPtr = dynamic_cast<Can *>(&this->peakFdCan);
	} else if(canType == CanType::Std) {
		retCanPtr = dynamic_cast<Can *>(&this->peakStdCan);
	} else if(canType == CanType::Replay) {
		retCanPtr = dynamic_cast<Can *>(&this->replayCan);
	}

	return retCanPtr;
}

const ConfigAll &Cmd::getConfigAll(void) const
{
	return this->configAll;
}

void Cmd::warnUnknownCmd(const QMap<QString, QString> &cmdMapRef)
{
	using namespace CmdDef;
	for(const QString &key : cmdMapRef.keys()) {
		if(allNames.contains(key)) {
			continue;
		}
		Util::log(LogType::CmdReq, LogSt::Unknown, key + " " + cmdMapRef[key]);
	}
}

void Cmd::handleConfigTracer(const QMap<QString, QString> &cmdMapRef)
{
	using namespace CmdDef;

	for(const QString &keyRef : cmdMapRef.keys()) {
		QString value = cmdMapRef[keyRef];
		const QVector<QString> pair = {keyRef, value};

		if(isOkToExec(reqIdHex, pair)) {
			this->configAll.tracer.setReqIdHex(value);
			Util::log(LogType::CmdResp, LogSt::Ok, reqIdHex, value, "");
		}

		if(isOkToExec(respIdHex, pair)) {
			this->configAll.tracer.setRespIdHex(value);
			Util::log(LogType::CmdResp, LogSt::Ok, respIdHex, value, "");
		}

		if(isOkToExec(logDirPath, pair)) {
			this->configAll.tracer.setLogDirPath(value);
			Util::log(LogType::CmdResp, LogSt::Ok, logDirPath, value, "");
		}
	}
}

void Cmd::handleCanInterface(const QMap<QString, QString> &cmdMapRef)
{
	using namespace CmdDef;
	for(const QString &keyRef : cmdMapRef.keys()) {
		QString value = cmdMapRef[keyRef];
		const QVector<QString> pair = {keyRef, value};

		if(isOkToExec(canType, pair)) {
			this->configAll.generic.setCanType(value);
			Util::log(LogType::CmdResp, LogSt::Ok, canType, value, "");
		}

		if(value == "on" && isOkToExec(CmdDef::connect, pair)) {
			QString canType = configAll.generic.getCanType();
			if(canType == CanType::Fd) {
				try {
					peakFdCan.connect(static_cast<const void *>(&configAll.fd));
					Util::log(LogType::CmdResp, LogSt::Ok, CmdDef::connect, value, "fd connected");
				} catch (const std::exception &e) {
					Util::log(
						this->isThrowEn ? LogType::CmdRespThrow : LogType::CmdResp,
						LogSt::Nok,
						CmdDef::connect,
						value,
						QString("Failed to fd connect: %1").arg(e.what())
					);
				}
			} else if(canType == CanType::Std) {
				try {
					peakStdCan.connect(static_cast<const void *>(&configAll.std));
					Util::log(LogType::CmdResp, LogSt::Ok, CmdDef::connect, value, "std connected");
				} catch (const std::exception &e) {
					Util::log(
						this->isThrowEn ? LogType::CmdRespThrow : LogType::CmdResp,
						LogSt::Nok,
						CmdDef::connect,
						value,
						QString("Failed to std connect: %1").arg(e.what())
					);
				}
			} else if (canType == CanType::Replay) {
				try {
					replayCan.connect(static_cast<const void *>(&configAll.replay));
					Util::log(LogType::CmdResp, LogSt::Ok, CmdDef::connect, value, "replay connected");
				} catch (const std::exception &e) {
					Util::log(
						this->isThrowEn ? LogType::CmdRespThrow : LogType::CmdResp,
						LogSt::Nok,
						CmdDef::connect,
						value,
						QString("Failed to replay connect: %1").arg(e.what())
					);
				}
			}
		}

		if(value == "off" && isOkToExec(CmdDef::connect, pair)) {
			QString canType = configAll.generic.getCanType();
			if(canType == CanType::Fd) {
				peakFdCan.disconnect();
				Util::log(LogType::CmdResp, LogSt::Ok, CmdDef::connect, value, "");
			} else if(canType == CanType::Std) {
				peakStdCan.disconnect();
				Util::log(LogType::CmdResp, LogSt::Ok, CmdDef::connect, value, "");
			} else if (canType == CanType::Replay) {
				replayCan.disconnect();
				Util::log(LogType::CmdResp, LogSt::Ok, CmdDef::connect, value, "");
			}
		}
	}
}
