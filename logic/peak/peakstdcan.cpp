#include "peakstdcan.h"
#include "util.h"

PeakStdCan::PeakStdCan(QObject *parent)
	: Can(parent)
	, configStdPtr(nullptr)
	, pcanHandle(0)
{

}

PeakStdCan::~PeakStdCan()
{}

void PeakStdCan::connect(const void *configPtr)
{
	TPCANStatus stsResult;
	TPCANBaudrate pcanBaud = this->invalidPcanBaud;
	uint64_t baudrate;
	QString dev;

	this->configStdPtr = (ConfigStd *)configPtr;
	if(this->configStdPtr == nullptr) {
		Util::log(LogType::CmdRespThrow, LogSt::Nok, "Peak std config pointer is null");
	}

	dev = this->configStdPtr->getDev();
	this->pcanHandle = getPeakHandleId(dev);

	if(this->pcanHandle == this->invalidPcanHandle) {
		Util::log(LogType::CmdRespThrow, LogSt::Nok, CmdDef::connect, "on", QString("Invalid device %1!").arg(dev));
		return;
	}

	baudrate = this->configStdPtr->getBaud().toInt();
	pcanBaud = getPcanBaud(baudrate);

	if(pcanBaud == this->invalidPcanBaud) {
		Util::log(LogType::CmdRespThrow, LogSt::Nok, CmdDef::connect, "on", QString("Invalid baudrate %1!").arg(baudrate));
		return;
	}

	stsResult = CAN_Initialize(this->pcanHandle, pcanBaud);

	if (stsResult != PCAN_ERROR_OK) {
		Util::log(LogType::CmdRespThrow, LogSt::Nok, CmdDef::connect, "on", getStatusStr(stsResult));
	} else {
		Util::log(LogType::CmdResp, LogSt::Ok, CmdDef::connect, "on", QString("%1 %2").arg(dev).arg(baudrate));
		emit eventOccured(CanEvent::Connected);
		startRxThread();
	}
}

void PeakStdCan::disconnect(void)
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

void PeakStdCan::rx()
{
	TPCANMsg peakCanMsg;
	TPCANTimestamp peakTimestamp;
	TPCANStatus peakResult;

	peakResult = CAN_Read(this->pcanHandle, &peakCanMsg, &peakTimestamp);
	if(peakResult == PCAN_ERROR_OK) {
		peakStdMsgToCanMsg(peakCanMsg, peakTimestamp, this->canMsg);
		this->rxQueue.enqueue(this->canMsg);
		emit eventOccured(CanEvent::MessageReceived);
	}
}

void PeakStdCan::peakStdMsgToCanMsg(const TPCANMsg &peakMsgRef, TPCANTimestamp peakTimestamp, CanMsg &canMsgRef)
{
	uint32_t idMask = 0x7FF;

	if ((peakMsgRef.MSGTYPE & PCAN_MESSAGE_EXTENDED) == PCAN_MESSAGE_EXTENDED) {
		idMask = 0x1FFFFFFF;
	}

	canMsgRef.id = peakMsgRef.ID & idMask;
	canMsgRef.dataLength = peakMsgRef.LEN;
	memcpy(canMsgRef.data, peakMsgRef.DATA, peakMsgRef.LEN);
	canMsgRef.timestamp = peakTimestamp.millis;
}

TPCANBaudrate PeakStdCan::getPcanBaud(uint64_t baudrate)
{
	TPCANBaudrate peakBaud = this->invalidPcanBaud;
	QMap<uint64_t, TPCANBaudrate> peakBauds = {
		{1000000, PCAN_BAUD_1M  },
		{800000 , PCAN_BAUD_800K},
		{500000 , PCAN_BAUD_500K},
		{250000 , PCAN_BAUD_250K},
		{125000 , PCAN_BAUD_125K},
		{100000 , PCAN_BAUD_100K},
		{95000  , PCAN_BAUD_95K },
		{83000  , PCAN_BAUD_83K },
		{50000  , PCAN_BAUD_50K },
		{47000  , PCAN_BAUD_47K },
		{33000  , PCAN_BAUD_33K },
		{20000  , PCAN_BAUD_20K },
		{10000  , PCAN_BAUD_10K },
		{5000   , PCAN_BAUD_5K  }
	};

	if(peakBauds.contains(baudrate)) {
		peakBaud = peakBauds[baudrate];
	}

	return peakBaud;
}
