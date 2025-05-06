#ifndef CMD_H
#define CMD_H

#include <QObject>
#include <QMap>
#include "config.h"
#include "cmddef.h"
#include "peakfdcan.h"
#include "peakstdcan.h"
#include "replaycan.h"

class Cmd : public QObject
{
	Q_OBJECT
public:
	explicit Cmd(QObject *parent = nullptr);
	void commandMap(const QMap<QString, QString> &cmdMapRef);
	void commandMapWThrow(const QMap<QString, QString> &cmdMapRef);

	Can *getCanInterface(void);
	const ConfigAll &getConfigAll(void) const;
signals:
	void configAllLoaded(const ConfigAll &configAll);
	void canEventOccured(CanEvent canEvent);

private:
	ConfigAll configAll;
	bool isThrowEn;
	PeakFdCan peakFdCan;
	PeakStdCan peakStdCan;
	ReplayCan replayCan;

	QMap<QString, QString> getCmdMap(int argc, char **argvPtrPtr);
	QMap<QString, QString> getCmdMap(const QVector<QString> &argvRef);
	QMap<QString, QString> getCmdMap(const QString &argvRef);

	void callHandleFunctions(const QMap<QString, QString> &cmdMapRef);

	void warnUnknownCmd(const QMap<QString, QString> &cmdMapRef);
	void handleConfigFd(const QMap<QString, QString> &cmdMapRef);
	void handleConfigStd(const QMap<QString, QString> &cmdMapRef);
	void handleConfigApp(const QMap<QString, QString> &cmdMapRef);
	void handleConfigReplay(const QMap<QString, QString> &cmdMapRef);
	void handleConfigTracer(const QMap<QString, QString> &cmdMapRef);
	void handleFileOp(const QMap<QString, QString> &cmdMapRef);
	void handleCanInterface(const QMap<QString, QString> &cmdMapRef);

	QDomElement getConfigXmlRoot(const QString &filePathRef);
};

#endif // CMD_H
