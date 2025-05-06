#ifndef CMDDEF_H
#define CMDDEF_H

#include <QObject>
#include <QString>
#include <QMap>

namespace CmdDef {
	enum class ValueType {
		PositiveNumber,
		HexNumber,
		ExistingFilePath,
		NewOrExistingFilePath,
		ExistingDirPath,
		Empty,
		None,
		PossibleValues,
		Invalid
	};

	enum class Type {
		CanFdCfg,
		CanStdCfg,
		CanReplayCfg,
		TracerCfg,
		FileOp,
		CanInterface,
		Generic,
		Unknown
	};

	enum class ExecPermit {
		Disconnected,
		Connected,
		Both,
		None
	};

	extern const QMap<Type, QString> typeNames;
	extern const QMap<ValueType, QString> valueTypeNames;
	extern QMap<Type, QVector<QString>> typeCmdNames;
	// Initialize the static member allNames
	extern QVector<QString> allNames;

	class Cmd : public QObject
	{
		Q_OBJECT
	public:
		explicit Cmd(QObject *parent, Type type, ExecPermit execPermit);
		explicit Cmd(const QString &nameRef, ValueType valueType, Type type, ExecPermit execPermit);
		explicit Cmd(const QString &nameRef, const QVector<QString> &possibleValuesRef, Type type, ExecPermit execPermit);
		const QString name;
		const ValueType valueType;
		const QVector<QString> possibleValues;
		const Type type;
		const ExecPermit execPermit;

		void fillAllNames(void);
	};

	extern QMap<QString, const Cmd *> allCommands;

	bool isExecPermitted(const QString &nameRef, const QString &valueRef, bool isCanConnected);
	bool isOkToExec(const Cmd &cmdRef, const QVector<QString> &nameValPairRef);
	bool isCmdValueOk(const Cmd &cmdRef, QString value);

	// Can Flexray Configuration commands
	extern const Cmd devFd;
	extern const Cmd clkFreqMHz;
	extern const Cmd arbitBaud;
	extern const Cmd dataBaud;
	extern const Cmd arbitPre;
	extern const Cmd dataPre;
	extern const Cmd arbitTseg1;
	extern const Cmd dataTseg1;
	extern const Cmd arbitTseg2;
	extern const Cmd dataTseg2;
	extern const Cmd arbitSjw;
	extern const Cmd dataSjw;
	// Can Standard Configuration commands
	extern const Cmd devStd;
	extern const Cmd baud;
	// Can Replay Configuration commands
	extern const Cmd devReplay;
	// Tracer Configuration commands
	extern const Cmd reqIdHex;
	extern const Cmd respIdHex;
	extern const Cmd logDirPath;
	// File op commands
	extern const Cmd storeConfig;
	extern const Cmd loadConfig;
	// Can Interface commands
	extern const Cmd connect;
	extern const Cmd canType;
}

#endif // CMDDEF_H
