#ifndef CONFIG_H
#define CONFIG_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QDomElement>

class CanType {
public:
	static const QString Std;
	static const QString Fd;
	static const QString Replay;
};

class ConfigAbstract : public QObject
{
	Q_OBJECT
public:
	ConfigAbstract(QObject *parent, QString name, const QMap<QString, QString> defMap);
	virtual ~ConfigAbstract() {}

	void setXml(const QDomElement &elem);
	QDomElement getXml(void);
	void reset(void);
	QMap<QString, QString> getMap(void) const;
	void setMap(const QMap<QString, QString> &mapRef);
	const QString &getName(void) const;

	friend class ConfigFd;
	friend class ConfigStd;
	friend class ConfigReplay;
	friend class ConfigTracer;
	friend class ConfigGeneric;

private:
	const QString name;
	const QMap<QString, QString> defMap;
	QMap<QString, QString> map;
};

class ConfigGeneric : public ConfigAbstract
{
public:
	ConfigGeneric(QObject *parent = nullptr);

	void setCanType(QString canType);

	QString getCanType(void) const;
};

class ConfigFd : public ConfigAbstract
{
public:
	ConfigFd(QObject *parent = nullptr);

	void setDev(const QString &devRef);
	void setClkFreq(const QString &clkRef);
	void setArbitBaud(const QString &baudRef);
	void setDataBaud(const QString &baudRef);
	void setArbitPre(const QString &preRef);
	void setDataPre(const QString &preRef);
	void setArbitTseg1(const QString &tseg1Ref);
	void setDataTseg1(const QString &tseg1Ref);
	void setArbitTseg2(const QString &tseg2Ref);
	void setDataTseg2(const QString &tseg2Ref);
	void setArbitSjw(const QString &sjwRef);
	void setDataSjw(const QString &sjwRef);

	QString getDev(void) const;
	QString getClkFreq(void) const;
	QString getArbitBaud(void) const;
	QString getDataBaud(void) const;
	QString getArbitPre(void) const;
	QString getDataPre(void) const;
	QString getArbitTseg1(void) const;
	QString getDataTseg1(void) const;
	QString getArbitTseg2(void) const;
	QString getDataTseg2(void) const;
	QString getArbitSjw(void) const;
	QString getDataSjw(void) const;
};

class ConfigStd : public ConfigAbstract
{
public:
	ConfigStd(QObject *parent = nullptr);

	void setDev(const QString &devRef);
	void setBaud(const QString &baudRef);

	QString getDev(void) const;
	QString getBaud(void) const;
};

class ConfigReplay : public ConfigAbstract
{
public:
	ConfigReplay(QObject *parent = nullptr);
	ConfigReplay(const ConfigReplay &other);

	void setDev(const QString &devRef);
	QString getDev(void) const;
};

class ConfigTracer : public ConfigAbstract
{
public:
	ConfigTracer(QObject *parent = nullptr);

	void setReqIdHex(const QString &reqIdHexRef);
	void setRespIdHex(const QString &respIdHexRef);
	void setLogDirPath(const QString &logDirPathRef);
	void setIsCaptureActive(const QString &isCaptureActiveRef);

	QString getReqIdHex(void) const;
	QString getRespIdHex(void) const;
	QString getLogDirPath(void) const;
};

class ConfigAll
{
public:
	ConfigAll();
	static const QString name;
	ConfigGeneric generic;
	ConfigFd fd;
	ConfigStd std;
	ConfigReplay replay;
	ConfigTracer tracer;
	void setXml(const QDomElement &elem);
	void getXml(QDomDocument &domDocRef);
	static const QByteArray xsdData;
};

#endif // CONFIG_H
