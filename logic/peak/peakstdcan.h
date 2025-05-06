#ifndef PEAKSTDCAN_H
#define PEAKSTDCAN_H

#include <QObject>
#include "can.h"
#include "config.h"
#include "peakbasiccan.h"

class PeakStdCan : public Can, public PeakBasicCan
{
	Q_OBJECT
public:
	explicit PeakStdCan(QObject *parent = nullptr);
	~PeakStdCan() override;

	void connect(const void *configPtr) override;
	void disconnect(void) override;
signals:

private:
	const TPCANBaudrate invalidPcanBaud = 0;
	ConfigStd *configStdPtr;
	void rx(void) override;
	TPCANHandle pcanHandle;
	TPCANBaudrate getPcanBaud(uint64_t baudrate);
	void peakStdMsgToCanMsg(const TPCANMsg &peakMsgRef, TPCANTimestamp peakTimestamp, CanMsg &canMsgRef);
};

#endif // PEAKSTDCAN_H
