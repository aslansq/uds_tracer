#include "replaycan.h"
#include "util.h"
#include "cobs.h"

ReplayCan::ReplayCan(QObject *parent)
	: Can(parent)
	, configReplayPtr(nullptr)
	, filePath("")
{

}

ReplayCan::~ReplayCan()
{
	if (this->filePtr != nullptr) {
		this->filePtr->close();
		delete filePtr;
		this->filePtr = nullptr;
	}
}

void ReplayCan::connect(const void *configPtr)
{
	this->configReplayPtr = (ConfigReplay *)configPtr;
	if(this->configReplayPtr == nullptr) {
		Util::log(
			LogType::CmdRespThrow,
			LogSt::Nok,
			CmdDef::connect,
			"on",
			"Replay can config is null"
		);
		return;
	}
	this->filePath = this->configReplayPtr->getDev();
	this->filePtr = new QFile(this->filePath);
	if (!filePtr->exists()) {
		Util::log(
			LogType::CmdRespThrow,
			LogSt::Nok,
			CmdDef::connect,
			"on",
			"Replay file does not exist"
		);
		return;
	}

	if (!filePtr->open(QIODevice::ReadOnly)) {
		Util::log(
			LogType::CmdRespThrow,
			LogSt::Nok,
			CmdDef::connect,
			"on",
			"Replay file open failed"
		);
		return;
	} 
	
	this->fileSize = this->filePtr->size();

	if(this->fileSize == 0) {
		Util::log(
			LogType::CmdRespThrow,
			LogSt::Nok,
			CmdDef::connect,
			"on",
			"Replay file is empty"
		);
		return;
	}
	
	Util::log(
		LogType::CmdResp,
		LogSt::Ok,
		CmdDef::connect,
		"on",
		this->configReplayPtr->getDev()
	);
	emit eventOccured(CanEvent::Connected);
	startRxThread();
}

void ReplayCan::disconnect(void)
{
	if (!isConnected()) {
		Util::log(
			LogType::CmdResp,
			LogSt::Warn,
			CmdDef::connect,
			"off",
			"Not Connected to disconnect!"
		);
		return;
	}
	stopRxThread();
	this->filePtr->close();
	delete filePtr;
	this->filePtr = nullptr;
	this->fileSize = 0;
	this->filePath = "";
	emit eventOccured(CanEvent::Disconnected);
}

void ReplayCan::rx(void)
{
	if (this->filePtr == nullptr) {
		Util::log(
			LogType::CmdRespThrow,
			LogSt::Nok,
			CmdDef::connect,
			"on",
			"Replay file pointer is null"
		);
		return;
	}

	QVector<uint8_t> canFrame;
	canFrame.clear();
	uint8_t rawData[sizeof(CanMsg)];
	size_t rawDataSize = sizeof(CanMsg);
	char byte;

	std::unique_ptr<uint8_t[]> buffer = std::make_unique<uint8_t[]>(this->fileSize);
	if (buffer == nullptr) {
		Util::log(
			LogType::CmdRespThrow,
			LogSt::Nok,
			CmdDef::connect,
			"on",
			"Buffer allocation failed"
		);
		return;
	}

	// Read the file into the buffer
	size_t bytesRead = this->filePtr->read((char *)buffer.get(), this->fileSize);
	if (bytesRead != this->fileSize) {
		Util::log(
			LogType::CmdRespThrow,
			LogSt::Nok,
			CmdDef::connect,
			"on",
			"File read failed"
		);
		return;
	}

	for(uint64_t i = 0; i < this->fileSize; ++i) {
		byte = buffer[i];
		if (byte == 0) {
			// COBS decode
			cobs_decode_result result = cobs_decode(
				rawData,
				rawDataSize,
				canFrame.data(),
				canFrame.size()
			);
			getCanMsgFromRaw(
				rawData,
				result.out_len,
				this->canMsg
			);
			this->rxQueue.enqueue(this->canMsg);
			emit eventOccured(CanEvent::MessageReceived);
			canFrame.clear();
			QThread::msleep(1);
		} else {
			canFrame.append((uint8_t)byte);
		}
	}

	disconnect();
}
