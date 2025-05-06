#ifndef UDS_DEF_H
#define UDS_DEF_H

#include <QMap>
#include <QString>
#include <QVector>

namespace UdsDef {
	enum class ServEnum {
		diagSessCtrl = 0x10, // DiagnosticSessionControl
		ecuReset = 0x11, // ECUReset
		securityAccess = 0x27, // SecurityAccess
		commCtrl = 0x28, // CommunicationControl
		testerPresent = 0x3E, // TesterPresent
		auth = 0x29, // Authentication
		securedDataTx = 0x84, // SecuredDataTransmission
		ctrlDtcSettings = 0x85, // ControlDTCSetting
		respOnEvent = 0x86, // ResponseOnEvent
		linkCtrl = 0x87, // LinkControl
		readDataById = 0x22, // ReadDataByIdentifier
		readMemByAddr = 0x23, // ReadMemoryByAddress
		readScalingDataById = 0x24, // ReadScalingDataByIdentifier
		readDataByPeriodicId = 0x2A, // ReadDataByPeriodicIdentifier
		dynDefDataId = 0x2C, // DynamicallyDefineDataIdentifier
		writeDataById = 0x2E, // WriteDataByIdentifier
		writeMemByAddr = 0x3D, // WriteMemoryByAddress
		clearDiagInfo = 0x14, // ClearDiagnosticInformation
		readDtcInfo = 0x19, // ReadDTCInformation
		ioCtrlById = 0x2F, // InputOutputControlByIdentifier
		routineCtrl = 0x31, // RoutineControl
		routineDownload = 0x34, // RequestDownload
		reqUpload = 0x35, // RequestUpload
		transferData = 0x36, // TransferData
		reqTransferExit = 0x37, // RequestTransferExit
		reqFileTransfer = 0x38, // RequestFileTransfer
		negResp = 0x7f, // NegativeResponce
	};


	enum class ServSubDiagSessCtrl {
		reserved1 = 0x00, // ISOSAEReserved
		def = 0x01, // defaultSession
		prog = 0x02, // ProgrammingSession
		extdDiag = 0x03, // Extended Diagnostic Session
		safetySysDiag = 0x04, // Safety System Diagnostic Session
		reserved2Start = 0x05, // ISOSAEReserved
		reserved2End = 0x3F, // ISOSAEReserved
		vehicleManufacturerSpecificStart = 0x40, // Vehicle Manufacturer Specific
		vehicleManufacturerSpecificEnd = 0x5F, // Vehicle Manufacturer Specific
		sysSupplierSpecificStart = 0x60, // System Supplier Specific
		sysSupplierSpecificEnd = 0x7E, // System Supplier Specific
		reserved3 = 0x7F, // ISOSAEReserved
	};

	enum class ServSubEcuReset {
		reserved1 = 0x00, // ISOSAEReserved
		hardReset = 0x01, // hardReset
		keyOffOnReset = 0x02, // keyOffOnReset
		softReset = 0x03, // softReset
		enRapidPwrShutDown = 0x04, // enableRapidPowerShutDown
		disableRapidPwrShutdown = 0x05, // disableRapidPowerShutDown
		reserved2Start = 0x06, // ISOSAEReserved
		reserved2End = 0x3F, // ISOSAEReserved
		vehicleManufacturerSpecificStart = 0x40, // vehicleManufacturerSpecific
		vehicleManufacturerSpecificEnd = 0x5F, // vehicleManufacturerSpecific
		sysSuppSpecificStart = 0x60, // systemSupplierSpecific
		sysSuppSpecificEnd = 0x7E, // systemSupplierSpecific
		reserved3 = 0x7F, // ISOSAEReserved
	};

	enum class ServSubSecurityAccess {
		reserved1 = 0x00, // ISOSAEReserved
		reqSeedDef = 0x01, // requestSeed
		sendKeyDef = 0x02, // sendKey
		reqSeedLevel = 0x03, // requestSeed
		reqSeedLevel1 = 0x05, // requestSeed
		reqSeedLevel2Start = 0x07, // requestSeed
		reqSeedLevel2End = 0x41, // requestSeed
		sendKeyLevel = 0x04, // sendKey
		sendKeyLevel1 = 0x06, // sendKey
		sendKeyLevel2Start = 0x08, // sendKey
		sendKeyLevel2End = 0x42, // sendKey
		reserved2Start = 0x43, // ISOSAEReserved
		reserved2End = 0x5E, // ISOSAEReserved
		iso26021ReqSeed = 0x5F, // ISO26021-2 values
		iso26021SendKey = 0x60, // ISO26021-2 sendKey values
		sysSuppSpecificStart = 0x61, // systemSupplierSpecific
		sysSuppSpecificEnd = 0x7E, // systemSupplierSpecific
		reserved3 = 0x7F, // ISOSAEReserved
	};

	enum class ServSubCommCtrl {
		enRxAndTx = 0x00, // enableRxAndTx
		enRxAndDisTx = 0x01, // enableRxAndDisableTx
		disRxAndEnTx = 0x02, // disableRxAndEnableTx
		disRxAndTx = 0x03, // disableRxAndTx
		enRxAndDisTxWEnchancedAddrInfo = 0x04, // enableRxAndDisableTxWithEnhancedAddressInformation
		enRxAndTxWEnhancedAddrInfo = 0x05, // enableRxAndTxWithEnhancedAddressInformation
		reserved1Start = 0x06, // ISOSAEReserved
		reserved1End = 0x3F, // ISOSAEReserved
		vehicleMfrSpecificStart = 0x40, // vehicleManufacturerSpecific
		vehicleMfrSpecificEnd = 0x5F, // vehicleManufacturerSpecific
		sysSuppSpecificStart = 0x60, // systemSupplierSpecific
		sysSuppSpecificEnd = 0x7E, // systemSupplierSpecific
		reserved2 = 0x7F, // ISOSAEReserved
	};

	enum class ServSubTesterPresent {
		none = 0x00, // zeroSubFunction
		reserved1Start = 0x01, // ISOSAEReserved
		reserved1End = 0x7F, // ISOSAEReserved
	};

	enum class ServSubReadDtcInfo {
		reserved1 = 0x00, // ISOSAEReserved
		reportNumOfDtcByStatusMask = 0x01, // reportNumberOfDTCByStatusMask
		reportDtcByStatusMask = 0x02, // reportDTCByStatusMask
		reportDtcSnapshotId = 0x03, // reportDTCSnapshotIdentification
		reportDtcSnapshotRecordByDtcNum = 0x04, // reportDTCSnapshotRecordByDTCNumber
		reportDtcStoredDataByRecordNum = 0x05, // reportDTCStoredDataByRecordNumber
		reportDtcExtDataRecordByDtcNum = 0x06, // reportDTCExtDataRecordByDTCNumber
		reportNumOfDtcBySeverityMaskRecord = 0x07, // reportNumberOfDTCBySeverityMaskRecord
		reportDtcBySeverityMaskRecord = 0x08, // reportDTCBySeverityMaskRecord
		reportSeverityInformationOfDtc = 0x09, // reportSeverityInformationOfDTC
		reportSuppDtc = 0x0A, // reportSupportedDTC
		reportFirstTestFailedDtc = 0x0B, // reportFirstTestFailedDTC
		reportFirstConfirmedDtc = 0x0C, // reportFirstConfirmedDTC
		reportMostRecentTestFailedDtc = 0x0D, // reportMostRecentTestFailedDTC
		reportMostRecentConfirmedDtc = 0x0E, // reportMostRecentConfirmedDTC
		reportDtcFaultDetectionCounter = 0x14, // reportDTCFaultDetectionCounter
		reportDtcWithPermanentStatus = 0x15, // reportDTCWithPermanentStatus
		reportDtcExtDataRecordByRecordNumber = 0x16, // reportDTCExtDataRecordByRecordNumber
		reportUserDefMemDtcByStatusMask = 0x17, // reportUserDefMemoryDTCByStatusMask
		reportUserDefMemDtcSnapshotRecordByDtcNum = 0x18, // reportUserDefMemoryDTCSnapshotRecordByDTCNumber
		reportUserDefMemDtcExtDataRecordByDtcNum = 0x19, // reportUserDefMemoryDTCExtDataRecordByDTCNumber
		reportDtcExtendedDataRecordId = 0x1A, // reportDTCExtendedDataRecordIdentification
		reserved2Start = 0x1B, // ISOSAEReserved
		reserved2End = 0x41, // ISOSAEReserved
		reportWWHOBDDTCByMaskRecord = 0x42, // reportWWHOBDDTCByMaskRecord
		reserved3Start = 0x43, // ISOSAEReserved
		reserved3End = 0x54, // ISOSAEReserved
		reportWWHOBDDTCWithPermanentStatus = 0x55, // reportWWHOBDDTCWithPermanentStatus
		reportDtcInfoByDtcReadinessGroupId = 0x56, // reportDTCInformationByDTCReadinessGroupIdentifier
		reserved4Start = 0x57, // ISOSAEReserved
		reserved4End = 0x7F, // ISOSAEReserved
	};

	enum class ServSubRoutineCtrl {
		reserved1 = 0x00, // ISOSAEReserved
		startRoutine = 0x01, // startRoutine
		stopRoutine = 0x02, // stopRoutine
		requestRoutineResults = 0x03, // requestRoutineResults
		reserved2Start = 0x04, // ISOSAEReserved
		reserved2End = 0x7F, // ISOSAEReserved
	};

	enum class ServSubNegResp {
		posResp = 0x00, // positiveResponse
		reserved1Start = 0x01, // ISOSAEReserved
		reserved1End = 0x0F, // ISOSAEReserved
		genReject = 0x10, // generalReject
		servNotSupp = 0x11, // serviceNotSupported
		subFuncNotSupp = 0x12, // SubFunctionNotSupported
		incorrectMsg = 0x13, // incorrectMessageLengthOrInvalidFormat
		respTooLong = 0x14, // responseTooLong
		reserved2Start = 0x15, // ISOSAEReserved
		reserved2End = 0x20, // ISOSAEReserved
		busyRepeat = 0x21, // busyRepeatRequest
		condNotCorrect = 0x22, // conditionsNotCorrect
		reserved3 = 0x23, // ISOSAEReserved
		reqSeqErr = 0x24, // requestSequenceError
		noRespFromSubComp = 0x25, // noResponseFromSubnetComponent
		failPreventsExec = 0x26, // FailurePreventsExecutionOfRequestedAction
		reserved4Start = 0x27, // ISOSAEReserved
		reserved4End = 0x30, // ISOSAEReserved
		reqOutOfRange = 0x31, // requestOutOfRange
		reserved5 = 0x32, // ISOSAEReserved
		securityAccessDenied = 0x33, // securityAccessDenied
		authRequried = 0x34, // authenticationRequired
		invavaliKKey = 0x35, // invalidKey
		exceedNumOfAttemps = 0x36, // exceedNumberOfAttempts
		requiredTimeDelayNotExpired = 0x37, // requiredTimeDelayNotExpired
		secureDataTxRequired = 0x38, // secureDataTransmissionRequired
		secureDataTxNotAllowed = 0x39, // secureDataTransmissionNotAllowed
		secureDataVerifFail = 0x3A, // secureDataVerificationFailed
		reserved6Start = 0x3B, // ISOSAEReserved
		reserved6End = 0x4F, // ISOSAEReserved
		certifInvalidPeriod = 0x50, // Certificate verification failed - Invalid Time Period
		certifInvalidSig = 0x51, // Certificate verification failed - Invalid Signature
		certifInvalidChainOfTrust = 0x52, // Certificate verification failed - Invalid Chain of Trust
		certifInvalidType = 0x53, // Certificate verification failed - Invalid Type
		certifInvalidFormat = 0x54, // Certificate verification failed - Invalid Format
		certifInvalidContent = 0x55, // Certificate verification failed - Invalid Content
		certifInvalidScope = 0x56, // Certificate verification failed - Invalid Scope
		certifInvalidCertif = 0x57, // Certificate verification failed – Invalid Certificate (revoked)
		ownerVerifFail = 0x58, // Ownership verification failed
		challengeCalcFail = 0x59, // Challenge calculation failed
		setAccessRightFail = 0x5A, // Setting Access Rights failed
		sessKeyCreateFail = 0x5B, // Session key creation/derivation failed
		confDataUsageFail = 0x5C, // Configuration data usage failed
		deAuthFail = 0x5D, // DeAuthentication failed
		reserved7Start = 0x5E, // ISOSAEReserved
		reserved7End = 0x6F, // ISOSAEReserved
		upDownNotAccept = 0x70, // uploadDownloadNotAccepted
		txDataSuspend = 0x71, // transferDataSuspended
		genProgFail = 0x72, // generalProgrammingFailure
		wrongBlockSeqCounter = 0x73, // wrongBlockSequenceCounter
		reserved8Start = 0x74, // ISOSAEReserved
		reserved8End = 0x77, // ISOSAEReserved
		reqCorrectRxRespPending = 0x78, // requestCorrectlyReceived-ResponsePending
		reserved9Start = 0x79, // ISOSAEReserved
		reserved9End = 0x7D, // ISOSAEReserved
		subFuncNotActInCurrSess = 0x7E, // SubFunctionNotSupportedInActiveSession
		servNotActInCurrSess = 0x7F, // serviceNotSupportedInActiveSession
		reserved10 = 0x80, // ISOSAEReserved
		rpmTooHigh = 0x81, // rpmTooHigh
		rpmTooLow = 0x82, // rpmTooLow
		engineRunning = 0x83, // engineIsRunning
		engineNotRunning = 0x84, // engineIsNotRunning
		engineRunTimeLow = 0x85, // engineRunTimeTooLow
		tempTooHigh = 0x86, // temperatureTooHigh
		tempTooLow = 0x87, // temperatureTooLow
		vehicleSpeedTooHigh = 0x88, // vehicleSpeedTooHigh
		vehicleSpeedTooLow = 0x89, // vehicleSpeedTooLow
		throttleTooHigh = 0x8A, // throttle/PedalTooHigh
		throttleTooLow = 0x8B, // throttle/PedalTooLow
		transmissionNotInNeutral = 0x8C, // transmissionRangeNotInNeutral
		transmissionNotInGear = 0x8D, // transmissionRangeNotInGear
		reserved11 = 0x8E, // ISOSAEReserved
		brakeNotApplied = 0x8F, // brakeSwitch(es)NotClosed (Brake Pedal not pressed or not applied)
		shifterLevelNotInPark = 0x90, // shifterLeverNotInPark
		torqConvClutchLocked = 0x91, // torqueConverterClutchLocked
		voltageTooHigh = 0x92, // voltageTooHigh
		voltageTooLow = 0x93, // voltageTooLow
		resourceTempNotAvail = 0x94, // ResourceTemporarilyNotAvailable
		reservedForSpecificCondsNotCorrectStart = 0x95, // reservedForSpecificConditionsNotCorrect
		reservedForSpecificCondsNotCorrectEnd = 0xEF, // reservedForSpecificConditionsNotCorrect
		vehicleMfrSpecificCondsNotCorrectStart = 0xF0, // vehicleManufacturerSpecificConditionsNotCorrect
		vehicleMfrSpecificCondsNotCorrectEnd = 0xFE, // vehicleManufacturerSpecificConditionsNotCorrect
		reserved12 = 0xFF, // ISOSAEReserved
	};

	class SecAccGenericReq {
	public:
		SecAccGenericReq(const QVector<uint8_t> &packetRef);
		bool existSid(void) const;
		uint8_t getSid(void) const;
		bool existSubFunc(void) const;
		uint8_t getSubFunc(void) const;
		bool existData(void) const;
		void getData(QVector<uint8_t> &outRef) const;
		const uint8_t sidPos = 0;
		const uint8_t subFuncPos = 1;
		const uint8_t dataPos = 2;
		const uint8_t sidSize = 1;
		const uint8_t subFuncSize = 1;
		const QVector<uint8_t> packet;
	};

	class SecAccReqSeedReq {
	public:
		SecAccReqSeedReq(const QVector<uint8_t> &packetRef);
		bool existSid(void) const;
		uint8_t getSid(void) const;
		bool existReqSeed(void) const;
		uint8_t getReqSeed(void) const;
		bool existSeed(void) const;
		void getSeed(QVector<uint8_t> &outRef) const;
		const uint8_t sidPos = 0;
		const uint8_t reqSeedPos = 1;
		const uint8_t seedPos = 2;
		const uint8_t sidSize = 1;
		const uint8_t reqSeedSize = 1;
		const QVector<uint8_t> packet;
	};

	class SecAccSendKeyReq {
	public:
		SecAccSendKeyReq(const QVector<uint8_t> &packetRef);
		bool existSid(void) const;
		uint8_t getSid(void) const;
		bool existSendKey(void) const;
		uint8_t getSendKey(void) const;
		bool existKey(void) const;
		void getKey(QVector<uint8_t> &outRef) const;
		const uint8_t sidPos = 0;
		const uint8_t sendKeyPos = 1;
		const uint8_t keyPos = 2;
		const uint8_t sidSize = 1;
		const uint8_t sendKeySize = 1;
		const QVector<uint8_t> packet;
	};

	class SecAccPosResp {
	public:
		SecAccPosResp(const QVector<uint8_t> &packetRef);
		bool existSid(void) const;
		uint8_t getSid(void) const;
		bool existSubFunc(void) const;
		uint8_t getSubFunc(void) const;
		bool existSecSeed(void) const;
		void getSecSeed(QVector<uint8_t> &outRef) const;
		const uint8_t sidPos = 0;
		const uint8_t subFuncPos = 1;
		const uint8_t secSeedPos = 2;
		const uint8_t sidSize = 1;
		const uint8_t subFuncSize = 1;
		const QVector<uint8_t> packet;
	};

	class DiagSessCtrlReq {
	public:
		DiagSessCtrlReq(const QVector<uint8_t> &packetRef);
		bool existSid(void) const;
		uint8_t getSid(void) const;
		bool existSubFunc(void) const;
		uint8_t getSubFunc(void) const;
		const uint8_t sidPos = 0;
		const uint8_t subFuncPos = 1;
		const uint8_t sidSize = 1;
		const uint8_t subFuncSize = 1;
		const QVector<uint8_t> packet;
	};

	class DiagSessCtrlPosResp {
	public:
		DiagSessCtrlPosResp(const QVector<uint8_t> &packetRef);
		bool existSid(void) const;
		uint8_t getSid(void) const;
		bool existSubFunc(void) const;
		uint8_t getSubFunc(void) const;
		bool existSessParam(void) const;
		uint32_t getSessParam(void) const;
		const uint8_t sidPos = 0;
		const uint8_t subFuncPos = 1;
		const uint8_t sessParamPos = 2;
		const uint8_t sidSize = 1;
		const uint8_t subFuncSize = 1;
		const uint8_t sessParamSize = 4;
		const QVector<uint8_t> packet;
	};

	class EcuResetReq {
	public:
		EcuResetReq(const QVector<uint8_t> &packetRef);
		bool existSid(void) const;
		uint8_t getSid(void) const;
		bool existSubFunc(void) const;
		uint8_t getSubFunc(void) const;
		const uint8_t sidPos = 0;
		const uint8_t subFuncPos = 1;
		const uint8_t sidSize = 1;
		const uint8_t subFuncSize = 1;
		const QVector<uint8_t> packet;
	};

	class EcuResetPosResp {
	public:
		EcuResetPosResp(const QVector<uint8_t> &packetRef);
		bool existSid(void) const;
		uint8_t getSid(void) const;
		bool existSubFunc(void) const;
		uint8_t getSubFunc(void) const;
		bool existPowerDownTime(void) const;
		uint8_t getPowerDownTime(void) const;
		const uint8_t sidPos = 0;
		const uint8_t subFuncPos = 1;
		const uint8_t powerDownTimePos = 2;
		const uint8_t sidSize = 1;
		const uint8_t subFuncSize = 1;
		const uint8_t powerDownTimeSize = 1;
		const QVector<uint8_t> packet;
	};

	class TesterPresentReq {
	public:
		TesterPresentReq(const QVector<uint8_t> &packetRef);
		bool existSid(void) const;
		uint8_t getSid(void) const;
		bool existSubFunc(void) const;
		uint8_t getSubFunc(void) const;
		const uint8_t sidPos = 0;
		const uint8_t subFuncPos = 1;
		const uint8_t sidSize = 1;
		const uint8_t subFuncSize = 1;
		const QVector<uint8_t> packet;
	};

	class TesterPresentPosResp {
	public:
		TesterPresentPosResp(const QVector<uint8_t> &packetRef);
		bool existSid(void) const;
		uint8_t getSid(void) const;
		bool existSubFunc(void) const;
		uint8_t getSubFunc(void) const;
		const uint8_t sidPos = 0;
		const uint8_t subFuncPos = 1;
		const uint8_t sidSize = 1;
		const uint8_t subFuncSize = 1;
		const QVector<uint8_t> packet;
	};

	class ReadDataByIdReq {
	public:
		ReadDataByIdReq(const QVector<uint8_t> &packetRef);
		bool existSid(void) const;
		uint8_t getSid(void) const;
		bool existDataId(void) const;
		void getDataId(QVector<uint8_t> &outRef) const;
		const uint8_t sidPos = 0;
		const uint8_t dataIdPos = 1;
		const uint8_t sidSize = 1;
		const QVector<uint8_t> packet;
	};

	class ReadDataByIdPosResp {
	public:
		ReadDataByIdPosResp(const QVector<uint8_t> &packetRef);
		bool existSid(void) const;
		uint8_t getSid(void) const;
		bool existData(void) const;
		void getData(QVector<uint8_t> &outRef) const;
		const uint8_t sidPos = 0;
		const uint8_t dataPos = 1;
		const uint8_t sidSize = 1;
		const QVector<uint8_t> packet;
	};

	class WriteDataByIdReq {
	public:
		WriteDataByIdReq(const QVector<uint8_t> &packetRef);
		bool existSid(void) const;
		uint8_t getSid(void) const;
		bool existDataId(void) const;
		uint16_t getDataId(void) const;
		bool existDataRecord(void) const;
		void getDataRecord(QVector<uint8_t> &outRef) const;
		const uint8_t sidPos = 0;
		const uint8_t dataIdPos = 1;
		const uint8_t dataRecordPos = 3;
		const uint8_t sidSize = 1;
		const uint8_t dataIdSize = 2;
		const QVector<uint8_t> packet;
	};

	class WriteDataByIdPosResp {
	public:
		WriteDataByIdPosResp(const QVector<uint8_t> &packetRef);
		bool existSid(void) const;
		uint8_t getSid(void) const;
		bool existDataId(void) const;
		uint16_t getDataId(void) const;
		const uint8_t sidPos = 0;
		const uint8_t dataIdPos = 1;
		const uint8_t sidSize = 1;
		const uint8_t dataIdSize = 2;
		const QVector<uint8_t> packet;
	};

	class RoutineCtrlReq {
	public:
		RoutineCtrlReq(const QVector<uint8_t> &packetRef);
		bool existSid(void) const;
		uint8_t getSid(void) const;
		bool existSubFunc(void) const;
		uint8_t getSubFunc(void) const;
		bool existRid(void) const;
		uint16_t getRid(void) const;
		bool existOptRecord(void) const;
		void getOptRecord(QVector<uint8_t> &outRef) const;
		const uint8_t sidPos = 0;
		const uint8_t subFuncPos = 1;
		const uint8_t ridPos = 2;
		const uint8_t optRecordPos = 4;
		const uint8_t sidSize = 1;
		const uint8_t subFuncSize = 1;
		const uint8_t ridSize = 2;
		const QVector<uint8_t> packet;
	};

	class RoutineCtrlPosResp {
	public:
		RoutineCtrlPosResp(const QVector<uint8_t> &packetRef);
		bool existSid(void) const;
		uint8_t getSid(void) const;
		bool existType(void) const;
		uint8_t getType(void) const;
		bool existRid(void) const;
		uint16_t getRid(void) const;
		bool existInfo(void) const;
		uint8_t getInfo(void) const;
		bool existStatusRecord(void) const;
		void getStatusRecord(QVector<uint8_t> &outRef) const;
		const uint8_t sidPos = 0;
		const uint8_t typePos = 1;
		const uint8_t ridPos = 2;
		const uint8_t infoPos = 4;
		const uint8_t statusRecordPos = 5;
		const uint8_t sidSize = 1;
		const uint8_t typeSize = 1;
		const uint8_t ridSize = 2;
		const uint8_t infoSize = 1;
		const QVector<uint8_t> packet;
	};

	class NegResp {
	public:
		NegResp(const QVector<uint8_t> &packetRef);
		bool existSid(void) const;
		uint8_t getSid(void) const;
		bool existReqSid(void) const;
		uint8_t getReqSid(void) const;
		bool existNrc(void) const;
		uint8_t getNrc(void) const;
		const uint8_t sidPos = 0;
		const uint8_t reqSidPos = 1;
		const uint8_t nrcPos = 2;
		const uint8_t sidSize = 1;
		const uint8_t reqSidSize = 1;
		const uint8_t nrcSize = 1;
		const QVector<uint8_t> packet;
	};

	class CommCtrlReq {
	public:
		CommCtrlReq(const QVector<uint8_t> &packetRef);
		bool existSid(void) const;
		uint8_t getSid(void) const;
		bool existSubFunc(void) const;
		uint8_t getSubFunc(void) const;
		bool existCommType(void) const;
		uint8_t getCommType(void) const;
		bool existNodeIdHigh(void) const;
		uint8_t getNodeIdHigh(void) const;
		bool existNodeIdLow(void) const;
		uint8_t getNodeIdLow(void) const;
		const uint8_t sidPos = 0;
		const uint8_t subFuncPos = 1;
		const uint8_t commTypePos = 2;
		const uint8_t nodeIdHighPos = 3;
		const uint8_t nodeIdLowPos = 4;
		const uint8_t sidSize = 1;
		const uint8_t subFuncSize = 1;
		const uint8_t commTypeSize = 1;
		const uint8_t nodeIdHighSize = 1;
		const uint8_t nodeIdLowSize = 1;
		const QVector<uint8_t> packet;
	};

	class TransferDataReq {
	public:
		TransferDataReq(const QVector<uint8_t> &packetRef);
		bool existSid(void) const;
		uint8_t getSid(void) const;
		bool existBsc(void) const;
		uint8_t getBsc(void) const;
		bool existData(void) const;
		void getData(QVector<uint8_t> &outRef) const;
		const uint8_t sidPos = 0;
		const uint8_t bscPos = 1;
		const uint8_t dataPos = 2;
		const uint8_t sidSize = 1;
		const uint8_t bscSize = 1;
		const QVector<uint8_t> packet;
	};

	class TransferDataPosResp {
	public:
		TransferDataPosResp(const QVector<uint8_t> &packetRef);
		bool existSid(void) const;
		uint8_t getSid(void) const;
		bool existBsc(void) const;
		uint8_t getBsc(void) const;
		bool existParamRecord(void) const;
		void getParamRecord(QVector<uint8_t> &outRef) const;
		const uint8_t sidPos = 0;
		const uint8_t bscPos = 1;
		const uint8_t paramRecordPos = 2;
		const uint8_t sidSize = 1;
		const uint8_t bscSize = 1;
		const QVector<uint8_t> packet;
	};


	class ServSub {
	public:
		ServSub(
			uint8_t id,
			const QString &shortName,
			const QString &detail
		);
		ServSub(
			uint8_t idStart,
			uint8_t idEnd,
			const QString &shortName,
			const QString &detail
		);
		bool isThis(uint8_t id) const;
		const uint8_t idStart;
		const uint8_t idEnd;
		const QString shortName;
		const QString detail;
		const bool isRange;
	};

	class Serv {
	public:
		Serv(
			ServEnum id,
			const QString &shortName,
			const QVector<const ServSub *> *sub
		);
		const ServEnum id;
		const QString shortName;
		const QVector<const ServSub *> *sub;
	};


	extern const ServSub servDiagSessCtrlSubReserved1;
	extern const ServSub servDiagSessCtrlSubDef;
	extern const ServSub servDiagSessCtrlSubProg;
	extern const ServSub servDiagSessCtrlSubExtdDiag;
	extern const ServSub servDiagSessCtrlSubSafetySysDiag;
	extern const ServSub servDiagSessCtrlSubReserved2;
	extern const ServSub servDiagSessCtrlSubVehicleManufacturerSpecific;
	extern const ServSub servDiagSessCtrlSubSysSupplierSpecific;
	extern const ServSub servDiagSessCtrlSubReserved3;

	extern const ServSub servEcuResetSubReserved1;
	extern const ServSub servEcuResetSubHardReset;
	extern const ServSub servEcuResetSubKeyOffOnReset;
	extern const ServSub servEcuResetSubSoftReset;
	extern const ServSub servEcuResetSubEnRapidPwrShutDown;
	extern const ServSub servEcuResetSubDisableRapidPwrShutdown;
	extern const ServSub servEcuResetSubReserved2;
	extern const ServSub servEcuResetSubVehicleManufacturerSpecific;
	extern const ServSub servEcuResetSubSysSuppSpecific;
	extern const ServSub servEcuResetSubReserved3;

	extern const ServSub servSecurityAccessSubReserved1;
	extern const ServSub servSecurityAccessSubReqSeedDef;
	extern const ServSub servSecurityAccessSubSendKeyDef;
	extern const ServSub servSecurityAccessSubReqSeedLevel;
	extern const ServSub servSecurityAccessSubReqSeedLevel1;
	extern const ServSub servSecurityAccessSubReqSeedLevel2;
	extern const ServSub servSecurityAccessSubSendKeyLevel;
	extern const ServSub servSecurityAccessSubSendKeyLevel1;
	extern const ServSub servSecurityAccessSubSendKeyLevel2;
	extern const ServSub servSecurityAccessSubReserved2;
	extern const ServSub servSecurityAccessSubIso26021ReqSeed;
	extern const ServSub servSecurityAccessSubIso26021SendKey;
	extern const ServSub servSecurityAccessSubSysSuppSpecific;
	extern const ServSub servSecurityAccessSubReserved3;

	extern const ServSub servCommCtrlSubEnRxAndTx;
	extern const ServSub servCommCtrlSubEnRxAndDisTx;
	extern const ServSub servCommCtrlSubDisRxAndEnTx;
	extern const ServSub servCommCtrlSubDisRxAndTx;
	extern const ServSub servCommCtrlSubEnRxAndDisTxWEnchancedAddrInfo;
	extern const ServSub servCommCtrlSubEnRxAndTxWEnhancedAddrInfo;
	extern const ServSub servCommCtrlSubReserved1;
	extern const ServSub servCommCtrlSubVehicleMfrSpecific;
	extern const ServSub servCommCtrlSubSysSuppSpecific;
	extern const ServSub servCommCtrlSubReserved2;

	extern const ServSub servTesterPresentSubNone;
	extern const ServSub servTesterPresentSubReserved1;

	extern const ServSub servReadDtcInfoSubReserved1;
	extern const ServSub servReadDtcInfoSubReportNumOfDtcByStatusMask;
	extern const ServSub servReadDtcInfoSubReportDtcByStatusMask;
	extern const ServSub servReadDtcInfoSubReportDtcSnapshotId;
	extern const ServSub servReadDtcInfoSubReportDtcSnapshotRecordByDtcNum;
	extern const ServSub servReadDtcInfoSubReportDtcStoredDataByRecordNum;
	extern const ServSub servReadDtcInfoSubReportDtcExtDataRecordByDtcNum;
	extern const ServSub servReadDtcInfoSubReportNumOfDtcBySeverityMaskRecord;
	extern const ServSub servReadDtcInfoSubReportDtcBySeverityMaskRecord;
	extern const ServSub servReadDtcInfoSubReportSeverityInformationOfDtc;
	extern const ServSub servReadDtcInfoSubReportSuppDtc;
	extern const ServSub servReadDtcInfoSubReportFirstTestFailedDtc;
	extern const ServSub servReadDtcInfoSubReportFirstConfirmedDtc;
	extern const ServSub servReadDtcInfoSubReportMostRecentTestFailedDtc;
	extern const ServSub servReadDtcInfoSubReportMostRecentConfirmedDtc;
	extern const ServSub servReadDtcInfoSubReportDtcFaultDetectionCounter;
	extern const ServSub servReadDtcInfoSubReportDtcWithPermanentStatus;
	extern const ServSub servReadDtcInfoSubReportDtcExtDataRecordByRecordNumber;
	extern const ServSub servReadDtcInfoSubReportUserDefMemDtcByStatusMask;
	extern const ServSub servReadDtcInfoSubReportUserDefMemDtcSnapshotRecordByDtcNum;
	extern const ServSub servReadDtcInfoSubReportUserDefMemDtcExtDataRecordByDtcNum;
	extern const ServSub servReadDtcInfoSubReportDtcExtendedDataRecordId;
	extern const ServSub servReadDtcInfoSubReserved2;
	extern const ServSub servReadDtcInfoSubReportWWHOBDDTCByMaskRecord;
	extern const ServSub servReadDtcInfoSubReserved3;
	extern const ServSub servReadDtcInfoSubReportWWHOBDDTCWithPermanentStatus;
	extern const ServSub servReadDtcInfoSubReportDtcInfoByDtcReadinessGroupId;
	extern const ServSub servReadDtcInfoSubReserved4;

	extern const ServSub servRoutineCtrlSubReserved1;
	extern const ServSub servRoutineCtrlSubStartRoutine;
	extern const ServSub servRoutineCtrlSubStopRoutine;
	extern const ServSub servRoutineCtrlSubRequestRoutineResults;
	extern const ServSub servRoutineCtrlSubReserved2;

	extern const ServSub servNegRespSubPosResp;
	extern const ServSub servNegRespSubReserved1;
	extern const ServSub servNegRespSubGenReject;
	extern const ServSub servNegRespSubServNotSupp;
	extern const ServSub servNegRespSubSubFuncNotSupp;
	extern const ServSub servNegRespSubIncorrectMsg;
	extern const ServSub servNegRespSubRespTooLong;
	extern const ServSub servNegRespSubReserved2;
	extern const ServSub servNegRespSubBusyRepeat;
	extern const ServSub servNegRespSubCondNotCorrect;
	extern const ServSub servNegRespSubReserved3;
	extern const ServSub servNegRespSubReqSeqErr;
	extern const ServSub servNegRespSubNoRespFromSubComp;
	extern const ServSub servNegRespSubFailPreventsExec;
	extern const ServSub servNegRespSubReserved4;
	extern const ServSub servNegRespSubReqOutOfRange;
	extern const ServSub servNegRespSubReserved5;
	extern const ServSub servNegRespSubSecurityAccessDenied;
	extern const ServSub servNegRespSubAuthRequried;
	extern const ServSub servNegRespSubInvavaliKKey;
	extern const ServSub servNegRespSubExceedNumOfAttemps;
	extern const ServSub servNegRespSubRequiredTimeDelayNotExpired;
	extern const ServSub servNegRespSubSecureDataTxRequired;
	extern const ServSub servNegRespSubSecureDataTxNotAllowed;
	extern const ServSub servNegRespSubSecureDataVerifFail;
	extern const ServSub servNegRespSubReserved6;
	extern const ServSub servNegRespSubCertifInvalidPeriod;
	extern const ServSub servNegRespSubCertifInvalidSig;
	extern const ServSub servNegRespSubCertifInvalidChainOfTrust;
	extern const ServSub servNegRespSubCertifInvalidType;
	extern const ServSub servNegRespSubCertifInvalidFormat;
	extern const ServSub servNegRespSubCertifInvalidContent;
	extern const ServSub servNegRespSubCertifInvalidScope;
	extern const ServSub servNegRespSubCertifInvalidCertif;
	extern const ServSub servNegRespSubOwnerVerifFail;
	extern const ServSub servNegRespSubChallengeCalcFail;
	extern const ServSub servNegRespSubSetAccessRightFail;
	extern const ServSub servNegRespSubSessKeyCreateFail;
	extern const ServSub servNegRespSubConfDataUsageFail;
	extern const ServSub servNegRespSubDeAuthFail;
	extern const ServSub servNegRespSubReserved7;
	extern const ServSub servNegRespSubUpDownNotAccept;
	extern const ServSub servNegRespSubTxDataSuspend;
	extern const ServSub servNegRespSubGenProgFail;
	extern const ServSub servNegRespSubWrongBlockSeqCounter;
	extern const ServSub servNegRespSubReserved8;
	extern const ServSub servNegRespSubReqCorrectRxRespPending;
	extern const ServSub servNegRespSubReserved9;
	extern const ServSub servNegRespSubSubFuncNotActInCurrSess;
	extern const ServSub servNegRespSubServNotActInCurrSess;
	extern const ServSub servNegRespSubReserved10;
	extern const ServSub servNegRespSubRpmTooHigh;
	extern const ServSub servNegRespSubRpmTooLow;
	extern const ServSub servNegRespSubEngineRunning;
	extern const ServSub servNegRespSubEngineNotRunning;
	extern const ServSub servNegRespSubEngineRunTimeLow;
	extern const ServSub servNegRespSubTempTooHigh;
	extern const ServSub servNegRespSubTempTooLow;
	extern const ServSub servNegRespSubVehicleSpeedTooHigh;
	extern const ServSub servNegRespSubVehicleSpeedTooLow;
	extern const ServSub servNegRespSubThrottleTooHigh;
	extern const ServSub servNegRespSubThrottleTooLow;
	extern const ServSub servNegRespSubTransmissionNotInNeutral;
	extern const ServSub servNegRespSubTransmissionNotInGear;
	extern const ServSub servNegRespSubReserved11;
	extern const ServSub servNegRespSubBrakeNotApplied;
	extern const ServSub servNegRespSubShifterLevelNotInPark;
	extern const ServSub servNegRespSubTorqConvClutchLocked;
	extern const ServSub servNegRespSubVoltageTooHigh;
	extern const ServSub servNegRespSubVoltageTooLow;
	extern const ServSub servNegRespSubResourceTempNotAvail;
	extern const ServSub servNegRespSubReservedForSpecificCondsNotCorrect;
	extern const ServSub servNegRespSubVehicleMfrSpecificCondsNotCorrect;
	extern const ServSub servNegRespSubReserved12;



	extern const QVector<const ServSub *> servDiagSessCtrlSub;
	extern const QVector<const ServSub *> servEcuResetSub;
	extern const QVector<const ServSub *> servSecurityAccessSub;
	extern const QVector<const ServSub *> servCommCtrlSub;
	extern const QVector<const ServSub *> servTesterPresentSub;
	extern const QVector<const ServSub *> servAuthSub;
	extern const QVector<const ServSub *> servSecuredDataTxSub;
	extern const QVector<const ServSub *> servCtrlDtcSettingsSub;
	extern const QVector<const ServSub *> servRespOnEventSub;
	extern const QVector<const ServSub *> servLinkCtrlSub;
	extern const QVector<const ServSub *> servReadDataByIdSub;
	extern const QVector<const ServSub *> servReadMemByAddrSub;
	extern const QVector<const ServSub *> servReadScalingDataByIdSub;
	extern const QVector<const ServSub *> servReadDataByPeriodicIdSub;
	extern const QVector<const ServSub *> servDynDefDataIdSub;
	extern const QVector<const ServSub *> servWriteDataByIdSub;
	extern const QVector<const ServSub *> servWriteMemByAddrSub;
	extern const QVector<const ServSub *> servClearDiagInfoSub;
	extern const QVector<const ServSub *> servReadDtcInfoSub;
	extern const QVector<const ServSub *> servIoCtrlByIdSub;
	extern const QVector<const ServSub *> servRoutineCtrlSub;
	extern const QVector<const ServSub *> servRoutineDownloadSub;
	extern const QVector<const ServSub *> servReqUploadSub;
	extern const QVector<const ServSub *> servTransferDataSub;
	extern const QVector<const ServSub *> servReqTransferExitSub;
	extern const QVector<const ServSub *> servReqFileTransferSub;
	extern const QVector<const ServSub *> servNegRespSub;


	extern const Serv servDiagSessCtrl;
	extern const Serv servEcuReset;
	extern const Serv servSecurityAccess;
	extern const Serv servCommCtrl;
	extern const Serv servTesterPresent;
	extern const Serv servAuth;
	extern const Serv servSecuredDataTx;
	extern const Serv servCtrlDtcSettings;
	extern const Serv servRespOnEvent;
	extern const Serv servLinkCtrl;
	extern const Serv servReadDataById;
	extern const Serv servReadMemByAddr;
	extern const Serv servReadScalingDataById;
	extern const Serv servReadDataByPeriodicId;
	extern const Serv servDynDefDataId;
	extern const Serv servWriteDataById;
	extern const Serv servWriteMemByAddr;
	extern const Serv servClearDiagInfo;
	extern const Serv servReadDtcInfo;
	extern const Serv servIoCtrlById;
	extern const Serv servRoutineCtrl;
	extern const Serv servRoutineDownload;
	extern const Serv servReqUpload;
	extern const Serv servTransferData;
	extern const Serv servReqTransferExit;
	extern const Serv servReqFileTransfer;
	extern const Serv servNegResp;

	extern const QMap<ServEnum, const Serv *> serv;
}

#endif // UDS_DEF_H