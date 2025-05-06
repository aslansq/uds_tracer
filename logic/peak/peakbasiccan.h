#ifndef PEAKBASICCAN_H
#define PEAKBASICCAN_H

#include <QObject>
#include <QMap>
#include <QString>
#include "PCANBasic.h"

class PeakBasicCan
{
public:
	const TPCANHandle invalidPcanHandle = 0;
	explicit PeakBasicCan(void);
	QString getStatusStr(TPCANStatus st);
	TPCANHandle getPeakHandleId(int devNum);
	TPCANHandle getPeakHandleId(QString devStr);
signals:

private:
	const QMap<TPCANStatus, QString> statusStrings;
};

#endif // PEAKBASICCAN_H
