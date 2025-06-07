#include "uds.h"
#include "uds_def.h"

#define U8CAST(x) static_cast<uint8_t>((x))

UdsInfo::UdsInfo() :
	name(""),
	detail(""),
	hex({}),
	hexIdx(0)
{
}

void UdsInfo::clear(void)
{
	this->name = "";
	this->detail = "";
	this->hex.clear();
}

QString UdsInfo::getHexStr(void) const
{
	QString s = "";
	for (int i = 0; i < this->hex.size(); ++i) {
		s += QString("%1 ").arg(this->hex[i], 2, 16, QChar('0'));
	}

	return s;
}

QString UdsInfo::getHexStr(int numOfBytes) const
{
	QString s = "";
	for (int i = 0; i < numOfBytes && i < this->hex.size(); ++i) {
		s += QString("%1 ").arg(this->hex[i], 2, 16, QChar('0'));
	}

	if (this->hex.size() > numOfBytes) {
		s += "...";
	}
	
	return s;
}


Uds::Uds(QObject *parent)
	: QObject{parent}
{

}

void Uds::getDiagSessCtrl()
{
	// only containes sub function, no need
}

void Uds::getEcuReset()
{
	// only containes sub function, no need
}

void Uds::getSecAcc()
{
	UdsDef::SecAccGenericReq secAccGenericReq(this->reqPacket);
	UdsInfo info;
	uint8_t subFunc;
	bool isSeedReq = false;
	bool isSendKeyReq = false;

	if(!secAccGenericReq.existSubFunc()) {
		return;
	}

	subFunc = secAccGenericReq.getSubFunc();

	isSeedReq =
		UdsDef::servSecurityAccessSubReqSeedDef.isThis(subFunc) ||
		UdsDef::servSecurityAccessSubReqSeedLevel.isThis(subFunc) ||
		UdsDef::servSecurityAccessSubReqSeedLevel1.isThis(subFunc) ||
		UdsDef::servSecurityAccessSubReqSeedLevel2.isThis(subFunc) ||
		UdsDef::servSecurityAccessSubIso26021ReqSeed.isThis(subFunc);

	if(isSeedReq) {
		UdsDef::SecAccReqSeedReq secAccReqSeedReq(this->reqPacket);
		if(!secAccReqSeedReq.existSeed()) {
			return;
		}
		QVector<uint8_t> seed;
		secAccReqSeedReq.getSeed(seed);
		info.clear();
		info.name = "Security Seed";
		info.detail = "Security Seed";
		for(int i = 0; i < seed.size(); ++i) {
			info.hex.append(seed[i]);
		}
		info.hexIdx = secAccReqSeedReq.seedPos;
		this->reqPacketInfo.append(info);
		return;
	}

	isSendKeyReq =
		UdsDef::servSecurityAccessSubSendKeyDef.isThis(subFunc) ||
		UdsDef::servSecurityAccessSubSendKeyLevel.isThis(subFunc) ||
		UdsDef::servSecurityAccessSubSendKeyLevel1.isThis(subFunc) ||
		UdsDef::servSecurityAccessSubSendKeyLevel2.isThis(subFunc) ||
		UdsDef::servSecurityAccessSubIso26021SendKey.isThis(subFunc);
	
	if(isSendKeyReq) {
		UdsDef::SecAccSendKeyReq secAccSendKeyReq(this->reqPacket);
		if(!secAccSendKeyReq.existKey()) {
			return;
		}
		QVector<uint8_t> key;
		secAccSendKeyReq.getKey(key);
		info.clear();
		info.name = "Security Key";
		info.detail = "Security Key";
		for(int i = 0; i < key.size(); ++i) {
			info.hex.append(key[i]);
		}
		info.hexIdx = secAccSendKeyReq.keyPos;
		this->reqPacketInfo.append(info);
		return;
	}
}

void Uds::getCommCtrl()
{
	UdsDef::CommCtrlReq commCtrlReq(this->reqPacket);

	if(!commCtrlReq.existCommType()) {
		return;
	}

	uint8_t commType = commCtrlReq.getCommType();
	UdsInfo info;

	info.name = "Communication Type";
	info.detail = "Communication Type";
	info.hex.append(commType);
	info.hexIdx = commCtrlReq.commTypePos;
	this->reqPacketInfo.append(info);

	if(!commCtrlReq.existNodeIdHigh()) {
		return;
	}
	uint8_t nodeIdHigh = commCtrlReq.getNodeIdHigh();
	info.clear();
	info.name = "Node ID High";
	info.detail = "Node ID High";
	info.hex.append(nodeIdHigh);
	info.hexIdx = commCtrlReq.nodeIdHighPos;
	this->reqPacketInfo.append(info);

	if(!commCtrlReq.existNodeIdLow()) {
		return;
	}
	uint8_t nodeIdLow = commCtrlReq.getNodeIdLow();
	info.clear();
	info.name = "Node ID Low";
	info.detail = "Node ID Low";
	info.hex.append(nodeIdLow);
	info.hexIdx = commCtrlReq.nodeIdLowPos;
	this->reqPacketInfo.append(info);
}

void Uds::getTesterPresent()
{
	// only containes sub function, no need
}

void Uds::getAuth()
{
	// not care as of now
}

void Uds::getSecDataTx()
{
	// not care as of now
}

void Uds::getCtrlDtcSettings()
{
	// not care as of now
}

void Uds::getRespOnEvent()
{
	// not care as of now
}

void Uds::getLinkCtrl()
{
	// not care as of now
}

void Uds::getReadDataById()
{
	UdsDef::ReadDataByIdReq readDataByIdReq(this->reqPacket);
	UdsInfo info;

	if(!readDataByIdReq.existDataId()) {
		return;
	}

	info.clear();
	info.name = "Data Identifier";
	info.detail = "Data Identifier";
	readDataByIdReq.getDataId(info.hex);
	info.hexIdx = readDataByIdReq.dataIdPos;
	this->reqPacketInfo.append(info);
}

void Uds::getReadMemByAddr()
{
	// not care as of now
}

void Uds::getReadScalingDataById()
{
	// not care as of now
}

void Uds::getReadDataByPeriodicId()
{
	// not care as of now
}

void Uds::getDynDefDataId()
{
	// not care as of now
}

void Uds::getWriteDataById()
{
	UdsDef::WriteDataByIdReq writeDataByIdReq(this->reqPacket);
	UdsInfo info;
	uint16_t id;

	if(!writeDataByIdReq.existDataId()) {
		return;
	}

	info.clear();
	info.name = "Data Identifier";
	info.detail = "Data Identifier";
	id = writeDataByIdReq.getDataId();
	for(int i = 0; i < writeDataByIdReq.dataIdSize; ++i) {
		info.hex.append(id >> (8 * i));
	}
	info.hexIdx = writeDataByIdReq.dataIdPos;
	this->reqPacketInfo.append(info);

	if(!writeDataByIdReq.existDataRecord()) {
		return;
	}
	info.clear();
	writeDataByIdReq.getDataRecord(info.hex);
	info.name = "Data Record";
	info.detail = "Data Record";
	info.hexIdx = writeDataByIdReq.dataRecordPos;
	this->reqPacketInfo.append(info);
}

void Uds::getWriteMemByAddr()
{
	// not care as of now
}

void Uds::getClearDiagInfo()
{
	// not care as of now
}

void Uds::getReadDtcInfoByStatusMask()
{
	UdsInfo info;
	const UdsDef::ReadDTCInfoByStReq readDtcInfoByStReq(this->reqPacket);
	uint8_t mask;

	if(!readDtcInfoByStReq.existMask()) {
		return;
	}
	info.clear();
	mask = readDtcInfoByStReq.getMask();
	info.name = "DTC Status Mask";
	info.detail = "DTC Status Mask";
	info.hex.append(mask);
	info.hexIdx = readDtcInfoByStReq.maskPos;
	this->reqPacketInfo.append(info);
}

void Uds::getReadDtcInfo()
{
	const UdsDef::ReadDtcInfoGenericReq readDtcInfoReqGeneric(this->reqPacket);
	uint8_t subFunc;

	if(!readDtcInfoReqGeneric.existSubFunc()) {
		return;
	}
	subFunc = readDtcInfoReqGeneric.getSubFunc();
	switch(subFunc) {
	case U8CAST(UdsDef::ServSubReadDtcInfo::reportDtcByStatusMask):
		getReadDtcInfoByStatusMask();
		break;
	}
}

void Uds::getIoCtrlById()
{
	// not care as of now
}

void Uds::getRoutineCtrl()
{
	UdsInfo info;
	const UdsDef::RoutineCtrlReq routineCtrlReq(this->reqPacket);
	uint16_t rid;
	QVector<uint8_t> optRecord;
	
	if(!routineCtrlReq.existRid()) {
		return;
	}
	info.clear();
	rid = routineCtrlReq.getRid();
	info.name = "Routine Identifier";
	info.detail = "Routine Identifier";
	for(int i = 0; i < routineCtrlReq.ridSize; ++i) {
		info.hex.append(rid >> (8 * i));
	}
	info.hexIdx = routineCtrlReq.ridPos;
	this->reqPacketInfo.append(info);

	if(!routineCtrlReq.existOptRecord()) {
		return;
	}
	info.clear();
	routineCtrlReq.getOptRecord(optRecord);
	info.name = "Optional Record";
	info.detail = "Optional Record";
	for(int i = 0; i < optRecord.size(); ++i) {
		info.hex.append(optRecord[i]);
	}
	info.hexIdx = routineCtrlReq.optRecordPos;
	this->reqPacketInfo.append(info);
}

void Uds::getRoutineDownload()
{
	// not care as of now
}

void Uds::getReqUpload()
{
	// not care as of now
}

void Uds::getTransferData()
{
	UdsDef::TransferDataReq transferDataReq(this->reqPacket);

	if(!transferDataReq.existBsc()) {
		return;
	}
	uint8_t bsc = transferDataReq.getBsc();
	UdsInfo info;

	info.name = "Block Sequence Counter";
	info.detail = "Block Sequence Counter";
	info.hex.append(bsc);
	info.hexIdx = transferDataReq.bscPos;
	this->reqPacketInfo.append(info);

	if(!transferDataReq.existData()) {
		return;
	}
	info.clear();
	info.name = "Data";
	info.detail = "Data";
	transferDataReq.getData(info.hex);
	info.hexIdx = transferDataReq.dataPos;
	this->reqPacketInfo.append(info);
}

void Uds::getReqTransferExit()
{
	// not care as of now
}

void Uds::getReqFileTransfer()
{
	// not care as of now
}

void Uds::getServSubInfo()
{
	UdsInfo info;
	info.name = "";
	info.detail = "";
	for (int i = 0; i < this->reqPacket.size(); ++i) {
		info.hex.append(this->reqPacket[i]);
	}
	info.hexIdx = 0;
	this->reqPacketInfo.append(info);

	for(const UdsDef::ServEnum &servEnum : UdsDef::serv.keys()) {
		const UdsDef::Serv *servPtr = UdsDef::serv[servEnum];
		if(servPtr == nullptr) {
			continue;
		}

		uint8_t sid = U8CAST(servEnum);
		if(this->reqPacket[0] != sid) {
			continue;
		}

		info.clear();
		info.name = servPtr->shortName;
		info.detail = servPtr->shortName;
		info.hex.append(this->reqPacket[0]);
		info.hexIdx = 0;
		this->reqPacketInfo.append(info);

		for(const UdsDef::ServSub *subPtr : *servPtr->sub) {
			if(subPtr->isThis(this->reqPacket[1])) {
				info.clear();
				info.name = subPtr->shortName;
				info.detail = subPtr->shortName;
				info.hex.append(this->reqPacket[1]);
				info.hexIdx = 1;
				this->reqPacketInfo.append(info);
				break;
			}
		}

		break;
	}
}

void Uds::getReqInfo(const QVector<uint8_t> &packetRef, QVector<UdsInfo> &packetInfoRef)
{
	this->reqPacket.clear();
	this->reqPacketInfo.clear();
	packetInfoRef.clear();

	this->reqPacket = packetRef;

	uint8_t sid;
	if(this->reqPacket.length() == 0) {
		return;
	}
	sid = U8CAST(this->reqPacket[0]);
	getServSubInfo();

	switch(sid) {
	case U8CAST(UdsDef::ServEnum::diagSessCtrl):
		getDiagSessCtrl();
		break;
	case U8CAST(UdsDef::ServEnum::ecuReset):
		getEcuReset();
		break;
	case U8CAST(UdsDef::ServEnum::testerPresent):
		getTesterPresent();
		break;
	case U8CAST(UdsDef::ServEnum::readDataById):
		getReadDataById();
		break;
	case U8CAST(UdsDef::ServEnum::writeDataById):
		getWriteDataById();
		break;
	case U8CAST(UdsDef::ServEnum::routineCtrl):
		getRoutineCtrl();
		break;
	case U8CAST(UdsDef::ServEnum::securityAccess):
		getSecAcc();
		break;
	case U8CAST(UdsDef::ServEnum::commCtrl):
		getCommCtrl();
		break;
	case U8CAST(UdsDef::ServEnum::transferData):
		getTransferData();
		break;
	case U8CAST(UdsDef::ServEnum::readDtcInfo):
		getReadDtcInfo();
		break;
	}

	packetInfoRef = this->reqPacketInfo;
}

void Uds::getNegResp()
{
	UdsInfo info;
	const UdsDef::Serv &servNegResp = UdsDef::servNegResp;
	info.clear();
	info.name = servNegResp.shortName;
	info.detail = servNegResp.shortName;
	info.hex.append(this->respPacket[0]);
	info.hexIdx = 0;
	this->respPacketInfo.append(info);

	info.clear();
	const UdsDef::NegResp negResp(this->respPacket);
	// trying to find service

	if(!negResp.existSid()) {
		return;
	}

	for(const UdsDef::ServEnum &servEnum : UdsDef::serv.keys()) {
		const UdsDef::Serv *servPtr = UdsDef::serv[servEnum];
		if(servPtr == nullptr) {
			continue;
		}

		uint8_t sid = U8CAST(servEnum);
		if(negResp.getReqSid() != sid) {
			continue;
		}

		info.name = servPtr->shortName;
		info.detail = servPtr->shortName;
		info.hex.append(negResp.getReqSid());
		info.hexIdx = negResp.reqSidPos;
		this->respPacketInfo.append(info);
	}

	// trying to find negative response code
	if(!negResp.existNrc()) {
		return;
	}

	info.clear();

	for(const UdsDef::ServSub *subPtr : *UdsDef::servNegResp.sub) {
		if(subPtr->isThis(negResp.getNrc())) {
			info.name = subPtr->shortName;
			info.detail = subPtr->shortName;
			info.hex.append(negResp.getNrc());
			info.hexIdx = negResp.nrcPos;
			this->respPacketInfo.append(info);
			break;
		}
	}
}


void Uds::getDiagSessCtrlResp()
{
	UdsInfo info;
	info.name = UdsDef::servDiagSessCtrl.shortName;
	info.detail = UdsDef::servDiagSessCtrl.shortName;
	info.hex.append(this->respPacket[0]);
	info.hexIdx = 0;
	this->respPacketInfo.append(info);
	uint8_t subFunc;
	uint32_t sessParam;
	const UdsDef::DiagSessCtrlPosResp diagSessCtrlPosResp(this->respPacket);

	if(!diagSessCtrlPosResp.existSubFunc()) {
		return;
	}

	info.clear();

	subFunc = diagSessCtrlPosResp.getSubFunc();
	for(const UdsDef::ServSub *servSubPtr : *UdsDef::servDiagSessCtrl.sub) {
		if(servSubPtr->isThis(subFunc)) {
			info.name = servSubPtr->shortName;
			info.detail = servSubPtr->detail;
			info.hex.append(subFunc);
			info.hexIdx = diagSessCtrlPosResp.subFuncPos;
			this->respPacketInfo.append(info);
			break;
		}
	}

	if(!diagSessCtrlPosResp.existSessParam()) {
		return;
	}
	info.clear();
	sessParam = diagSessCtrlPosResp.getSessParam();
	info.name = "Session Parameter";
	info.detail = "Session Parameter";
	for(int i = 0; i < diagSessCtrlPosResp.sessParamSize; ++i) {
		info.hex.append(sessParam >> (8 * i));
	}
	info.hexIdx = diagSessCtrlPosResp.sessParamPos;
	this->respPacketInfo.append(info);
}

void Uds::getEcuResetResp()
{
	UdsInfo info;
	info.name = UdsDef::servEcuReset.shortName;
	info.detail = UdsDef::servEcuReset.shortName;
	info.hex.append(this->respPacket[0]);
	info.hexIdx = 0;
	this->respPacketInfo.append(info);
	const UdsDef::EcuResetPosResp ecuResetPosResp(this->respPacket);

	if(!ecuResetPosResp.existSubFunc()) {
		return;
	}
	info.clear();
	uint8_t subFunc = ecuResetPosResp.getSubFunc();
	for(const UdsDef::ServSub *servSubPtr : *UdsDef::servEcuReset.sub) {
		if(servSubPtr->isThis(subFunc)) {
			info.name = servSubPtr->shortName;
			info.detail = servSubPtr->detail;
			info.hex.append(subFunc);
			info.hexIdx = ecuResetPosResp.subFuncPos;
			this->respPacketInfo.append(info);
			break;
		}
	}

	if(!ecuResetPosResp.existPowerDownTime()) {
		return;
	}
	info.clear();
	info.name = "Power Down Time";
	info.detail = "Power Down Time";
	uint8_t powerDownTime = ecuResetPosResp.getPowerDownTime();
	info.hex.append(powerDownTime);
	info.hexIdx = ecuResetPosResp.powerDownTimePos;
	this->respPacketInfo.append(info);
}


void Uds::getSecAccResp()
{
	QVector<uint8_t> seed;
	UdsInfo info;
	info.name = UdsDef::servSecurityAccess.shortName;
	info.detail = UdsDef::servSecurityAccess.shortName;
	info.hex.append(this->respPacket[0]);
	info.hexIdx = 0;
	this->respPacketInfo.append(info);

	const UdsDef::SecAccPosResp secAccPosResp(this->respPacket);

	if(!secAccPosResp.existSubFunc()) {
		return;
	}
	info.clear();
	uint8_t subFunc = secAccPosResp.getSubFunc();
	for(const UdsDef::ServSub *servSubPtr : *UdsDef::servSecurityAccess.sub) {
		if(servSubPtr->isThis(subFunc)) {
			info.name = servSubPtr->shortName;
			info.detail = servSubPtr->detail;
			info.hex.append(subFunc);
			info.hexIdx = secAccPosResp.subFuncPos;
			this->respPacketInfo.append(info);
			break;
		}
	}

	if(!secAccPosResp.existSecSeed()) {
		return;
	}
	info.clear();
	info.name = "Security Seed";
	info.detail = "Security Seed";
	seed.clear();
	secAccPosResp.getSecSeed(seed);
	info.hex = seed;
	info.hexIdx = secAccPosResp.secSeedPos;
	this->respPacketInfo.append(info);
}

void Uds::getRoutineCtrlResp()
{
	UdsInfo info;
	info.name = UdsDef::servRoutineCtrl.shortName;
	info.detail = UdsDef::servRoutineCtrl.shortName;
	info.hex.append(this->respPacket[0]);
	info.hexIdx = 0;
	this->respPacketInfo.append(info);

	const UdsDef::RoutineCtrlPosResp routineCtrlPosResp(this->respPacket);

	if(!routineCtrlPosResp.existType()) {
		return;
	}
	info.clear();
	uint8_t subFunc = routineCtrlPosResp.getType();
	for(const UdsDef::ServSub *servSubPtr : *UdsDef::servRoutineCtrl.sub) {
		if(servSubPtr->isThis(subFunc)) {
			info.name = servSubPtr->shortName;
			info.detail = servSubPtr->detail;
			info.hex.append(subFunc);
			info.hexIdx = routineCtrlPosResp.typePos;
			this->respPacketInfo.append(info);
			break;
		}
	}

	if(!routineCtrlPosResp.existRid()) {
		return;
	}
	info.clear();
	info.name = "RID";
	info.detail = "RID";
	uint16_t rid = routineCtrlPosResp.getRid();
	for(int i = 0; i < routineCtrlPosResp.ridSize; ++i) {
		info.hex.append((uint8_t)(rid >> (8 * i)));
	}
	info.hexIdx = routineCtrlPosResp.ridPos;
	this->respPacketInfo.append(info);

	if(!routineCtrlPosResp.existInfo()) {
		return;
	}
	info.clear();
	info.name = "Info";
	info.detail = "Info";
	uint8_t ridInfo = routineCtrlPosResp.getInfo();
	info.hex.append(ridInfo);
	info.hexIdx = routineCtrlPosResp.infoPos;
	this->respPacketInfo.append(info);

	if(!routineCtrlPosResp.existStatusRecord()) {
		return;
	}
	info.clear();
	info.name = "Status Record";
	info.detail = "Status Record";
	routineCtrlPosResp.getStatusRecord(info.hex);
	info.hexIdx = routineCtrlPosResp.statusRecordPos;
	this->respPacketInfo.append(info);
}

void Uds::getWriteDataByIdResp()
{
	UdsInfo info;
	info.name = UdsDef::servWriteDataById.shortName;
	info.detail = UdsDef::servWriteDataById.shortName;
	info.hex.append(this->respPacket[0]);
	info.hexIdx = 0;
	this->respPacketInfo.append(info);

	const UdsDef::WriteDataByIdPosResp writeDataByIdPosResp(this->respPacket);

	if(!writeDataByIdPosResp.existDataId()) {
		return;
	}
	info.clear();
	uint16_t dataId = writeDataByIdPosResp.getDataId();
	info.name = "Data Identifier";
	info.detail = "Data Identifier";
	for(int i = 0; i < writeDataByIdPosResp.dataIdSize; ++i) {
		info.hex.append((uint8_t)(dataId >> (8 * i)));
	}
	info.hexIdx = writeDataByIdPosResp.dataIdPos;
	this->respPacketInfo.append(info);
}

void Uds::getTransferDataResp()
{
	UdsInfo info;
	info.name = UdsDef::servTransferData.shortName;
	info.detail = UdsDef::servTransferData.shortName;
	info.hex.append(this->respPacket[0]);
	info.hexIdx = 0;
	this->respPacketInfo.append(info);

	const UdsDef::TransferDataPosResp transferDataPosResp(this->respPacket);

	if(!transferDataPosResp.existBsc()) {
		return;
	}
	info.clear();
	uint8_t bsc = transferDataPosResp.getBsc();
	info.name = "Block Sequence Counter";
	info.detail = "Block Sequence Counter";
	info.hex.append(bsc);
	info.hexIdx = transferDataPosResp.bscPos;
	this->respPacketInfo.append(info);

	if(!transferDataPosResp.existParamRecord()) {
		return;
	}
	info.clear();
	info.name = "Parameter Record";
	info.detail = "Parameter Record";
	transferDataPosResp.getParamRecord(info.hex);
	info.hexIdx = transferDataPosResp.paramRecordPos;
	this->respPacketInfo.append(info);
}

void Uds::getReadDtcInfoByStatusMaskResp()
{
	UdsInfo info;
	uint8_t availStMask;
	QVector<uint8_t> dtcNSt;
	const uint8_t dtcNStSize = 4; // dtc(high,middle,low) + status mask
	QVector<uint8_t> dtcNStRecord;


	const UdsDef::ReadDTCInfoByStPosResp posResp(this->respPacket);

	if(!posResp.existAvailStMask()) {
		return;
	}
	info.clear();
	info.name = "Available DTC Status Mask";
	info.detail = info.name;
	availStMask = posResp.getAvailStMask();
	info.hex.append(availStMask);
	info.hexIdx = posResp.availStMaskPos;
	this->respPacketInfo.append(info);

	if(!posResp.existDtcNStRecord()) {
		return;
	}

	dtcNStRecord.clear();

	posResp.getDtcNStRecord(dtcNStRecord);

	for(int i = 0; i < dtcNStRecord.length();) {
		dtcNSt.clear();
		for(; i < dtcNStSize && i < dtcNStRecord.length(); ++i) {
			dtcNSt.append(dtcNStRecord[i]);
		}

		if(dtcNSt.length() != dtcNStSize) {
			continue;
		}

		info.clear();
		info.name = "DTC and Status Mask";
		info.detail = info.name;
		info.hex = dtcNSt;
		info.hexIdx = i - dtcNStSize;
		this->respPacketInfo.append(info);
	}
}

void Uds::getReadDtcInfoResp()
{
	UdsInfo info;
	uint8_t subFunc;

	info.name = UdsDef::servReadDtcInfo.shortName;
	info.detail = UdsDef::servReadDtcInfo.shortName;
	info.hex.append(this->respPacket[0]);
	info.hexIdx = 0;
	this->respPacketInfo.append(info);

	const UdsDef::ReadDtcInfoGenericPosResp genericPosResp(this->respPacket);
	if(!genericPosResp.existSubFunc()) {
		return;
	}

	subFunc = genericPosResp.getSubFunc();

	for(const UdsDef::ServSub *servSubPtr : UdsDef::servReadDtcInfoSub) {
		if(servSubPtr->isThis(subFunc)) {
			info.clear();
			info.name = servSubPtr->shortName;
			info.detail = servSubPtr->shortName;
			info.hex.append(subFunc);
			info.hexIdx = genericPosResp.subFuncPos;
			this->respPacketInfo.append(info);
			break;
		}
	}

	if(!genericPosResp.existBuf()) {
		return;
	}

	if(subFunc == U8CAST(UdsDef::ServSubReadDtcInfo::reportDtcByStatusMask)) {
		getReadDtcInfoByStatusMaskResp();
	}
}

void Uds::getRespInfo(const QVector<uint8_t> &packetRef, QVector<UdsInfo> &packetInfoRef)
{
	packetInfoRef.clear();
	this->respPacket.clear();
	this->respPacketInfo.clear();
	this->respPacket = packetRef;
	UdsInfo info;
	uint8_t sid;
	info.name = "";
	info.detail = "";
	for (int i = 0; i < this->respPacket.size(); ++i) {
		info.hex.append(this->respPacket[i]);
	}
	info.hexIdx = 0;
	this->respPacketInfo.append(info);

	if(this->respPacket.size() == 0) {
		return;
	}

	sid = U8CAST(this->respPacket[0]) - 0x40;

	if(U8CAST(this->respPacket[0]) == U8CAST(UdsDef::ServEnum::negResp)) {
		getNegResp();
	} else if(sid == U8CAST(UdsDef::ServEnum::diagSessCtrl)) {
		getDiagSessCtrlResp();
	} else if(sid == U8CAST(UdsDef::ServEnum::ecuReset)) {
		getEcuResetResp();
	} else if(sid == U8CAST(UdsDef::ServEnum::securityAccess)) {
		getSecAccResp();
	} else if(sid == U8CAST(UdsDef::ServEnum::routineCtrl)) {
		getRoutineCtrlResp();
	} else if(sid == U8CAST(UdsDef::ServEnum::writeDataById)) {
		getWriteDataByIdResp();
	} else if(sid == U8CAST(UdsDef::ServEnum::transferData)) {
		getTransferDataResp();
	} else if(sid == U8CAST(UdsDef::ServEnum::readDtcInfo)) {
		getReadDtcInfoResp();
	} else {
		for(const UdsDef::ServEnum &servEnum : UdsDef::serv.keys()) {
			const UdsDef::Serv *servPtr = UdsDef::serv[servEnum];
			if(servPtr == nullptr) {
				continue;
			}

			uint8_t sid = U8CAST(servEnum);
			if(this->respPacket[0] != sid + 0x40) {
				continue;
			}

			info.clear();
			info.name = servPtr->shortName;
			info.detail = servPtr->shortName;
			info.hex.append(this->respPacket[0]);
			info.hexIdx = 0;
			this->respPacketInfo.append(info);
		}
	}

	packetInfoRef = this->respPacketInfo;
}
