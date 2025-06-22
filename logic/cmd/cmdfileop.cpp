#include <QDomDocument>
#include <QDomElement>
#include <QFile>
#include <QTextStream>
//#include <QXmlSchema>
//#include <QXmlSchemaValidator>
#include "cmd.h"
#include "util.h"

void Cmd::handleFileOp(const QMap<QString, QString> &cmdMapRef)
{
	using namespace CmdDef;

	for(const QString &keyRef : cmdMapRef.keys()) {
		QString value = cmdMapRef[keyRef];
		const QVector<QString> pair = { keyRef, value };

		if(isOkToExec(storeConfig, pair)) {
			QDomDocument doc;
			this->configAll.getXml(doc);
			QFile file(value);
			if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
				Util::log(
					this->isThrowEn ? LogType::CmdRespThrow : LogType::CmdResp,
					LogSt::Nok,
					storeConfig,
					value,
					"Failed to open file for writing."
				);
			} else {
				QTextStream out(&file);
				out << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>" << Qt::endl;
				out << doc.toString();
				file.close();
				Util::log(LogType::CmdResp, LogSt::Ok, storeConfig, value, "");
			}
		}

		if(isOkToExec(loadConfig, pair)) {
			QDomElement root = getConfigXmlRoot(value);
			if(root.isNull()) {
				// do nothing
			} else {
				configAll.setXml(root);
				emit configAllLoaded(configAll);
				Util::log(
					LogType::CmdResp,
					LogSt::Ok,
					loadConfig,
					value,
					""
				);
			}
		}
	}
}

QDomElement Cmd::getConfigXmlRoot(const QString &filePathRef)
{
	QDomElement root;
	QFile file(filePathRef);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		Util::log(
			LogType::CmdResp,
			LogSt::Nok,
			CmdDef::loadConfig,
			filePathRef,
			"Failed to open file for reading!"
		);
		return root;
	}

	QDomDocument doc;
	if (!doc.setContent(&file)) {
		Util::log(
			LogType::CmdResp,
			LogSt::Nok,
			CmdDef::loadConfig,
			filePathRef,
			"Failed to parse XML content from file!"
		);
		file.close();
		return root;
	}
	file.close();

	root = doc.documentElement();

	if (root.tagName() != this->configAll.name) {
		Util::log(
			LogType::CmdResp,
			LogSt::Nok,
			CmdDef::loadConfig,
			filePathRef,
			QString("Invalid XML root tag: %1!").arg(root.tagName())
		);
		root.clear();
	}

	//QXmlSchema schema;
	//if (!schema.load(this->configAll.xsdData)) {
	//	Util::log(
	//		LogType::CmdResp,
	//		LogSt::Nok,
	//		CmdDef::loadConfig,
	//		filePathRef,
	//		"Failed to load XSD schema from string."
	//	);
	//	return root;
	//}

	//QXmlSchemaValidator validator(schema);
	//if (!validator.validate(doc.toByteArray())) {
	//	Util::log(
	//		LogType::CmdResp,
	//		LogSt::Nok,
	//		CmdDef::loadConfig,
	//		filePathRef,
	//		"XML validation against XSD failed."
	//	);
	//	root.clear();
	//}

	return root;
}
