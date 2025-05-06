#ifndef REPLAYCAN_H
#define REPLAYCAN_H

#include <QFile>
#include <QObject>
#include "can.h"
#include "config.h"

class ReplayCan : public Can
{
	Q_OBJECT
public:
	explicit ReplayCan(QObject *parent = nullptr);
	~ReplayCan() override;

	void connect(const void *configPtr) override;
	void disconnect(void) override;
signals:

private:
	const ConfigReplay *configReplayPtr;
	void rx(void) override;
	QFile *filePtr;
	QString filePath;
	uint64_t fileSize;
};

#endif // REPLAYCAN_H
