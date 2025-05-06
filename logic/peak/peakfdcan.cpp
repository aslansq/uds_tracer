#include "peakfdcan.h"
#include "util.h"
#include <PCANBasic.h> // Ensure the header file declaring CAN_InitializeFD is included

PeakFdCan::PeakFdCan(QObject *parent)
	: Can(parent)
	, configFdPtr(nullptr)
	, pcanHandle(this->invalidPcanHandle)
	, pcanConfigStr("")
	, pcanConfigCStr("")
{
}

PeakFdCan::~PeakFdCan()
{
}

void PeakFdCan::connect(const void * configPtr)
{
	TPCANStatus stsResult;

	this->configFdPtr = (ConfigFd *)configPtr;
	if (configFdPtr == nullptr) {
		Util::log(LogType::CmdRespThrow, LogSt::Nok, "Peak fd config pointer is null");
	}

	this->pcanHandle = getPeakHandleId(configFdPtr->getDev());

	this->pcanConfigStr =
		QString("f_clock_mhz=%1, ").arg(this->configFdPtr->getClkFreq()) +
		QString("nom_bitrate=%1, ").arg(this->configFdPtr->getArbitBaud()) +
		QString("data_bitrate=%1, ").arg(this->configFdPtr->getDataBaud()) +
		QString("nom_brp=%1, ").arg(this->configFdPtr->getArbitPre()) +
		QString("nom_tseg1=%1, ").arg(this->configFdPtr->getArbitTseg1()) +
		QString("nom_tseg2=%1, ").arg(this->configFdPtr->getArbitTseg2()) +
		QString("nom_sjw=%1, ").arg(this->configFdPtr->getArbitSjw()) +
		QString("data_brp=%1, ").arg(this->configFdPtr->getDataPre()) +
		QString("data_tseg1=%1, ").arg(this->configFdPtr->getDataTseg1()) +
		QString("data_tseg2=%1, ").arg(this->configFdPtr->getDataTseg2()) +
		QString("data_sjw=%1").arg(this->configFdPtr->getDataSjw());

	memcpy(this->pcanConfigCStr, this->pcanConfigStr.toUtf8().data(), this->pcanConfigStr.toUtf8().size() + 1);

	stsResult = CAN_InitializeFD(this->pcanHandle, pcanConfigCStr);

	if (stsResult != PCAN_ERROR_OK) {
		Util::log(LogType::CmdRespThrow, LogSt::Nok, CmdDef::connect, "on", getStatusStr(stsResult));
	} else {
		Util::log(LogType::CmdResp, LogSt::Ok, CmdDef::connect, "on", this->pcanConfigStr);
		emit eventOccured(CanEvent::Connected);
		startRxThread();
	}
}

void PeakFdCan::disconnect(void)
{
	if(!isConnected()) {
		Util::log(LogType::CmdResp, LogSt::Warn, CmdDef::connect, "off", "Not Connected to disconnect!");
		return;
	}
	stopRxThread();
	CAN_Uninitialize(this->pcanHandle);
	this->pcanHandle = this->invalidPcanHandle;
	emit eventOccured(CanEvent::Disconnected);
}

void PeakFdCan::peakFdMsgToCanMsg(const TPCANMsgFD &peakMsgRef, TPCANTimestampFD timestamp, CanMsg &canMsgRef)
{
	uint32_t idMask = 0x7FF;

	if ((peakMsgRef.MSGTYPE & PCAN_MESSAGE_EXTENDED) == PCAN_MESSAGE_EXTENDED) {
		idMask = 0x1FFFFFFF;
	}

	canMsgRef.id = peakMsgRef.ID & idMask;
	canMsgRef.dataLength = peakMsgRef.DLC;
	memcpy(canMsgRef.data, peakMsgRef.DATA, peakMsgRef.DLC);
	canMsgRef.timestamp = timestamp;
}

void PeakFdCan::rx()
{
	TPCANStatus peakResult;
	TPCANMsgFD peakCanMsg;
	TPCANTimestampFD peakTimestamp;

	peakResult = CAN_ReadFD(this->pcanHandle, &peakCanMsg, &peakTimestamp);
	if (peakResult == PCAN_ERROR_OK) {
		peakFdMsgToCanMsg(peakCanMsg, peakTimestamp, this->canMsg);
		this->rxQueue.enqueue(this->canMsg);
		emit eventOccured(CanEvent::MessageReceived);
	}
}
