#ifndef PEAKFDCAN_H
#define PEAKFDCAN_H

#include <QObject>
#include "can.h"
#include "config.h"
#include "peakbasiccan.h"

class PeakFdCan : public Can, public PeakBasicCan
{
	Q_OBJECT
public:
	explicit PeakFdCan(QObject *parent = nullptr);
	~PeakFdCan() override;

	void connect(const void *configPtr) override;
	void disconnect(void) override;
signals:

private:
	const ConfigFd *configFdPtr;
	void rx(void) override;
	TPCANHandle pcanHandle;
	QString pcanConfigStr;
	char pcanConfigCStr[10000];
	void peakFdMsgToCanMsg(const TPCANMsgFD &peakMsgRef, TPCANTimestampFD timestamp, CanMsg &canMsgRef);
};

#endif // PEAKFDCAN_H
