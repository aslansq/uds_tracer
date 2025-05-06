#include "cmddef.h"
#include <QRegularExpression>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include "util.h"

namespace CmdDef {
	const QString positiveNumberRegex("^[0-9]+$");
	const QString hexNumberRegex("^[0-9A-Fa-f]+$");
	QMap<QString, const Cmd *> allCommands = {};

	const QMap<ValueType, QString> valueTypeNames = {
		{ ValueType::PositiveNumber, "PositiveNumber" },
		{ ValueType::HexNumber, "HexNumber" },
		{ ValueType::ExistingFilePath, "ExistingFilePath" },
		{ ValueType::NewOrExistingFilePath, "NewOrExistingFilePath" },
		{ ValueType::ExistingDirPath, "ExistingDirPath" },
		{ ValueType::Empty, "Empty" },
		{ ValueType::None, "None" },
		{ ValueType::PossibleValues, "PossibleValues" },
		{ ValueType::Invalid, "Invalid" }
	};

	extern const QMap<Type, QString> typeNames = {
		{ Type::CanFdCfg, "CanFdCfg" },
		{ Type::CanStdCfg, "CanStdCfg" },
		{ Type::CanReplayCfg, "CanReplayCfg" },
		{ Type::TracerCfg, "TracerCfg" },
		{ Type::FileOp, "FileOperations" },
		{ Type::CanInterface, "CanInterface"},
		{ Type::Generic, "Generic" },
		{ Type::Unknown, "Unknown" }
	};

	QMap<Type, QVector<QString>> typeCmdNames = {};
	// Initialize the static member allNames
	QVector<QString> allNames = {};

	Cmd::Cmd(QObject *parent, Type type, ExecPermit execPermit) :
		QObject{parent},
		name(""),
		valueType(ValueType::Invalid),
		possibleValues({}),
		type(type),
		execPermit(execPermit)
	{
		fillAllNames();
	}
	
	Cmd::Cmd(const QString &nameRef, ValueType valueType, Type type, ExecPermit execPermit) :
		name(nameRef),
		valueType(valueType),
		possibleValues({}),
		type(type),
		execPermit(execPermit)
	{
		fillAllNames();
	}

	Cmd::Cmd(const QString &nameRef, const QVector<QString> &possibleValuesRef, Type type, ExecPermit execPermit) :
		name(nameRef),
		valueType(ValueType::PossibleValues),
		possibleValues(possibleValuesRef),
		type(type),
		execPermit(execPermit)
	{
		fillAllNames();
	}

	void Cmd::fillAllNames(void)
	{
		if(name.isEmpty()) {
			Util::log(LogType::GenericThrow, "Cmd name is empty, cannot fill all names.");
			return;
		}

		if(typeCmdNames.contains(type)) {
			typeCmdNames[type].append(name);
		} else {
			typeCmdNames[type] = { name };
		}

		if(!allNames.contains(name)) {
			allNames.append(name);
			allCommands.insert(name, static_cast<const Cmd *>(this));
		} else {
			Util::log(LogType::GenericThrow, QString("Cmd name already exists in allNames: %1").arg(name));
		}
	}

	bool isOkToExec(const Cmd &cmdRef, const QVector<QString> &nameValPairRef)
	{
		if(nameValPairRef.isEmpty()) {
			return false;
		}

		const QString &nameRef = nameValPairRef[0];
		const QString &value = nameValPairRef[1];

		if(nameRef != cmdRef.name) {
			return false;
		}

		if(!isCmdValueOk(cmdRef, value)) {
			return false;
		}

		return true;
	}

	bool isExecPermitted(const QString &nameRef, const QString &valueRef, bool isCanConnected)
	{
		if(!allCommands.contains(nameRef)) {
			Util::log(LogType::CmdResp, LogSt::Warn, "ignored " + nameRef + " " + valueRef);
			return false;
		}
		const Cmd *cmdPtr = allCommands[nameRef];

		Util::log(LogType::CmdReq, LogSt::Ok, *cmdPtr, valueRef, "");

		if(cmdPtr->execPermit == ExecPermit::Both) {
			return true;
		}

		if(isCanConnected == true && cmdPtr->execPermit == ExecPermit::Connected) {
			return true;
		}

		if(isCanConnected == false && cmdPtr->execPermit == ExecPermit::Disconnected) {
			return true;
		}

		Util::log(LogType::CmdResp, LogSt::Nok, *cmdPtr, valueRef, "");
		return false;
	}
	
	bool isCmdValueOk(const Cmd &cmdRef, QString value)
	{
		bool isOk = false;
		QRegularExpression regex;
		QRegularExpressionMatch match;
	
		switch (cmdRef.valueType) {
		case ValueType::PositiveNumber:
			regex.setPattern(positiveNumberRegex);
			match = regex.match(value);
			isOk = match.hasMatch();
			break;
		case ValueType::HexNumber:
			regex.setPattern(hexNumberRegex);
			match = regex.match(value);
			isOk = match.hasMatch();
			break;
		case ValueType::ExistingFilePath:
			isOk = QFile::exists(value);
			break;
		case ValueType::NewOrExistingFilePath:
			isOk = QFileInfo(value).dir().exists();
			break;
		case ValueType::ExistingDirPath:
			isOk = QDir(value).exists();
			break;
		case ValueType::Empty:
			isOk = value.isEmpty();
			break;
		case ValueType::None:
			isOk = true;
			break;
		case ValueType::PossibleValues:
			isOk = cmdRef.possibleValues.contains(value);
			break;
		case ValueType::Invalid:
			isOk = false;
			break;
		}

		if(!isOk) {
			Util::log(
				LogType::CmdResp,
				LogSt::Nok,
				cmdRef,
				value,
				QString("Expected: %1").arg(valueTypeNames[cmdRef.valueType])
			);
		}
	
		return isOk;
	}

	const Cmd devFd("devFd", ValueType::ExistingFilePath, Type::CanFdCfg, ExecPermit::Disconnected);
	const Cmd clkFreqMHz("clkFreqMHz", ValueType::PositiveNumber, Type::CanFdCfg, ExecPermit::Disconnected);
	const Cmd arbitBaud("arbitBaud", ValueType::PositiveNumber, Type::CanFdCfg, ExecPermit::Disconnected);
	const Cmd dataBaud("dataBaud", ValueType::PositiveNumber, Type::CanFdCfg, ExecPermit::Disconnected);
	const Cmd arbitPre("arbitPre", ValueType::PositiveNumber, Type::CanFdCfg, ExecPermit::Disconnected);
	const Cmd dataPre("dataPre", ValueType::PositiveNumber, Type::CanFdCfg, ExecPermit::Disconnected);
	const Cmd arbitTseg1("arbitTseg1", ValueType::PositiveNumber, Type::CanFdCfg, ExecPermit::Disconnected);
	const Cmd dataTseg1("dataTseg1", ValueType::PositiveNumber, Type::CanFdCfg, ExecPermit::Disconnected);
	const Cmd arbitTseg2("arbitTseg2", ValueType::PositiveNumber, Type::CanFdCfg, ExecPermit::Disconnected);
	const Cmd dataTseg2("dataTseg2", ValueType::PositiveNumber, Type::CanFdCfg, ExecPermit::Disconnected);
	const Cmd arbitSjw("arbitSjw", ValueType::PositiveNumber, Type::CanFdCfg, ExecPermit::Disconnected);
	const Cmd dataSjw("dataSjw", ValueType::PositiveNumber, Type::CanFdCfg, ExecPermit::Disconnected);

	const Cmd devStd("devStd", ValueType::ExistingFilePath, Type::CanStdCfg, ExecPermit::Disconnected);
	const Cmd baud("baud", ValueType::PositiveNumber, Type::CanStdCfg, ExecPermit::Disconnected);

	const Cmd devReplay("devReplay", ValueType::ExistingFilePath, Type::CanReplayCfg, ExecPermit::Disconnected);

	const Cmd reqIdHex("reqIdHex", ValueType::HexNumber, Type::TracerCfg, ExecPermit::Disconnected);
	const Cmd respIdHex("respIdHex", ValueType::HexNumber, Type::TracerCfg, ExecPermit::Disconnected);
	const Cmd logDirPath("logDirPath", ValueType::ExistingDirPath, Type::TracerCfg, ExecPermit::Disconnected);

	const Cmd storeConfig("storeConfig", ValueType::NewOrExistingFilePath, Type::FileOp, ExecPermit::Both);
	const Cmd loadConfig("loadConfig", ValueType::ExistingFilePath, Type::FileOp, ExecPermit::Disconnected);

	const Cmd connect("connect", { "on", "off" }, Type::CanInterface, ExecPermit::Disconnected);
	const Cmd canType("canType", {"Std", "Fd", "Replay"}, Type::CanInterface, ExecPermit::Disconnected);

}
