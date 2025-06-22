#include <QDir>
#include <QDebug>
#include "config.h"
#include "cmddef.h"
#include "util.h"

const QString ConfigAll::name = "ConfigAll";
const QString CanType::Std = "Std";
const QString CanType::Fd = "Fd";
const QString CanType::Replay = "Replay";

const QByteArray ConfigAll::xsdData = QByteArrayLiteral(R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">
	<xs:element name="ConfigAll">
		<xs:complexType>
			<xs:sequence>
				<xs:element name="Generic">
					<xs:complexType>
						<xs:sequence>
							<xs:element name="canType" type="xs:string" />
						</xs:sequence>
					</xs:complexType>
				</xs:element>
				<xs:element name="CanFdCfg">
					<xs:complexType>
						<xs:sequence>
							<xs:element name="arbitBaud" type="xs:integer" minOccurs="0" />
							<xs:element name="arbitPre" type="xs:integer" />
							<xs:element name="arbitSjw" type="xs:integer" />
							<xs:element name="arbitTseg1" type="xs:integer" />
							<xs:element name="arbitTseg2" type="xs:integer" />
							<xs:element name="clkFreqMHz" type="xs:integer" />
							<xs:element name="dataBaud" type="xs:integer" />
							<xs:element name="dataPre" type="xs:integer" />
							<xs:element name="dataSjw" type="xs:integer" />
							<xs:element name="dataTseg1" type="xs:integer" />
							<xs:element name="dataTseg2" type="xs:integer" />
							<xs:element name="devFd" type="xs:string" />
						</xs:sequence>
					</xs:complexType>
				</xs:element>
				<xs:element name="CanStdCfg">
					<xs:complexType>
						<xs:sequence>
							<xs:element name="baud" type="xs:integer" />
							<xs:element name="devStd" type="xs:string" />
						</xs:sequence>
					</xs:complexType>
				</xs:element>
				<xs:element name="CanReplayCfg">
					<xs:complexType>
						<xs:sequence>
							<xs:element name="devReplay" type="xs:string" />
						</xs:sequence>
					</xs:complexType>
				</xs:element>
				<xs:element name="TracerCfg">
					<xs:complexType>
						<xs:sequence>
							<xs:element name="logDirPath" type="xs:string" />
							<xs:element name="reqIdHex" type="xs:string" />
							<xs:element name="respIdHex" type="xs:string" />
						</xs:sequence>
					</xs:complexType>
				</xs:element>
			</xs:sequence>
		</xs:complexType>
	</xs:element>
</xs:schema>)");


ConfigAbstract::ConfigAbstract(QObject *parent, QString name, const QMap<QString, QString> defMap) :
	QObject(parent),
	name(name),
	defMap(defMap),
	map(defMap)
{
}

void ConfigAbstract::setMap(const QMap<QString, QString> &mapRef)
{
	this->map = mapRef;
}

void ConfigAbstract::setXml(const QDomElement &elem)
{
	QMap<QString, QString> map;
	QDomNodeList nodeList = elem.childNodes();
	for (int i = 0; i < nodeList.size(); i++) {
		QDomNode node = nodeList.at(i);
		if (node.isElement()) {
			QDomElement element = node.toElement();
			map[element.tagName()] = element.text();
		}
	}

	this->map = map;
}

QDomElement ConfigAbstract::getXml(void)
{
	QDomDocument doc;
	QDomElement root = doc.createElement(this->name);
	doc.appendChild(root);

	for (const QString &keyRef : this->map.keys()) {
		QDomElement element = doc.createElement(keyRef);
		QDomText text = doc.createTextNode(this->map[keyRef]);
		element.appendChild(text);
		root.appendChild(element);
	}

	return root;
}

const QString &ConfigAbstract::getName(void) const
{
	return this->name;
}

void ConfigAbstract::reset(void)
{
	this->map = this->defMap;
}

QMap<QString, QString> ConfigAbstract::getMap(void) const
{
	return this->map;
}

ConfigGeneric::ConfigGeneric(QObject *parent):
	ConfigAbstract(
		parent,
		CmdDef::typeNames[CmdDef::Type::Generic],
		{
			{ CmdDef::canType.name , CmdDef::canType.possibleValues[0] }
		}
	)
{
}

void ConfigGeneric::setCanType(QString canType)
{
	this->map[CmdDef::canType.name] = canType;
}

QString ConfigGeneric::getCanType(void) const
{
	return this->map[CmdDef::canType.name];
}


ConfigFd::ConfigFd(QObject *parent):
	ConfigAbstract(
		parent,
		CmdDef::typeNames[CmdDef::Type::CanFdCfg],
		{
#ifdef Q_OS_WIN32
			{ CmdDef::devFd.name     , "1" },
#else
			{ CmdDef::devFd.name     , "/dev/pcanusbfd32" },
#endif
			{ CmdDef::clkFreqMHz.name, "80" },
			{ CmdDef::arbitBaud.name , "500000" },
			{ CmdDef::dataBaud.name  , "2000000" },
			{ CmdDef::arbitPre.name , "1" },
			{ CmdDef::dataPre.name , "1" },
			{ CmdDef::arbitTseg1.name, "119" },
			{ CmdDef::dataTseg1.name , "29" },
			{ CmdDef::arbitTseg2.name, "40" },
			{ CmdDef::dataTseg2.name , "10" },
			{ CmdDef::arbitSjw.name  , "40" },
			{ CmdDef::dataSjw.name   , "10" }
		}
	)
{
	for(const QString &keyRef : CmdDef::typeCmdNames[CmdDef::Type::CanFdCfg]) {
		if (!this->defMap.contains(keyRef)) {
			Util::log(LogType::GenericThrow, "Key missing in fd: " + keyRef);
		}
	}

	for (const QString &keyRef : this->defMap.keys()) {
		if (!CmdDef::typeCmdNames[CmdDef::Type::CanFdCfg].contains(keyRef)) {
			Util::log(LogType::GenericThrow, "Extra key in fd: " + keyRef);
		}
	}

	reset();
}

void ConfigFd::setDev(const QString &devRef)
{
	this->map[CmdDef::devFd.name] = devRef;
}

void ConfigFd::setClkFreq(const QString &clkRef)
{
	this->map[CmdDef::clkFreqMHz.name] = clkRef;
}

void ConfigFd::setArbitBaud(const QString &baudRef)
{
	this->map[CmdDef::arbitBaud.name] = baudRef;
}

void ConfigFd::setDataBaud(const QString &baudRef)
{
	this->map[CmdDef::dataBaud.name] = baudRef;
}

void ConfigFd::setArbitPre(const QString &preRef)
{
	this->map[CmdDef::arbitPre.name] = preRef;
}

void ConfigFd::setDataPre(const QString &preRef)
{
	this->map[CmdDef::dataPre.name] = preRef;
}

void ConfigFd::setArbitTseg1(const QString &tseg1Ref)
{
	this->map[CmdDef::arbitTseg1.name] = tseg1Ref;
}

void ConfigFd::setDataTseg1(const QString &tseg1Ref)
{
	this->map[CmdDef::dataTseg1.name] = tseg1Ref;
}

void ConfigFd::setArbitTseg2(const QString &tseg2Ref)
{
	this->map[CmdDef::arbitTseg2.name] = tseg2Ref;
}

void ConfigFd::setDataTseg2(const QString &tseg2Ref)
{
	this->map[CmdDef::dataTseg2.name] = tseg2Ref;
}

void ConfigFd::setArbitSjw(const QString &sjwRef)
{
	this->map[CmdDef::arbitSjw.name] = sjwRef;
}

void ConfigFd::setDataSjw(const QString &sjwRef)
{
	this->map[CmdDef::dataSjw.name] = sjwRef;
}

QString ConfigFd::getDev(void) const
{
	return this->map[CmdDef::devFd.name];
}

QString ConfigFd::getClkFreq() const
{
	return this->map[CmdDef::clkFreqMHz.name];
}

QString ConfigFd::getArbitBaud(void) const
{
	return this->map[CmdDef::arbitBaud.name];
}

QString ConfigFd::getDataBaud(void) const
{
	return this->map[CmdDef::dataBaud.name];
}

QString ConfigFd::getArbitPre() const
{
	return this->map[CmdDef::arbitPre.name];
}

QString ConfigFd::getDataPre() const
{
	return this->map[CmdDef::dataPre.name];
}

QString ConfigFd::getArbitTseg1(void) const
{
	return this->map[CmdDef::arbitTseg1.name];
}

QString ConfigFd::getDataTseg1(void) const
{
	return this->map[CmdDef::dataTseg1.name];
}

QString ConfigFd::getArbitTseg2(void) const
{
	return this->map[CmdDef::arbitTseg2.name];
}

QString ConfigFd::getDataTseg2(void) const
{
	return this->map[CmdDef::dataTseg2.name];
}

QString ConfigFd::getArbitSjw(void) const
{
	return this->map[CmdDef::arbitSjw.name];
}

QString ConfigFd::getDataSjw(void) const
{
	return this->map[CmdDef::dataSjw.name];
}

ConfigStd::ConfigStd(QObject *parent):
	ConfigAbstract(
		parent,
		CmdDef::typeNames[CmdDef::Type::CanStdCfg],
		{
#ifdef Q_OS_WIN32
			{ CmdDef::devStd.name     , "1" },
#else
			{ CmdDef::devStd.name, "/dev/pcanusb32" },
#endif
			{ CmdDef::baud.name, "500000" }
		}
	)
{
	for (const QString &keyRef : CmdDef::typeCmdNames[CmdDef::Type::CanStdCfg]) {
		if (!defMap.contains(keyRef)) {
			Util::log(LogType::GenericThrow, "Key missing in std:" + keyRef);
		}
	}

	for (const QString &keyRef : defMap.keys()) {
		if (!CmdDef::typeCmdNames[CmdDef::Type::CanStdCfg].contains(keyRef)) {
			Util::log(LogType::GenericThrow, "Extra key in std:" + keyRef);
		}
	}

	reset();
}

void ConfigStd::setDev(const QString &devRef)
{
	this->map[CmdDef::devStd.name] = devRef;
}

void ConfigStd::setBaud(const QString &baudRef)
{
	this->map[CmdDef::baud.name] = baudRef;
}

QString ConfigStd::getDev(void) const
{
	return this->map[CmdDef::devStd.name];
}

QString ConfigStd::getBaud(void) const
{
	return this->map[CmdDef::baud.name];
}

ConfigReplay::ConfigReplay(QObject *parent):
	ConfigAbstract(
		parent,
		CmdDef::typeNames[CmdDef::Type::CanReplayCfg],
		{
			{ CmdDef::devReplay.name, QDir::homePath() + "/log.blf" }
		}
	)
{
	for (const QString &keyRef : CmdDef::typeCmdNames[CmdDef::Type::CanReplayCfg]) {
		if (!defMap.contains(keyRef)) {
			Util::log(LogType::GenericThrow, "Key missing in replay:" + keyRef);
		}
	}

	for (const QString &keyRef : defMap.keys()) {
		if (!CmdDef::typeCmdNames[CmdDef::Type::CanReplayCfg].contains(keyRef)) {
			Util::log(LogType::GenericThrow, "Extra key in replay:" + keyRef);
		}
	}

	reset();
}

void ConfigReplay::setDev(const QString &devRef)
{
	this->map[CmdDef::devReplay.name] = devRef;
}

QString ConfigReplay::getDev(void) const
{
	return this->map[CmdDef::devReplay.name];
}

ConfigTracer::ConfigTracer(QObject *parent):
	ConfigAbstract(
		parent,
		CmdDef::typeNames[CmdDef::Type::TracerCfg],
		{
			{ CmdDef::reqIdHex.name, "0x7DF" },
			{ CmdDef::respIdHex.name, "0x7E8" },
			{ CmdDef::logDirPath.name, QDir::homePath() + "/udstracer" }
		}
	)
{
	for (const QString &keyRef : CmdDef::typeCmdNames[CmdDef::Type::TracerCfg]) {
		if (!defMap.contains(keyRef)) {
			Util::log(LogType::GenericThrow, "Key missing in app:"+ keyRef);
		}
	}

	for (const QString &keyRef : defMap.keys()) {
		if (!CmdDef::typeCmdNames[CmdDef::Type::TracerCfg].contains(keyRef)) {
			Util::log(LogType::GenericThrow, "Extra key in app:" + keyRef);
		}
	}

	reset();
}

void ConfigTracer::setReqIdHex(const QString &reqIdRef)
{
	this->map[CmdDef::reqIdHex.name] = reqIdRef;
}

void ConfigTracer::setRespIdHex(const QString &respIdRef)
{
	this->map[CmdDef::respIdHex.name] = respIdRef;
}

void ConfigTracer::setLogDirPath(const QString &logDirPathRef)
{
	this->map[CmdDef::logDirPath.name] = logDirPathRef;
}

QString ConfigTracer::getReqIdHex(void) const
{
	return this->map[CmdDef::reqIdHex.name];
}

QString ConfigTracer::getRespIdHex(void) const
{
	return this->map[CmdDef::respIdHex.name];
}

QString ConfigTracer::getLogDirPath(void) const
{
	return this->map[CmdDef::logDirPath.name];
}

ConfigAll::ConfigAll()
{}

void ConfigAll::setXml(const QDomElement &root)
{
	QDomElement configGeneric = root.firstChildElement(this->generic.getName());
	QDomElement configFdElem = root.firstChildElement(this->fd.getName());
	QDomElement configStdElem = root.firstChildElement(this->std.getName());
	QDomElement configReplayElem = root.firstChildElement(this->replay.getName());
	QDomElement configTracerElem = root.firstChildElement(this->tracer.getName());
	this->generic.setXml(configGeneric);
	this->fd.setXml(configFdElem);
	this->std.setXml(configStdElem);
	this->replay.setXml(configReplayElem);
	this->tracer.setXml(configTracerElem);
}

void ConfigAll::getXml(QDomDocument &domDocRef)
{
	QDomElement root = domDocRef.createElement(this->name);
	domDocRef.appendChild(root);
	QDomElement configGenericElem = this->generic.getXml();
	QDomElement configFdElem = this->fd.getXml();
	QDomElement configStdElem = this->std.getXml();
	QDomElement configReplayElem = this->replay.getXml();
	QDomElement configTracerElem = this->tracer.getXml();
	root.appendChild(configGenericElem);
	root.appendChild(configFdElem);
	root.appendChild(configStdElem);
	root.appendChild(configReplayElem);
	root.appendChild(configTracerElem);
}
