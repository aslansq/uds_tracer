#include "can.h"
#include "util.h"
#include <QMetaType>

Can::Can(QObject *parent)
	: QObject{parent}
	, rxThread(nullptr)
	, canMsg({
		.id = 0,
		.dataLength = 0,
		.data = {0},
		.timestamp = 0
	})
{
	qRegisterMetaType<CanEvent>("CanEvent");
}

bool Can::isConnected(void) const
{
	return this->rxThread != nullptr;
}

void Can::stopRxThread(void)
{
	if(this->rxThread != nullptr) {
		this->rxThread->exit();
		this->rxThread = nullptr;
	}
}

void Can::startRxThread(void)
{
	this->rxThread = QThread::create([this]() {
		while(isConnected()) {
			rx();
		}
	});
	this->rxThread->start();
}

QString Can::getMsgStr(const CanMsg &canMsgRef)
{
	QString s = QString("ID: %1, Data Length: %2, Data: ")
		.arg(canMsgRef.id, 0, 16)
		.arg(canMsgRef.dataLength, 2, 16, QChar('0'));
	for(int i = 0; i < canMsgRef.dataLength; ++i) {
		s += QString("%1 ").arg(canMsgRef.data[i], 2, 16, QChar('0'));
	}
	return s;
}

bool Can::getCanMsgFromRaw(const uint8_t *rawCanMsgPtr, size_t rawCanMsgSize, CanMsg &canMsgRef)
{
	if(rawCanMsgSize > sizeof(CanMsg)) {
		return false;
	}

	uint64_t offset = 0;

	memcpy((void *)&canMsgRef.id, (void *)rawCanMsgPtr, sizeof(CanMsg::id));
	offset += sizeof(CanMsg::id);
	memcpy((void *)&canMsgRef.dataLength, (void *)(rawCanMsgPtr + offset), sizeof(CanMsg::dataLength));
	offset += sizeof(CanMsg::dataLength);
	memcpy(canMsgRef.data, (void *)(rawCanMsgPtr + offset), canMsgRef.dataLength);
	offset += canMsgRef.dataLength;
	memcpy((void *)&canMsgRef.timestamp, (void *)(rawCanMsgPtr + offset), sizeof(CanMsg::timestamp));
	offset += sizeof(CanMsg::timestamp);

	if(offset != rawCanMsgSize) {
		return false;
	}
	return true;
}

size_t Can::getRawCanMsg(const CanMsg &canMsgRef, uint8_t *rawCanMsgPtr, size_t rawCanMsgSize)
{
	if(rawCanMsgSize < sizeof(CanMsg)) {
		return 0;
	}

	uint64_t offset = 0;

	memcpy((void *)rawCanMsgPtr, (void *)&canMsgRef.id, sizeof(CanMsg::id));
	offset += sizeof(CanMsg::id);
	memcpy((void *)(rawCanMsgPtr + offset), (void *)&canMsgRef.dataLength, sizeof(CanMsg::dataLength));
	offset += sizeof(CanMsg::dataLength);
	memcpy((void *)(rawCanMsgPtr + offset), canMsgRef.data, canMsgRef.dataLength);
	offset += canMsgRef.dataLength;
	memcpy((void *)(rawCanMsgPtr + offset), &canMsgRef.timestamp, sizeof(CanMsg::timestamp));
	offset += sizeof(CanMsg::timestamp);
	return offset;
}

void Can::printMsg(const CanMsg &canMsgRef)
{
	QString s = getMsgStr(canMsgRef);
	Util::log(LogType::CanMsg, LogSt::Ok, s);
}
