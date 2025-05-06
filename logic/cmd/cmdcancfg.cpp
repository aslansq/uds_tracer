#include "cmd.h"
#include "cmddef.h"
#include "util.h"

void Cmd::handleConfigStd(const QMap<QString, QString> &cmdMapRef)
{
	using namespace CmdDef;

	for(const QString &keyRef : cmdMapRef.keys()) {
		QString value = cmdMapRef[keyRef];
		const QVector<QString> pair = { keyRef, value };

		if(isOkToExec(devStd, pair)) {
			this->configAll.std.setDev(value);
			Util::log(LogType::CmdResp, LogSt::Ok, devStd, value, "");
			continue;
		}

		if(isOkToExec(baud, pair)) {
			this->configAll.std.setBaud(value);
			Util::log(LogType::CmdResp, LogSt::Ok, baud, value, "");
			continue;
		}
	}
}

void Cmd::handleConfigFd(const QMap<QString, QString> &cmdMapRef)
{
	using namespace CmdDef;

	for(const QString &keyRef : cmdMapRef.keys()) {
		QString value = cmdMapRef[keyRef];
		const QVector<QString> pair = { keyRef, value };

		if(isOkToExec(devFd, pair)) {
			this->configAll.fd.setDev(value);
			Util::log(LogType::CmdResp, LogSt::Ok, devFd, value, "");
			continue;
		}

		if(isOkToExec(clkFreqMHz, pair)) {
			this->configAll.fd.setClkFreq(value);
			Util::log(LogType::CmdResp, LogSt::Ok, clkFreqMHz, value, "");
			continue;
		}

		if(isOkToExec(arbitBaud, pair)) {
			this->configAll.fd.setArbitBaud(value);
			Util::log(LogType::CmdResp, LogSt::Ok, arbitBaud, value, "");
			continue;
		}

		if(isOkToExec(dataBaud, pair)) {
			this->configAll.fd.setDataBaud(value);
			Util::log(LogType::CmdResp, LogSt::Ok, dataBaud, value, "");
			continue;
		}

		if(isOkToExec(arbitPre, pair)) {
			this->configAll.fd.setArbitPre(value);
			Util::log(LogType::CmdResp, LogSt::Ok, arbitPre, value, "");
			continue;
		}

		if(isOkToExec(dataPre, pair)) {
			this->configAll.fd.setDataPre(value);
			Util::log(LogType::CmdResp, LogSt::Ok, dataPre, value, "");
			continue;
		}

		if(isOkToExec(arbitTseg1, pair)) {
			this->configAll.fd.setArbitTseg1(value);
			Util::log(LogType::CmdResp, LogSt::Ok, arbitTseg1, value, "");
			continue;
		}

		if(isOkToExec(dataTseg1, pair)) {
			this->configAll.fd.setDataTseg1(value);
			Util::log(LogType::CmdResp, LogSt::Ok, dataTseg1, value, "");
			continue;
		}

		if(isOkToExec(arbitTseg2, pair)) {
			this->configAll.fd.setArbitTseg2(value);
			Util::log(LogType::CmdResp, LogSt::Ok, arbitTseg2, value, "");
			continue;
		}

		if(isOkToExec(dataTseg2, pair)) {
			this->configAll.fd.setDataTseg2(value);
			Util::log(LogType::CmdResp, LogSt::Ok, dataTseg2, value, "");
			continue;
		}

		if(isOkToExec(arbitSjw, pair)) {
			this->configAll.fd.setArbitSjw(value);
			Util::log(LogType::CmdResp, LogSt::Ok, arbitSjw, value, "");
			continue;
		}

		if(isOkToExec(dataSjw, pair)) {
			this->configAll.fd.setDataSjw(value);
			Util::log(LogType::CmdResp, LogSt::Ok, dataSjw, value, "");
			continue;
		}
	}
}

void Cmd::handleConfigReplay(const QMap<QString, QString> &cmdMapRef)
{
	using namespace CmdDef;

	for(const QString &keyRef : cmdMapRef.keys()) {
		QString value = cmdMapRef[keyRef];

		if(isOkToExec(devReplay, { keyRef, value })) {
			this->configAll.replay.setDev(value);
			Util::log(LogType::CmdResp, LogSt::Ok, devReplay, value, "");
			continue;
		}
	}
}
