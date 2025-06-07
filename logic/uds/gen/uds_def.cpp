#include "uds_def.h"

namespace UdsDef {
	ServSub::ServSub(
		uint8_t id,
		const QString &shortName,
		const QString &detail
	) :
		idStart(id),
		idEnd(id),
		shortName(shortName),
		detail(detail),
		isRange(false)
	{
	}

	ServSub::ServSub(
		uint8_t idStart,
		uint8_t idEnd,
		const QString &shortName,
		const QString &detail
	) :
		idStart(idStart),
		idEnd(idEnd),
		shortName(shortName),
		detail(detail),
		isRange(true)
	{
	}

	bool ServSub::isThis(uint8_t id) const
	{
		if(id >= this->idStart && id <= this->idEnd) {
			return true;
		}
		return false;
	}


	Serv::Serv(
		ServEnum id,
		const QString &shortName,
		const QVector<const ServSub *> *sub
	) :
		id(id),
		shortName(shortName),
		sub(sub)
	{
	}
	SecAccGenericReq::SecAccGenericReq (const QVector<uint8_t> &packetRef) :
		packet(packetRef)
	{
	}

	bool SecAccGenericReq::existSid(void) const
	{
		return packet.length() >= (sidPos + sidSize);
	}

	uint8_t SecAccGenericReq::getSid(void) const
	{
		return packet[sidPos];
	}

	bool SecAccGenericReq::existSubFunc(void) const
	{
		return packet.length() >= (subFuncPos + subFuncSize);
	}

	uint8_t SecAccGenericReq::getSubFunc(void) const
	{
		return packet[subFuncPos];
	}

	bool SecAccGenericReq::existData(void) const
	{
		return packet.length() > dataPos;
	}

	void SecAccGenericReq::getData(QVector<uint8_t> &outRef) const
	{
		outRef.clear();
		int outSize = packet.length() - dataPos;
		for(int i = 0; i < outSize; ++i) {
			outRef.append(packet[dataPos + i]);
		}
	}

	SecAccReqSeedReq::SecAccReqSeedReq (const QVector<uint8_t> &packetRef) :
		packet(packetRef)
	{
	}

	bool SecAccReqSeedReq::existSid(void) const
	{
		return packet.length() >= (sidPos + sidSize);
	}

	uint8_t SecAccReqSeedReq::getSid(void) const
	{
		return packet[sidPos];
	}

	bool SecAccReqSeedReq::existReqSeed(void) const
	{
		return packet.length() >= (reqSeedPos + reqSeedSize);
	}

	uint8_t SecAccReqSeedReq::getReqSeed(void) const
	{
		return packet[reqSeedPos];
	}

	bool SecAccReqSeedReq::existSeed(void) const
	{
		return packet.length() > seedPos;
	}

	void SecAccReqSeedReq::getSeed(QVector<uint8_t> &outRef) const
	{
		outRef.clear();
		int outSize = packet.length() - seedPos;
		for(int i = 0; i < outSize; ++i) {
			outRef.append(packet[seedPos + i]);
		}
	}

	SecAccSendKeyReq::SecAccSendKeyReq (const QVector<uint8_t> &packetRef) :
		packet(packetRef)
	{
	}

	bool SecAccSendKeyReq::existSid(void) const
	{
		return packet.length() >= (sidPos + sidSize);
	}

	uint8_t SecAccSendKeyReq::getSid(void) const
	{
		return packet[sidPos];
	}

	bool SecAccSendKeyReq::existSendKey(void) const
	{
		return packet.length() >= (sendKeyPos + sendKeySize);
	}

	uint8_t SecAccSendKeyReq::getSendKey(void) const
	{
		return packet[sendKeyPos];
	}

	bool SecAccSendKeyReq::existKey(void) const
	{
		return packet.length() > keyPos;
	}

	void SecAccSendKeyReq::getKey(QVector<uint8_t> &outRef) const
	{
		outRef.clear();
		int outSize = packet.length() - keyPos;
		for(int i = 0; i < outSize; ++i) {
			outRef.append(packet[keyPos + i]);
		}
	}

	SecAccPosResp::SecAccPosResp (const QVector<uint8_t> &packetRef) :
		packet(packetRef)
	{
	}

	bool SecAccPosResp::existSid(void) const
	{
		return packet.length() >= (sidPos + sidSize);
	}

	uint8_t SecAccPosResp::getSid(void) const
	{
		return packet[sidPos];
	}

	bool SecAccPosResp::existSubFunc(void) const
	{
		return packet.length() >= (subFuncPos + subFuncSize);
	}

	uint8_t SecAccPosResp::getSubFunc(void) const
	{
		return packet[subFuncPos];
	}

	bool SecAccPosResp::existSecSeed(void) const
	{
		return packet.length() > secSeedPos;
	}

	void SecAccPosResp::getSecSeed(QVector<uint8_t> &outRef) const
	{
		outRef.clear();
		int outSize = packet.length() - secSeedPos;
		for(int i = 0; i < outSize; ++i) {
			outRef.append(packet[secSeedPos + i]);
		}
	}

	DiagSessCtrlReq::DiagSessCtrlReq (const QVector<uint8_t> &packetRef) :
		packet(packetRef)
	{
	}

	bool DiagSessCtrlReq::existSid(void) const
	{
		return packet.length() >= (sidPos + sidSize);
	}

	uint8_t DiagSessCtrlReq::getSid(void) const
	{
		return packet[sidPos];
	}

	bool DiagSessCtrlReq::existSubFunc(void) const
	{
		return packet.length() >= (subFuncPos + subFuncSize);
	}

	uint8_t DiagSessCtrlReq::getSubFunc(void) const
	{
		return packet[subFuncPos];
	}


	DiagSessCtrlPosResp::DiagSessCtrlPosResp (const QVector<uint8_t> &packetRef) :
		packet(packetRef)
	{
	}

	bool DiagSessCtrlPosResp::existSid(void) const
	{
		return packet.length() >= (sidPos + sidSize);
	}

	uint8_t DiagSessCtrlPosResp::getSid(void) const
	{
		return packet[sidPos];
	}

	bool DiagSessCtrlPosResp::existSubFunc(void) const
	{
		return packet.length() >= (subFuncPos + subFuncSize);
	}

	uint8_t DiagSessCtrlPosResp::getSubFunc(void) const
	{
		return packet[subFuncPos];
	}

	bool DiagSessCtrlPosResp::existP2Server(void) const
	{
		return packet.length() >= (p2ServerPos + p2ServerSize);
	}

	uint16_t DiagSessCtrlPosResp::getP2Server(void) const
	{
		uint16_t ret = 0;
		for(int i = 0; i < p2ServerSize; ++i) {
			((uint8_t *)&ret)[i] = packet[p2ServerPos + i];
		}
		return ret;
	}

	bool DiagSessCtrlPosResp::existP2StarServer(void) const
	{
		return packet.length() >= (p2StarServerPos + p2StarServerSize);
	}

	uint16_t DiagSessCtrlPosResp::getP2StarServer(void) const
	{
		uint16_t ret = 0;
		for(int i = 0; i < p2StarServerSize; ++i) {
			((uint8_t *)&ret)[i] = packet[p2StarServerPos + i];
		}
		return ret;
	}


	EcuResetReq::EcuResetReq (const QVector<uint8_t> &packetRef) :
		packet(packetRef)
	{
	}

	bool EcuResetReq::existSid(void) const
	{
		return packet.length() >= (sidPos + sidSize);
	}

	uint8_t EcuResetReq::getSid(void) const
	{
		return packet[sidPos];
	}

	bool EcuResetReq::existSubFunc(void) const
	{
		return packet.length() >= (subFuncPos + subFuncSize);
	}

	uint8_t EcuResetReq::getSubFunc(void) const
	{
		return packet[subFuncPos];
	}


	EcuResetPosResp::EcuResetPosResp (const QVector<uint8_t> &packetRef) :
		packet(packetRef)
	{
	}

	bool EcuResetPosResp::existSid(void) const
	{
		return packet.length() >= (sidPos + sidSize);
	}

	uint8_t EcuResetPosResp::getSid(void) const
	{
		return packet[sidPos];
	}

	bool EcuResetPosResp::existSubFunc(void) const
	{
		return packet.length() >= (subFuncPos + subFuncSize);
	}

	uint8_t EcuResetPosResp::getSubFunc(void) const
	{
		return packet[subFuncPos];
	}

	bool EcuResetPosResp::existPowerDownTime(void) const
	{
		return packet.length() >= (powerDownTimePos + powerDownTimeSize);
	}

	uint8_t EcuResetPosResp::getPowerDownTime(void) const
	{
		return packet[powerDownTimePos];
	}


	TesterPresentReq::TesterPresentReq (const QVector<uint8_t> &packetRef) :
		packet(packetRef)
	{
	}

	bool TesterPresentReq::existSid(void) const
	{
		return packet.length() >= (sidPos + sidSize);
	}

	uint8_t TesterPresentReq::getSid(void) const
	{
		return packet[sidPos];
	}

	bool TesterPresentReq::existSubFunc(void) const
	{
		return packet.length() >= (subFuncPos + subFuncSize);
	}

	uint8_t TesterPresentReq::getSubFunc(void) const
	{
		return packet[subFuncPos];
	}


	TesterPresentPosResp::TesterPresentPosResp (const QVector<uint8_t> &packetRef) :
		packet(packetRef)
	{
	}

	bool TesterPresentPosResp::existSid(void) const
	{
		return packet.length() >= (sidPos + sidSize);
	}

	uint8_t TesterPresentPosResp::getSid(void) const
	{
		return packet[sidPos];
	}

	bool TesterPresentPosResp::existSubFunc(void) const
	{
		return packet.length() >= (subFuncPos + subFuncSize);
	}

	uint8_t TesterPresentPosResp::getSubFunc(void) const
	{
		return packet[subFuncPos];
	}


	ReadDataByIdReq::ReadDataByIdReq (const QVector<uint8_t> &packetRef) :
		packet(packetRef)
	{
	}

	bool ReadDataByIdReq::existSid(void) const
	{
		return packet.length() >= (sidPos + sidSize);
	}

	uint8_t ReadDataByIdReq::getSid(void) const
	{
		return packet[sidPos];
	}

	bool ReadDataByIdReq::existDataId(void) const
	{
		return packet.length() > dataIdPos;
	}

	void ReadDataByIdReq::getDataId(QVector<uint8_t> &outRef) const
	{
		outRef.clear();
		int outSize = packet.length() - dataIdPos;
		for(int i = 0; i < outSize; ++i) {
			outRef.append(packet[dataIdPos + i]);
		}
	}

	ReadDataByIdPosResp::ReadDataByIdPosResp (const QVector<uint8_t> &packetRef) :
		packet(packetRef)
	{
	}

	bool ReadDataByIdPosResp::existSid(void) const
	{
		return packet.length() >= (sidPos + sidSize);
	}

	uint8_t ReadDataByIdPosResp::getSid(void) const
	{
		return packet[sidPos];
	}

	bool ReadDataByIdPosResp::existData(void) const
	{
		return packet.length() > dataPos;
	}

	void ReadDataByIdPosResp::getData(QVector<uint8_t> &outRef) const
	{
		outRef.clear();
		int outSize = packet.length() - dataPos;
		for(int i = 0; i < outSize; ++i) {
			outRef.append(packet[dataPos + i]);
		}
	}

	WriteDataByIdReq::WriteDataByIdReq (const QVector<uint8_t> &packetRef) :
		packet(packetRef)
	{
	}

	bool WriteDataByIdReq::existSid(void) const
	{
		return packet.length() >= (sidPos + sidSize);
	}

	uint8_t WriteDataByIdReq::getSid(void) const
	{
		return packet[sidPos];
	}

	bool WriteDataByIdReq::existDataId(void) const
	{
		return packet.length() >= (dataIdPos + dataIdSize);
	}

	uint16_t WriteDataByIdReq::getDataId(void) const
	{
		uint16_t ret = 0;
		for(int i = 0; i < dataIdSize; ++i) {
			((uint8_t *)&ret)[i] = packet[dataIdPos + i];
		}
		return ret;
	}

	bool WriteDataByIdReq::existDataRecord(void) const
	{
		return packet.length() > dataRecordPos;
	}

	void WriteDataByIdReq::getDataRecord(QVector<uint8_t> &outRef) const
	{
		outRef.clear();
		int outSize = packet.length() - dataRecordPos;
		for(int i = 0; i < outSize; ++i) {
			outRef.append(packet[dataRecordPos + i]);
		}
	}

	WriteDataByIdPosResp::WriteDataByIdPosResp (const QVector<uint8_t> &packetRef) :
		packet(packetRef)
	{
	}

	bool WriteDataByIdPosResp::existSid(void) const
	{
		return packet.length() >= (sidPos + sidSize);
	}

	uint8_t WriteDataByIdPosResp::getSid(void) const
	{
		return packet[sidPos];
	}

	bool WriteDataByIdPosResp::existDataId(void) const
	{
		return packet.length() >= (dataIdPos + dataIdSize);
	}

	uint16_t WriteDataByIdPosResp::getDataId(void) const
	{
		uint16_t ret = 0;
		for(int i = 0; i < dataIdSize; ++i) {
			((uint8_t *)&ret)[i] = packet[dataIdPos + i];
		}
		return ret;
	}


	RoutineCtrlReq::RoutineCtrlReq (const QVector<uint8_t> &packetRef) :
		packet(packetRef)
	{
	}

	bool RoutineCtrlReq::existSid(void) const
	{
		return packet.length() >= (sidPos + sidSize);
	}

	uint8_t RoutineCtrlReq::getSid(void) const
	{
		return packet[sidPos];
	}

	bool RoutineCtrlReq::existSubFunc(void) const
	{
		return packet.length() >= (subFuncPos + subFuncSize);
	}

	uint8_t RoutineCtrlReq::getSubFunc(void) const
	{
		return packet[subFuncPos];
	}

	bool RoutineCtrlReq::existRid(void) const
	{
		return packet.length() >= (ridPos + ridSize);
	}

	uint16_t RoutineCtrlReq::getRid(void) const
	{
		uint16_t ret = 0;
		for(int i = 0; i < ridSize; ++i) {
			((uint8_t *)&ret)[i] = packet[ridPos + i];
		}
		return ret;
	}

	bool RoutineCtrlReq::existOptRecord(void) const
	{
		return packet.length() > optRecordPos;
	}

	void RoutineCtrlReq::getOptRecord(QVector<uint8_t> &outRef) const
	{
		outRef.clear();
		int outSize = packet.length() - optRecordPos;
		for(int i = 0; i < outSize; ++i) {
			outRef.append(packet[optRecordPos + i]);
		}
	}

	RoutineCtrlPosResp::RoutineCtrlPosResp (const QVector<uint8_t> &packetRef) :
		packet(packetRef)
	{
	}

	bool RoutineCtrlPosResp::existSid(void) const
	{
		return packet.length() >= (sidPos + sidSize);
	}

	uint8_t RoutineCtrlPosResp::getSid(void) const
	{
		return packet[sidPos];
	}

	bool RoutineCtrlPosResp::existType(void) const
	{
		return packet.length() >= (typePos + typeSize);
	}

	uint8_t RoutineCtrlPosResp::getType(void) const
	{
		return packet[typePos];
	}

	bool RoutineCtrlPosResp::existRid(void) const
	{
		return packet.length() >= (ridPos + ridSize);
	}

	uint16_t RoutineCtrlPosResp::getRid(void) const
	{
		uint16_t ret = 0;
		for(int i = 0; i < ridSize; ++i) {
			((uint8_t *)&ret)[i] = packet[ridPos + i];
		}
		return ret;
	}

	bool RoutineCtrlPosResp::existInfo(void) const
	{
		return packet.length() >= (infoPos + infoSize);
	}

	uint8_t RoutineCtrlPosResp::getInfo(void) const
	{
		return packet[infoPos];
	}

	bool RoutineCtrlPosResp::existStatusRecord(void) const
	{
		return packet.length() > statusRecordPos;
	}

	void RoutineCtrlPosResp::getStatusRecord(QVector<uint8_t> &outRef) const
	{
		outRef.clear();
		int outSize = packet.length() - statusRecordPos;
		for(int i = 0; i < outSize; ++i) {
			outRef.append(packet[statusRecordPos + i]);
		}
	}

	NegResp::NegResp (const QVector<uint8_t> &packetRef) :
		packet(packetRef)
	{
	}

	bool NegResp::existSid(void) const
	{
		return packet.length() >= (sidPos + sidSize);
	}

	uint8_t NegResp::getSid(void) const
	{
		return packet[sidPos];
	}

	bool NegResp::existReqSid(void) const
	{
		return packet.length() >= (reqSidPos + reqSidSize);
	}

	uint8_t NegResp::getReqSid(void) const
	{
		return packet[reqSidPos];
	}

	bool NegResp::existNrc(void) const
	{
		return packet.length() >= (nrcPos + nrcSize);
	}

	uint8_t NegResp::getNrc(void) const
	{
		return packet[nrcPos];
	}


	CommCtrlReq::CommCtrlReq (const QVector<uint8_t> &packetRef) :
		packet(packetRef)
	{
	}

	bool CommCtrlReq::existSid(void) const
	{
		return packet.length() >= (sidPos + sidSize);
	}

	uint8_t CommCtrlReq::getSid(void) const
	{
		return packet[sidPos];
	}

	bool CommCtrlReq::existSubFunc(void) const
	{
		return packet.length() >= (subFuncPos + subFuncSize);
	}

	uint8_t CommCtrlReq::getSubFunc(void) const
	{
		return packet[subFuncPos];
	}

	bool CommCtrlReq::existCommType(void) const
	{
		return packet.length() >= (commTypePos + commTypeSize);
	}

	uint8_t CommCtrlReq::getCommType(void) const
	{
		return packet[commTypePos];
	}

	bool CommCtrlReq::existNodeIdHigh(void) const
	{
		return packet.length() >= (nodeIdHighPos + nodeIdHighSize);
	}

	uint8_t CommCtrlReq::getNodeIdHigh(void) const
	{
		return packet[nodeIdHighPos];
	}

	bool CommCtrlReq::existNodeIdLow(void) const
	{
		return packet.length() >= (nodeIdLowPos + nodeIdLowSize);
	}

	uint8_t CommCtrlReq::getNodeIdLow(void) const
	{
		return packet[nodeIdLowPos];
	}


	TransferDataReq::TransferDataReq (const QVector<uint8_t> &packetRef) :
		packet(packetRef)
	{
	}

	bool TransferDataReq::existSid(void) const
	{
		return packet.length() >= (sidPos + sidSize);
	}

	uint8_t TransferDataReq::getSid(void) const
	{
		return packet[sidPos];
	}

	bool TransferDataReq::existBsc(void) const
	{
		return packet.length() >= (bscPos + bscSize);
	}

	uint8_t TransferDataReq::getBsc(void) const
	{
		return packet[bscPos];
	}

	bool TransferDataReq::existData(void) const
	{
		return packet.length() > dataPos;
	}

	void TransferDataReq::getData(QVector<uint8_t> &outRef) const
	{
		outRef.clear();
		int outSize = packet.length() - dataPos;
		for(int i = 0; i < outSize; ++i) {
			outRef.append(packet[dataPos + i]);
		}
	}

	TransferDataPosResp::TransferDataPosResp (const QVector<uint8_t> &packetRef) :
		packet(packetRef)
	{
	}

	bool TransferDataPosResp::existSid(void) const
	{
		return packet.length() >= (sidPos + sidSize);
	}

	uint8_t TransferDataPosResp::getSid(void) const
	{
		return packet[sidPos];
	}

	bool TransferDataPosResp::existBsc(void) const
	{
		return packet.length() >= (bscPos + bscSize);
	}

	uint8_t TransferDataPosResp::getBsc(void) const
	{
		return packet[bscPos];
	}

	bool TransferDataPosResp::existParamRecord(void) const
	{
		return packet.length() > paramRecordPos;
	}

	void TransferDataPosResp::getParamRecord(QVector<uint8_t> &outRef) const
	{
		outRef.clear();
		int outSize = packet.length() - paramRecordPos;
		for(int i = 0; i < outSize; ++i) {
			outRef.append(packet[paramRecordPos + i]);
		}
	}

	ReadDtcInfoGenericReq::ReadDtcInfoGenericReq (const QVector<uint8_t> &packetRef) :
		packet(packetRef)
	{
	}

	bool ReadDtcInfoGenericReq::existSid(void) const
	{
		return packet.length() >= (sidPos + sidSize);
	}

	uint8_t ReadDtcInfoGenericReq::getSid(void) const
	{
		return packet[sidPos];
	}

	bool ReadDtcInfoGenericReq::existSubFunc(void) const
	{
		return packet.length() >= (subFuncPos + subFuncSize);
	}

	uint8_t ReadDtcInfoGenericReq::getSubFunc(void) const
	{
		return packet[subFuncPos];
	}

	bool ReadDtcInfoGenericReq::existBuf(void) const
	{
		return packet.length() > bufPos;
	}

	void ReadDtcInfoGenericReq::getBuf(QVector<uint8_t> &outRef) const
	{
		outRef.clear();
		int outSize = packet.length() - bufPos;
		for(int i = 0; i < outSize; ++i) {
			outRef.append(packet[bufPos + i]);
		}
	}

	ReadDtcInfoGenericPosResp::ReadDtcInfoGenericPosResp (const QVector<uint8_t> &packetRef) :
		packet(packetRef)
	{
	}

	bool ReadDtcInfoGenericPosResp::existSid(void) const
	{
		return packet.length() >= (sidPos + sidSize);
	}

	uint8_t ReadDtcInfoGenericPosResp::getSid(void) const
	{
		return packet[sidPos];
	}

	bool ReadDtcInfoGenericPosResp::existSubFunc(void) const
	{
		return packet.length() >= (subFuncPos + subFuncSize);
	}

	uint8_t ReadDtcInfoGenericPosResp::getSubFunc(void) const
	{
		return packet[subFuncPos];
	}

	bool ReadDtcInfoGenericPosResp::existBuf(void) const
	{
		return packet.length() > bufPos;
	}

	void ReadDtcInfoGenericPosResp::getBuf(QVector<uint8_t> &outRef) const
	{
		outRef.clear();
		int outSize = packet.length() - bufPos;
		for(int i = 0; i < outSize; ++i) {
			outRef.append(packet[bufPos + i]);
		}
	}

	ReadDtcInfoByStReq::ReadDtcInfoByStReq (const QVector<uint8_t> &packetRef) :
		packet(packetRef)
	{
	}

	bool ReadDtcInfoByStReq::existSid(void) const
	{
		return packet.length() >= (sidPos + sidSize);
	}

	uint8_t ReadDtcInfoByStReq::getSid(void) const
	{
		return packet[sidPos];
	}

	bool ReadDtcInfoByStReq::existSubFunc(void) const
	{
		return packet.length() >= (subFuncPos + subFuncSize);
	}

	uint8_t ReadDtcInfoByStReq::getSubFunc(void) const
	{
		return packet[subFuncPos];
	}

	bool ReadDtcInfoByStReq::existMask(void) const
	{
		return packet.length() >= (maskPos + maskSize);
	}

	uint8_t ReadDtcInfoByStReq::getMask(void) const
	{
		return packet[maskPos];
	}


	ReadDtcInfoByStPosResp::ReadDtcInfoByStPosResp (const QVector<uint8_t> &packetRef) :
		packet(packetRef)
	{
	}

	bool ReadDtcInfoByStPosResp::existSid(void) const
	{
		return packet.length() >= (sidPos + sidSize);
	}

	uint8_t ReadDtcInfoByStPosResp::getSid(void) const
	{
		return packet[sidPos];
	}

	bool ReadDtcInfoByStPosResp::existReportType(void) const
	{
		return packet.length() >= (reportTypePos + reportTypeSize);
	}

	uint8_t ReadDtcInfoByStPosResp::getReportType(void) const
	{
		return packet[reportTypePos];
	}

	bool ReadDtcInfoByStPosResp::existAvailStMask(void) const
	{
		return packet.length() >= (availStMaskPos + availStMaskSize);
	}

	uint8_t ReadDtcInfoByStPosResp::getAvailStMask(void) const
	{
		return packet[availStMaskPos];
	}

	bool ReadDtcInfoByStPosResp::existDtcNStRecord(void) const
	{
		return packet.length() > dtcNStRecordPos;
	}

	void ReadDtcInfoByStPosResp::getDtcNStRecord(QVector<uint8_t> &outRef) const
	{
		outRef.clear();
		int outSize = packet.length() - dtcNStRecordPos;
		for(int i = 0; i < outSize; ++i) {
			outRef.append(packet[dtcNStRecordPos + i]);
		}
	}

	DtcNStatusRecordPosResp::DtcNStatusRecordPosResp (const QVector<uint8_t> &packetRef) :
		packet(packetRef)
	{
	}

	bool DtcNStatusRecordPosResp::existDtcHighByte(void) const
	{
		return packet.length() >= (dtcHighBytePos + dtcHighByteSize);
	}

	uint8_t DtcNStatusRecordPosResp::getDtcHighByte(void) const
	{
		return packet[dtcHighBytePos];
	}

	bool DtcNStatusRecordPosResp::existDtcMiddleByte(void) const
	{
		return packet.length() >= (dtcMiddleBytePos + dtcMiddleByteSize);
	}

	uint8_t DtcNStatusRecordPosResp::getDtcMiddleByte(void) const
	{
		return packet[dtcMiddleBytePos];
	}

	bool DtcNStatusRecordPosResp::existDtcLowByte(void) const
	{
		return packet.length() >= (dtcLowBytePos + dtcLowByteSize);
	}

	uint8_t DtcNStatusRecordPosResp::getDtcLowByte(void) const
	{
		return packet[dtcLowBytePos];
	}

	bool DtcNStatusRecordPosResp::existStatusOfDtc(void) const
	{
		return packet.length() >= (statusOfDtcPos + statusOfDtcSize);
	}

	uint8_t DtcNStatusRecordPosResp::getStatusOfDtc(void) const
	{
		return packet[statusOfDtcPos];
	}




	const ServSub servDiagSessCtrlSubReserved1 (
		static_cast<uint8_t>(ServSubDiagSessCtrl::reserved1), // ISOSAEReserved
		"ISOSAEReserved",
		"This value is reserved by this document."
	);
	const ServSub servDiagSessCtrlSubDef (
		static_cast<uint8_t>(ServSubDiagSessCtrl::def), // defaultSession
		"defaultSession",
		"This diagnostic session enables the default diagnostic session in the server(s) and does not support any diagnostic application timeout handling provisions (e.g. no TesterPresent service is necessary to keep the session active).\nIf any other session than the defaultSession has been active in the server and the defaultSession is once again started, then the following implementation rules shall be followed (see also the server diagnostic session state diagram given above):\nThe server shall stop the current diagnostic session when it has sent the DiagnosticSessionControl positive response message and shall start the newly requested diagnostic session afterwards.\nIf the server has sent a DiagnosticSessionControl positive response message it shall have re-locked the server if the client unlocked it during the diagnostic session.\nIf the server sends a negative response message with the DiagnosticSessionControl request service identifier the active session shall be continued.\nIn case the used data link requires an initialization step then the initialized server(s) shall start the default diagnostic session by default. No DiagnosticSessionControl with diagnosticSession set to defaultSession shall be required after the initialization step."
	);
	const ServSub servDiagSessCtrlSubProg (
		static_cast<uint8_t>(ServSubDiagSessCtrl::prog), // ProgrammingSession
		"ProgrammingSession",
		"This diagnosticSession enables all diagnostic services required to support the memory programming of a server.\nIt is vehicle-manufacturer specific whether the positive response is sent prior or after the ECU has switched to/from programmingSession.\nIn case the server runs the programmingSession in the boot software, the programmingSession shall only be left via an ECUReset (1116) service initiated by the client, a DiagnosticSessionControl (1016) service with sessionType equal to defaultSession, or a session layer timeout in the server.\nIn case the server runs in the boot software when it receives the DiagnosticSessionControl (1016) service with sessionType equal to defaultSession or a session layer timeout occurs and a valid application software is present for both cases then the server shall restart the application software. This document does not specify the various implementation methods of how to achieve the restart of the valid application software (e.g. a valid application software can be determined directly in the boot software, during the ECU startup phase when performing an ECU reset, etc.)."
	);
	const ServSub servDiagSessCtrlSubExtdDiag (
		static_cast<uint8_t>(ServSubDiagSessCtrl::extdDiag), // Extended Diagnostic Session
		"Extended Diagnostic Session",
		"This diagnosticSession can be used to enable all diagnostic services required to support the adjustment of functions like \"Idle Speed, CO Value, etc.\" in the server's memory. It can also be used to enable diagnostic services, which are not specifically tied to the adjustment of functions (e.g. refer to timed services in Table 23)."
	);
	const ServSub servDiagSessCtrlSubSafetySysDiag (
		static_cast<uint8_t>(ServSubDiagSessCtrl::safetySysDiag), // Safety System Diagnostic Session
		"Safety System Diagnostic Session",
		"This diagnosticSession enables all diagnostic services required to support safety system related functions (e.g. airbag deployment)."
	);
	const ServSub servDiagSessCtrlSubReserved2 (
		static_cast<uint8_t>(ServSubDiagSessCtrl::reserved2Start), // ISOSAEReserved
		static_cast<uint8_t>(ServSubDiagSessCtrl::reserved2End), // ISOSAEReserved
		"ISOSAEReserved",
		"Reserved for future definition."
	);
	const ServSub servDiagSessCtrlSubVehicleManufacturerSpecific (
		static_cast<uint8_t>(ServSubDiagSessCtrl::vehicleManufacturerSpecificStart), // Vehicle Manufacturer Specific
		static_cast<uint8_t>(ServSubDiagSessCtrl::vehicleManufacturerSpecificEnd), // Vehicle Manufacturer Specific
		"Vehicle Manufacturer Specific",
		"Reserved for vehicle manufacturer-specific use."
	);
	const ServSub servDiagSessCtrlSubSysSupplierSpecific (
		static_cast<uint8_t>(ServSubDiagSessCtrl::sysSupplierSpecificStart), // System Supplier Specific
		static_cast<uint8_t>(ServSubDiagSessCtrl::sysSupplierSpecificEnd), // System Supplier Specific
		"System Supplier Specific",
		"Reserved for system supplier-specific use."
	);
	const ServSub servDiagSessCtrlSubReserved3 (
		static_cast<uint8_t>(ServSubDiagSessCtrl::reserved3), // ISOSAEReserved
		"ISOSAEReserved",
		"Reserved for future definition."
	);

	const ServSub servEcuResetSubReserved1 (
		static_cast<uint8_t>(ServSubEcuReset::reserved1), // ISOSAEReserved
		"ISOSAEReserved",
		"This value is reserved by this document."
	);
	const ServSub servEcuResetSubHardReset (
		static_cast<uint8_t>(ServSubEcuReset::hardReset), // hardReset
		"hardReset",
		"This SubFunction identifies a \"hard reset\" condition which simulates the power-on/start-up sequence typically performed after a server has been previously disconnected from its power supply (i.e. battery). The performed action is implementation specific and not defined by this document. It might result in the re-initialization of both volatile memory and non-volatile memory locations to predetermined values."
	);
	const ServSub servEcuResetSubKeyOffOnReset (
		static_cast<uint8_t>(ServSubEcuReset::keyOffOnReset), // keyOffOnReset
		"keyOffOnReset",
		"This SubFunction identifies a condition similar to the driver turning the ignition key off and back on. This reset condition should simulate a key-off-on sequence (i.e. interrupting the switched power supply). The performed action is implementation specific and not defined by this document. Typically the values of non-volatile memory locations are preserved; volatile memory will be initialized."
	);
	const ServSub servEcuResetSubSoftReset (
		static_cast<uint8_t>(ServSubEcuReset::softReset), // softReset
		"softReset",
		"This SubFunction identifies a \"soft reset\" condition, which causes the server to immediately restart the application program if applicable. The performed action is implementation specific and not defined by this document. A typical action is to restart the application without reinitializing of previously learned configuration data, adaptive factors and other long-term adjustments."
	);
	const ServSub servEcuResetSubEnRapidPwrShutDown (
		static_cast<uint8_t>(ServSubEcuReset::enRapidPwrShutDown), // enableRapidPowerShutDown
		"enableRapidPowerShutDown",
		"This SubFunction applies to ECUs which are not ignition powered but battery powered only. Therefore a shutdown forces the sleep mode rather than a power off. Sleep means power off but still ready for wake-up (battery powered). The intention of the SubFunction is to reduce the stand-by time of an ECU after ignition is turned into the off position. This value requests the server to enable and perform a \"rapid power shut down\" function. The server shall execute the function immediately once the \"key/ignition” is switched off. While the server executes the power down function, it shall transition either directly or after a defined stand-by-time to sleep mode. If the client requires a response message and the server is already prepared to execute the \"rapid power shut down\" function, the server shall send the positive response message prior to the start of the \"rapid power shut down\" function. The next occurrence of a \"key on\" or \"ignition on\" signal terminates the \"rapid power shut down\" function. NOTE This SubFunction is only applicable to a server supporting a stand-by-mode."
	);
	const ServSub servEcuResetSubDisableRapidPwrShutdown (
		static_cast<uint8_t>(ServSubEcuReset::disableRapidPwrShutdown), // disableRapidPowerShutDown
		"disableRapidPowerShutDown",
		"This SubFunction requests the server to disable the previously enabled \"rapid power shut down\" function."
	);
	const ServSub servEcuResetSubReserved2 (
		static_cast<uint8_t>(ServSubEcuReset::reserved2Start), // ISOSAEReserved
		static_cast<uint8_t>(ServSubEcuReset::reserved2End), // ISOSAEReserved
		"ISOSAEReserved",
		"This range of values is reserved by this document for future definition."
	);
	const ServSub servEcuResetSubVehicleManufacturerSpecific (
		static_cast<uint8_t>(ServSubEcuReset::vehicleManufacturerSpecificStart), // vehicleManufacturerSpecific
		static_cast<uint8_t>(ServSubEcuReset::vehicleManufacturerSpecificEnd), // vehicleManufacturerSpecific
		"vehicleManufacturerSpecific",
		"This range of values is reserved for vehicle manufacturer specific use."
	);
	const ServSub servEcuResetSubSysSuppSpecific (
		static_cast<uint8_t>(ServSubEcuReset::sysSuppSpecificStart), // systemSupplierSpecific
		static_cast<uint8_t>(ServSubEcuReset::sysSuppSpecificEnd), // systemSupplierSpecific
		"systemSupplierSpecific",
		"This range of values is reserved for system supplier specific use."
	);
	const ServSub servEcuResetSubReserved3 (
		static_cast<uint8_t>(ServSubEcuReset::reserved3), // ISOSAEReserved
		"ISOSAEReserved",
		"This value is reserved by this document for future definition."
	);

	const ServSub servSecurityAccessSubReserved1 (
		static_cast<uint8_t>(ServSubSecurityAccess::reserved1), // ISOSAEReserved
		"ISOSAEReserved",
		"Reserved by this document"
	);
	const ServSub servSecurityAccessSubReqSeedDef (
		static_cast<uint8_t>(ServSubSecurityAccess::reqSeedDef), // requestSeed
		"requestSeed",
		"RequestSeed with the level of security defined by the vehicle manufacturer"
	);
	const ServSub servSecurityAccessSubSendKeyDef (
		static_cast<uint8_t>(ServSubSecurityAccess::sendKeyDef), // sendKey
		"sendKey",
		"SendKey with the level of security defined by the vehicle manufacturer"
	);
	const ServSub servSecurityAccessSubReqSeedLevel (
		static_cast<uint8_t>(ServSubSecurityAccess::reqSeedLevel), // requestSeed
		"requestSeed",
		"RequestSeed with different levels of security defined by the vehicle manufacturer"
	);
	const ServSub servSecurityAccessSubReqSeedLevel1 (
		static_cast<uint8_t>(ServSubSecurityAccess::reqSeedLevel1), // requestSeed
		"requestSeed",
		"RequestSeed with different levels of security defined by the vehicle manufacturer"
	);
	const ServSub servSecurityAccessSubReqSeedLevel2 (
		static_cast<uint8_t>(ServSubSecurityAccess::reqSeedLevel2Start), // requestSeed
		static_cast<uint8_t>(ServSubSecurityAccess::reqSeedLevel2End), // requestSeed
		"requestSeed",
		"RequestSeed with different levels of security defined by the vehicle manufacturer"
	);
	const ServSub servSecurityAccessSubSendKeyLevel (
		static_cast<uint8_t>(ServSubSecurityAccess::sendKeyLevel), // sendKey
		"sendKey",
		"SendKey with different levels of security defined by the vehicle manufacturer"
	);
	const ServSub servSecurityAccessSubSendKeyLevel1 (
		static_cast<uint8_t>(ServSubSecurityAccess::sendKeyLevel1), // sendKey
		"sendKey",
		"SendKey with different levels of security defined by the vehicle manufacturer"
	);
	const ServSub servSecurityAccessSubSendKeyLevel2 (
		static_cast<uint8_t>(ServSubSecurityAccess::sendKeyLevel2Start), // sendKey
		static_cast<uint8_t>(ServSubSecurityAccess::sendKeyLevel2End), // sendKey
		"sendKey",
		"SendKey with different levels of security defined by the vehicle manufacturer"
	);
	const ServSub servSecurityAccessSubReserved2 (
		static_cast<uint8_t>(ServSubSecurityAccess::reserved2Start), // ISOSAEReserved
		static_cast<uint8_t>(ServSubSecurityAccess::reserved2End), // ISOSAEReserved
		"ISOSAEReserved",
		"Reserved by this document for future definition"
	);
	const ServSub servSecurityAccessSubIso26021ReqSeed (
		static_cast<uint8_t>(ServSubSecurityAccess::iso26021ReqSeed), // ISO26021-2 values
		"ISO26021-2 values",
		"RequestSeed for end-of-life activation of on-board pyrotechnic devices as defined in ISO 26021-2"
	);
	const ServSub servSecurityAccessSubIso26021SendKey (
		static_cast<uint8_t>(ServSubSecurityAccess::iso26021SendKey), // ISO26021-2 sendKey values
		"ISO26021-2 sendKey values",
		"SendKey for end-of-life activation of on-board pyrotechnic devices as defined in ISO 26021-2"
	);
	const ServSub servSecurityAccessSubSysSuppSpecific (
		static_cast<uint8_t>(ServSubSecurityAccess::sysSuppSpecificStart), // systemSupplierSpecific
		static_cast<uint8_t>(ServSubSecurityAccess::sysSuppSpecificEnd), // systemSupplierSpecific
		"systemSupplierSpecific",
		"Reserved for system supplier-specific use"
	);
	const ServSub servSecurityAccessSubReserved3 (
		static_cast<uint8_t>(ServSubSecurityAccess::reserved3), // ISOSAEReserved
		"ISOSAEReserved",
		"Reserved by this document for future definition"
	);

	const ServSub servCommCtrlSubEnRxAndTx (
		static_cast<uint8_t>(ServSubCommCtrl::enRxAndTx), // enableRxAndTx
		"enableRxAndTx",
		"This value indicates that the reception and transmission of messages shall be enabled for the specified communicationType."
	);
	const ServSub servCommCtrlSubEnRxAndDisTx (
		static_cast<uint8_t>(ServSubCommCtrl::enRxAndDisTx), // enableRxAndDisableTx
		"enableRxAndDisableTx",
		"This value indicates that the reception of messages shall be enabled and the transmission shall be disabled for the specified communicationType."
	);
	const ServSub servCommCtrlSubDisRxAndEnTx (
		static_cast<uint8_t>(ServSubCommCtrl::disRxAndEnTx), // disableRxAndEnableTx
		"disableRxAndEnableTx",
		"This value indicates that the reception of messages shall be disabled and the transmission shall be enabled for the specified communicationType."
	);
	const ServSub servCommCtrlSubDisRxAndTx (
		static_cast<uint8_t>(ServSubCommCtrl::disRxAndTx), // disableRxAndTx
		"disableRxAndTx",
		"This value indicates that the reception and transmission of messages shall be disabled for the specified communicationType."
	);
	const ServSub servCommCtrlSubEnRxAndDisTxWEnchancedAddrInfo (
		static_cast<uint8_t>(ServSubCommCtrl::enRxAndDisTxWEnchancedAddrInfo), // enableRxAndDisableTxWithEnhancedAddressInformation
		"enableRxAndDisableTxWithEnhancedAddressInformation",
		"This value indicates that the addressed bus master shall switch the related sub-bus segment to the diagnostic-only scheduling mode."
	);
	const ServSub servCommCtrlSubEnRxAndTxWEnhancedAddrInfo (
		static_cast<uint8_t>(ServSubCommCtrl::enRxAndTxWEnhancedAddrInfo), // enableRxAndTxWithEnhancedAddressInformation
		"enableRxAndTxWithEnhancedAddressInformation",
		"This value indicates that the addressed bus master shall switch the related sub-bus segment to the application scheduling mode."
	);
	const ServSub servCommCtrlSubReserved1 (
		static_cast<uint8_t>(ServSubCommCtrl::reserved1Start), // ISOSAEReserved
		static_cast<uint8_t>(ServSubCommCtrl::reserved1End), // ISOSAEReserved
		"ISOSAEReserved",
		"This range of values is reserved by this document for future definition."
	);
	const ServSub servCommCtrlSubVehicleMfrSpecific (
		static_cast<uint8_t>(ServSubCommCtrl::vehicleMfrSpecificStart), // vehicleManufacturerSpecific
		static_cast<uint8_t>(ServSubCommCtrl::vehicleMfrSpecificEnd), // vehicleManufacturerSpecific
		"vehicleManufacturerSpecific",
		"This range of values is reserved for vehicle manufacturer specific use."
	);
	const ServSub servCommCtrlSubSysSuppSpecific (
		static_cast<uint8_t>(ServSubCommCtrl::sysSuppSpecificStart), // systemSupplierSpecific
		static_cast<uint8_t>(ServSubCommCtrl::sysSuppSpecificEnd), // systemSupplierSpecific
		"systemSupplierSpecific",
		"This range of values is reserved for system supplier specific use."
	);
	const ServSub servCommCtrlSubReserved2 (
		static_cast<uint8_t>(ServSubCommCtrl::reserved2), // ISOSAEReserved
		"ISOSAEReserved",
		"This value is reserved by this document for future definition."
	);

	const ServSub servTesterPresentSubNone (
		static_cast<uint8_t>(ServSubTesterPresent::none), // zeroSubFunction
		"zeroSubFunction",
		"This parameter value is used to indicate that no SubFunction value beside the suppressPosRspMsgIndicationBit is supported by this service."
	);
	const ServSub servTesterPresentSubReserved1 (
		static_cast<uint8_t>(ServSubTesterPresent::reserved1Start), // ISOSAEReserved
		static_cast<uint8_t>(ServSubTesterPresent::reserved1End), // ISOSAEReserved
		"ISOSAEReserved",
		"This range of values is reserved by this document."
	);

	const ServSub servReadDtcInfoSubReserved1 (
		static_cast<uint8_t>(ServSubReadDtcInfo::reserved1), // ISOSAEReserved
		"ISOSAEReserved",
		"This value is reserved by this document for future definition."
	);
	const ServSub servReadDtcInfoSubReportNumOfDtcByStatusMask (
		static_cast<uint8_t>(ServSubReadDtcInfo::reportNumOfDtcByStatusMask), // reportNumberOfDTCByStatusMask
		"reportNumberOfDTCByStatusMask",
		"This parameter specifies that the server shall transmit to the client the number of DTCs matching a client defined status mask."
	);
	const ServSub servReadDtcInfoSubReportDtcByStatusMask (
		static_cast<uint8_t>(ServSubReadDtcInfo::reportDtcByStatusMask), // reportDTCByStatusMask
		"reportDTCByStatusMask",
		"This parameter specifies that the server shall transmit to the client a list of DTCs and corresponding statuses matching a client defined status mask."
	);
	const ServSub servReadDtcInfoSubReportDtcSnapshotId (
		static_cast<uint8_t>(ServSubReadDtcInfo::reportDtcSnapshotId), // reportDTCSnapshotIdentification
		"reportDTCSnapshotIdentification",
		"This parameter specifies that the server shall transmit to the client all DTCSnapshot data record identifications [DTC number(s) and DTCSnapshot record number(s)]."
	);
	const ServSub servReadDtcInfoSubReportDtcSnapshotRecordByDtcNum (
		static_cast<uint8_t>(ServSubReadDtcInfo::reportDtcSnapshotRecordByDtcNum), // reportDTCSnapshotRecordByDTCNumber
		"reportDTCSnapshotRecordByDTCNumber",
		"This parameter specifies that the server shall transmit to the client the DTCSnapshot record(s) associated with a client defined DTC number and DTCSnapshot record number (FF16 for all records)."
	);
	const ServSub servReadDtcInfoSubReportDtcStoredDataByRecordNum (
		static_cast<uint8_t>(ServSubReadDtcInfo::reportDtcStoredDataByRecordNum), // reportDTCStoredDataByRecordNumber
		"reportDTCStoredDataByRecordNumber",
		"This parameter specifies that the server shall transmit to the client the DTCStoredDatarecord(s) associated with a client defined DTCStoredData record number (FF16 for all records)."
	);
	const ServSub servReadDtcInfoSubReportDtcExtDataRecordByDtcNum (
		static_cast<uint8_t>(ServSubReadDtcInfo::reportDtcExtDataRecordByDtcNum), // reportDTCExtDataRecordByDTCNumber
		"reportDTCExtDataRecordByDTCNumber",
		"This parameter specifies that the server shall transmit to the client the DTCExtendedData record(s) associated with a client defined DTC number and DTCExtendedData record number (FF16 for all records, FE16 for all OBD records)."
	);
	const ServSub servReadDtcInfoSubReportNumOfDtcBySeverityMaskRecord (
		static_cast<uint8_t>(ServSubReadDtcInfo::reportNumOfDtcBySeverityMaskRecord), // reportNumberOfDTCBySeverityMaskRecord
		"reportNumberOfDTCBySeverityMaskRecord",
		"This parameter specifies that the server shall transmit to the client the number of DTCs matching a client defined severity mask record."
	);
	const ServSub servReadDtcInfoSubReportDtcBySeverityMaskRecord (
		static_cast<uint8_t>(ServSubReadDtcInfo::reportDtcBySeverityMaskRecord), // reportDTCBySeverityMaskRecord
		"reportDTCBySeverityMaskRecord",
		"This parameter specifies that the server shall transmit to the client a list of DTCs and corresponding statuses matching a client defined severity mask record."
	);
	const ServSub servReadDtcInfoSubReportSeverityInformationOfDtc (
		static_cast<uint8_t>(ServSubReadDtcInfo::reportSeverityInformationOfDtc), // reportSeverityInformationOfDTC
		"reportSeverityInformationOfDTC",
		"This parameter specifies that the server shall transmit to the client the severity information of a specific DTC specified in the client request message."
	);
	const ServSub servReadDtcInfoSubReportSuppDtc (
		static_cast<uint8_t>(ServSubReadDtcInfo::reportSuppDtc), // reportSupportedDTC
		"reportSupportedDTC",
		"This parameter specifies that the server shall transmit to the client a list of all DTCs and corresponding statuses supported within the server."
	);
	const ServSub servReadDtcInfoSubReportFirstTestFailedDtc (
		static_cast<uint8_t>(ServSubReadDtcInfo::reportFirstTestFailedDtc), // reportFirstTestFailedDTC
		"reportFirstTestFailedDTC",
		"This parameter specifies that the server shall transmit to the client the first failed DTC to be detected by the server since the last clear of diagnostic information. Note that the information reported via this SubFunction parameter shall be independent of whether or not the DTC was confirmed or aged."
	);
	const ServSub servReadDtcInfoSubReportFirstConfirmedDtc (
		static_cast<uint8_t>(ServSubReadDtcInfo::reportFirstConfirmedDtc), // reportFirstConfirmedDTC
		"reportFirstConfirmedDTC",
		"This parameter specifies that the server shall transmit to the client the first confirmed DTC to be detected by the server since the last clear of diagnostic information. The information reported via this SubFunction parameter shall be independent of the aging process of confirmed DTCs (e.g. if a DTC ages such that its status is allowed to be reset, the first confirmed DTC record shall continue to be preserved by the server, regardless of any other DTCs that become confirmed afterwards)."
	);
	const ServSub servReadDtcInfoSubReportMostRecentTestFailedDtc (
		static_cast<uint8_t>(ServSubReadDtcInfo::reportMostRecentTestFailedDtc), // reportMostRecentTestFailedDTC
		"reportMostRecentTestFailedDTC",
		"This parameter specifies that the server shall transmit to the client the most recent failed DTC to be detected by the server since the last clear of diagnostic information. Note that the information reported via this SubFunction parameter shall be independent of whether or not the DTC was confirmed or aged."
	);
	const ServSub servReadDtcInfoSubReportMostRecentConfirmedDtc (
		static_cast<uint8_t>(ServSubReadDtcInfo::reportMostRecentConfirmedDtc), // reportMostRecentConfirmedDTC
		"reportMostRecentConfirmedDTC",
		"This parameter specifies that the server shall transmit to the client the most recent confirmed DTC to be detected by the server since the last clear of diagnostic information. Note that the information reported via this SubFunction parameter shall be independent of the aging process of confirmed DTCs (e.g. if a DTC ages such that its status is allowed to be reset, the first confirmed DTC record shall continue to be preserved by the server assuming no other DTCs become confirmed afterwards)."
	);
	const ServSub servReadDtcInfoSubReportDtcFaultDetectionCounter (
		static_cast<uint8_t>(ServSubReadDtcInfo::reportDtcFaultDetectionCounter), // reportDTCFaultDetectionCounter
		"reportDTCFaultDetectionCounter",
		"This parameter specifies that the server shall transmit to the client a list of current \"prefailed\" DTCs which have or have not yet been detected as \"pending\" or \"confirmed\".\nThe intention of the DTCFaultDetectionCounter is a simple method to identify a growing or intermittent problem which can not be identified/read by the statusOfDTC byte of a particular DTC. The internal implementation of the DTCFaultDetectionCounter shall be vehicle manufacturer specific (e.g. number of bytes, signed versus unsigned, etc.) but the reported value shall be a scaled 1 byte signed value so that +127 (7F16) represents a test result of \"failed\" and any other non-zero positive value represents a test result of \"prefailed\". However DTCs with DTCFaultDetectionCounter with the value +127 shall not be reported according to below stated rule. The DTCFaultDetectionCounter shall be incremented by a vehicle manufacturer specific amount each time the test logic runs and indicates a fail for that test run.\nA reported DTCFaultDetectionCounter value greater than zero and less than +127 (i.e. 0116 to 7E16) indicates that the DTC enable criteria was met and that a non completed test result prefailed at least in one condition or threshold.\nOnly DTCs with DTCFaultDetectionCounters with a non-zero positive value less than +127 (7F16) shall be reported.\nThe DTCFaultDetectionCounter shall be decremented by a vehicle manufacturer specific amount each time the test logic runs and indicates a pass for that test run. If the DTCFaultDetectionCounter is decremented to zero or below the DTC shall no longer be reported in the positive response message. The value of the DTCFaultDetectionCounter shall not be maintained between operation cycles.\nIf a ClearDiagnosticInformation service request is received the DTCFaultDetectionCounter value shall be reset to zero for all DTCs. Additional reset conditions shall be defined by the vehicle manufacturer. Refer to D.5 for example implementation details.\n"
	);
	const ServSub servReadDtcInfoSubReportDtcWithPermanentStatus (
		static_cast<uint8_t>(ServSubReadDtcInfo::reportDtcWithPermanentStatus), // reportDTCWithPermanentStatus
		"reportDTCWithPermanentStatus",
		"This parameter specifies that the server shall transmit to the client a list of DTCs with \"permanent DTC\" status as described in 3.12."
	);
	const ServSub servReadDtcInfoSubReportDtcExtDataRecordByRecordNumber (
		static_cast<uint8_t>(ServSubReadDtcInfo::reportDtcExtDataRecordByRecordNumber), // reportDTCExtDataRecordByRecordNumber
		"reportDTCExtDataRecordByRecordNumber",
		"This parameter specifies that the server shall transmit to the client the DTCExtendedData records associated with a client defined DTCExtendedData record number less than F016."
	);
	const ServSub servReadDtcInfoSubReportUserDefMemDtcByStatusMask (
		static_cast<uint8_t>(ServSubReadDtcInfo::reportUserDefMemDtcByStatusMask), // reportUserDefMemoryDTCByStatusMask
		"reportUserDefMemoryDTCByStatusMask",
		"This parameter specifies that the server shall transmit to the client a list of DTCs out of the user defined DTC memory and corresponding statuses matching a client defined status mask."
	);
	const ServSub servReadDtcInfoSubReportUserDefMemDtcSnapshotRecordByDtcNum (
		static_cast<uint8_t>(ServSubReadDtcInfo::reportUserDefMemDtcSnapshotRecordByDtcNum), // reportUserDefMemoryDTCSnapshotRecordByDTCNumber
		"reportUserDefMemoryDTCSnapshotRecordByDTCNumber",
		"This parameter specifies that the server shall transmit to the client the DTCSnapshot record(s) – out of the user defined DTC memory - associated with a client defined DTC number and DTCSnapshot record number (FF16 for all records)."
	);
	const ServSub servReadDtcInfoSubReportUserDefMemDtcExtDataRecordByDtcNum (
		static_cast<uint8_t>(ServSubReadDtcInfo::reportUserDefMemDtcExtDataRecordByDtcNum), // reportUserDefMemoryDTCExtDataRecordByDTCNumber
		"reportUserDefMemoryDTCExtDataRecordByDTCNumber",
		"This parameter specifies that the server shall transmit to the client the DTCExtendedData record(s) – out of the user defined DTC memory - associated with a client defined DTC number and DTCExtendedData record number (FF16 for all records)."
	);
	const ServSub servReadDtcInfoSubReportDtcExtendedDataRecordId (
		static_cast<uint8_t>(ServSubReadDtcInfo::reportDtcExtendedDataRecordId), // reportDTCExtendedDataRecordIdentification
		"reportDTCExtendedDataRecordIdentification",
		"This parameter specifies that the server shall transmit to the client the DTCs which supports a DTCExtendedDataRecord."
	);
	const ServSub servReadDtcInfoSubReserved2 (
		static_cast<uint8_t>(ServSubReadDtcInfo::reserved2Start), // ISOSAEReserved
		static_cast<uint8_t>(ServSubReadDtcInfo::reserved2End), // ISOSAEReserved
		"ISOSAEReserved",
		"This value is reserved by this document for future definition."
	);
	const ServSub servReadDtcInfoSubReportWWHOBDDTCByMaskRecord (
		static_cast<uint8_t>(ServSubReadDtcInfo::reportWWHOBDDTCByMaskRecord), // reportWWHOBDDTCByMaskRecord
		"reportWWHOBDDTCByMaskRecord",
		"This parameter specifies that the server shall transmit to the client a list of WWH OBD DTCs and corresponding status and severity information matching a client defined status mask and severity mask record."
	);
	const ServSub servReadDtcInfoSubReserved3 (
		static_cast<uint8_t>(ServSubReadDtcInfo::reserved3Start), // ISOSAEReserved
		static_cast<uint8_t>(ServSubReadDtcInfo::reserved3End), // ISOSAEReserved
		"ISOSAEReserved",
		"This value is reserved by this document for future definition."
	);
	const ServSub servReadDtcInfoSubReportWWHOBDDTCWithPermanentStatus (
		static_cast<uint8_t>(ServSubReadDtcInfo::reportWWHOBDDTCWithPermanentStatus), // reportWWHOBDDTCWithPermanentStatus
		"reportWWHOBDDTCWithPermanentStatus",
		"This parameter specifies that the server shall transmit to the client a list of WWH OBD DTCs with \"permanent DTC\" status as described in 3.12."
	);
	const ServSub servReadDtcInfoSubReportDtcInfoByDtcReadinessGroupId (
		static_cast<uint8_t>(ServSubReadDtcInfo::reportDtcInfoByDtcReadinessGroupId), // reportDTCInformationByDTCReadinessGroupIdentifier
		"reportDTCInformationByDTCReadinessGroupIdentifier",
		"This parameter specifies that the server shall transmit to the client a list of OBD DTCs which matches the DTCReadiness Group Identifier."
	);
	const ServSub servReadDtcInfoSubReserved4 (
		static_cast<uint8_t>(ServSubReadDtcInfo::reserved4Start), // ISOSAEReserved
		static_cast<uint8_t>(ServSubReadDtcInfo::reserved4End), // ISOSAEReserved
		"ISOSAEReserved",
		"This value is reserved by this document for future definition."
	);

	const ServSub servRoutineCtrlSubReserved1 (
		static_cast<uint8_t>(ServSubRoutineCtrl::reserved1), // ISOSAEReserved
		"ISOSAEReserved",
		"This value is reserved by this document for future definition."
	);
	const ServSub servRoutineCtrlSubStartRoutine (
		static_cast<uint8_t>(ServSubRoutineCtrl::startRoutine), // startRoutine
		"startRoutine",
		"This parameter specifies that the server shall start the routine specified by the routineIdentifier."
	);
	const ServSub servRoutineCtrlSubStopRoutine (
		static_cast<uint8_t>(ServSubRoutineCtrl::stopRoutine), // stopRoutine
		"stopRoutine",
		"This parameter specifies that the server shall stop the routine specified by the routineIdentifier."
	);
	const ServSub servRoutineCtrlSubRequestRoutineResults (
		static_cast<uint8_t>(ServSubRoutineCtrl::requestRoutineResults), // requestRoutineResults
		"requestRoutineResults",
		"This parameter specifies that the server shall return result values of the routine specified by the routineIdentifier."
	);
	const ServSub servRoutineCtrlSubReserved2 (
		static_cast<uint8_t>(ServSubRoutineCtrl::reserved2Start), // ISOSAEReserved
		static_cast<uint8_t>(ServSubRoutineCtrl::reserved2End), // ISOSAEReserved
		"ISOSAEReserved",
		"This value is reserved by this document for future definition."
	);

	const ServSub servNegRespSubPosResp (
		static_cast<uint8_t>(ServSubNegResp::posResp), // positiveResponse
		"positiveResponse",
		"This NRC shall not be used in a negative response message. This positiveResponse parameter value is reserved for server internal implementation. Refer to 8.7.5."
	);
	const ServSub servNegRespSubReserved1 (
		static_cast<uint8_t>(ServSubNegResp::reserved1Start), // ISOSAEReserved
		static_cast<uint8_t>(ServSubNegResp::reserved1End), // ISOSAEReserved
		"ISOSAEReserved",
		"This range of values is reserved by this document for future definition."
	);
	const ServSub servNegRespSubGenReject (
		static_cast<uint8_t>(ServSubNegResp::genReject), // generalReject
		"generalReject",
		"This NRC indicates that the requested action has been rejected by the server. The generalReject response code shall only be implemented in the server if none of the negative response codes defined in this document meet the needs of the implementation. At no means shall this NRC be a general replacement for the response codes defined in this document."
	);
	const ServSub servNegRespSubServNotSupp (
		static_cast<uint8_t>(ServSubNegResp::servNotSupp), // serviceNotSupported
		"serviceNotSupported",
		"This NRC indicates that the requested action will not be taken because the server does not support the requested service. The server shall send this NRC in case the client has sent a request message with a service identifier which is unknown, not supported by the server, or is specified as a response service identifier. Therefore this negative response code is not shown in the list of negative response codes to be supported for a diagnostic service, because this negative response code is not applicable for supported services."
	);
	const ServSub servNegRespSubSubFuncNotSupp (
		static_cast<uint8_t>(ServSubNegResp::subFuncNotSupp), // SubFunctionNotSupported
		"SubFunctionNotSupported",
		"This NRC indicates that the requested action will not be taken because the server does not support the service specific parameters of the request message. The server shall send this NRC in case the client has sent a request message with a known and supported service identifier but with 'SubFunction' which is either unknown or not supported."
	);
	const ServSub servNegRespSubIncorrectMsg (
		static_cast<uint8_t>(ServSubNegResp::incorrectMsg), // incorrectMessageLengthOrInvalidFormat
		"incorrectMessageLengthOrInvalidFormat",
		"This NRC indicates that the requested action will not be taken because the length of the received request message does not match the prescribed length for the specified service or the format of the paramters do not match the prescribed format for the specified service."
	);
	const ServSub servNegRespSubRespTooLong (
		static_cast<uint8_t>(ServSubNegResp::respTooLong), // responseTooLong
		"responseTooLong",
		"This NRC shall be reported by the server if the response to be generated exceeds the maximum number of bytes available by the underlying network layer. This could occur if the response message exceeds the maximum size allowed by the underlying transport protocol or if the response message exceeds the server buffer size allocated for that purpose. EXAMPLE This problem may occur when several DIDs at a time are requested and the combination of all DIDs in the response exceeds the limit of the underlying transport protocol."
	);
	const ServSub servNegRespSubReserved2 (
		static_cast<uint8_t>(ServSubNegResp::reserved2Start), // ISOSAEReserved
		static_cast<uint8_t>(ServSubNegResp::reserved2End), // ISOSAEReserved
		"ISOSAEReserved",
		"This range of values is reserved by this document for future definition."
	);
	const ServSub servNegRespSubBusyRepeat (
		static_cast<uint8_t>(ServSubNegResp::busyRepeat), // busyRepeatRequest
		"busyRepeatRequest",
		"This NRC indicates that the server is temporarily too busy to perform the requested operation. In this circumstance the client shall perform repetition of the 'identical request message' or 'another request message'. The repetition of the request shall be delayed by a time specified in the respective implementation documents. EXAMPLE In a multi-client environment the diagnostic request of one client might be blocked temporarily by an NRC 2116 while a different client finishes a diagnostic task. If the server is able to perform the diagnostic task but needs additional time to finish the task and prepare the response, the NRC 7816 shall be used instead of NRC 2116. This NRC is in general supported by each diagnostic service, as not otherwise stated in the data link specific implementation document, therefore it is not listed in the list of applicable response codes of the diagnostic services."
	);
	const ServSub servNegRespSubCondNotCorrect (
		static_cast<uint8_t>(ServSubNegResp::condNotCorrect), // conditionsNotCorrect
		"conditionsNotCorrect",
		"This NRC indicates that the requested action will not be taken because the server prerequisite conditions are not met."
	);
	const ServSub servNegRespSubReserved3 (
		static_cast<uint8_t>(ServSubNegResp::reserved3), // ISOSAEReserved
		"ISOSAEReserved",
		"This range of values is reserved by this document for future definition."
	);
	const ServSub servNegRespSubReqSeqErr (
		static_cast<uint8_t>(ServSubNegResp::reqSeqErr), // requestSequenceError
		"requestSequenceError",
		"This NRC indicates that the requested action will not be taken because the server expects a different sequence of request messages or message as sent by the client. This may occur when sequence sensitive requests are issued in the wrong order. EXAMPLE A successful SecurityAccess service specifies a sequence of requestSeed and sendKey as SubFuction in the request messages. If the sequence is sent different by the client, the server sends a negative response message with the negative response code 2416 requestSequenceError."
	);
	const ServSub servNegRespSubNoRespFromSubComp (
		static_cast<uint8_t>(ServSubNegResp::noRespFromSubComp), // noResponseFromSubnetComponent
		"noResponseFromSubnetComponent",
		"This NRC indicates that the server has received the request but the requested action could not be performed by the server as a subnet component which is necessary to supply the requested information did not respond within the specified time. The noResponseFromSubnetComponent negative response shall be implemented by gateways in electronic systems which contain electronic subnet components and which do not directly respond to the client's request. The gateway may receive the request for the subnet component and then request the necessary information from the subnet component. If the subnet component fails to respond, the server shall use this negative response to inform the client about the failure of the subnet component. This NRC is in general supported by each diagnostic service, as not otherwise stated in the data link specific implementation document, therefore it is not listed in the list of applicable response codes of the diagnostic services."
	);
	const ServSub servNegRespSubFailPreventsExec (
		static_cast<uint8_t>(ServSubNegResp::failPreventsExec), // FailurePreventsExecutionOfRequestedAction
		"FailurePreventsExecutionOfRequestedAction",
		"This NRC indicates that the requested action will not be taken because a failure condition, identified by a DTC (with at least one DTC status bit for TestFailed, Pending, Confirmed or TestFailedSinceLastClear set to 1), has occurred and that this failure condition prevents the server from performing the requested action. This NRC can, for example, direct the technician to read DTCs in order to identify and fix the problem. Diagnostic services used to access DTCs shall not implement this NRC, because an external test tool may check for the above NRC and automatically request DTCs whenever the above NRC has been received. This NRC is in general supported by each diagnostic service (except the services mentioned above), as not otherwise stated in the data link specific implementation document, therefore it is not listed in the list of applicable response codes of the diagnostic services."
	);
	const ServSub servNegRespSubReserved4 (
		static_cast<uint8_t>(ServSubNegResp::reserved4Start), // ISOSAEReserved
		static_cast<uint8_t>(ServSubNegResp::reserved4End), // ISOSAEReserved
		"ISOSAEReserved",
		"This range of values is reserved by this document for future definition."
	);
	const ServSub servNegRespSubReqOutOfRange (
		static_cast<uint8_t>(ServSubNegResp::reqOutOfRange), // requestOutOfRange
		"requestOutOfRange",
		"This NRC indicates that the requested action will not be taken because the server has detected that the request message contains a parameter which attempts to substitute a value beyond its range of authority (e.g. attempting to substitute a data byte of 111 when the data is only defined to 100), or which attempts to access a dataIdentifier/routineIdentifer that is not supported or not supported in active session. This NRC shall be implemented for all services, which allow the client to read data, write data or adjust functions by data in the server."
	);
	const ServSub servNegRespSubReserved5 (
		static_cast<uint8_t>(ServSubNegResp::reserved5), // ISOSAEReserved
		"ISOSAEReserved",
		"This range of values is reserved by this document for future definition."
	);
	const ServSub servNegRespSubSecurityAccessDenied (
		static_cast<uint8_t>(ServSubNegResp::securityAccessDenied), // securityAccessDenied
		"securityAccessDenied",
		"This NRC indicates that the requested action will not be taken because the server's security strategy has not been satisfied by the client. The server shall send this NRC if one of the following cases occur: — the test conditions of the server are not met, — the required message sequence, e.g. DiagnosticSessionControl, securityAccess is not met, — the client has sent a request message which requires an unlocked server. Beside the mandatory use of this negative response code as specified in the applicable services within this document, this negative response code can also be used for any case where security is required and is not yet granted to perform the required service."
	);
	const ServSub servNegRespSubAuthRequried (
		static_cast<uint8_t>(ServSubNegResp::authRequried), // authenticationRequired
		"authenticationRequired",
		"This NRC indicates that the requested service will not be taken because the client has insufficient rights based on its Authentication state. This NRC is in general supported by each diagnostic service, therefore it is not listed in the list of applicable response codes of the diagnostic services."
	);
	const ServSub servNegRespSubInvavaliKKey (
		static_cast<uint8_t>(ServSubNegResp::invavaliKKey), // invalidKey
		"invalidKey",
		"This NRC indicates that the server has not given security access because the key sent by the client did not match with the key in the server's memory. This counts as an attempt to gain security. The server shall remain in the locked state and increment its internal securityAccessFailed counter as described in 10.4."
	);
	const ServSub servNegRespSubExceedNumOfAttemps (
		static_cast<uint8_t>(ServSubNegResp::exceedNumOfAttemps), // exceedNumberOfAttempts
		"exceedNumberOfAttempts",
		"This NRC indicates that the requested action will not be taken because the client has unsuccessfully attempted to gain security access more times than the server's security strategy will allow."
	);
	const ServSub servNegRespSubRequiredTimeDelayNotExpired (
		static_cast<uint8_t>(ServSubNegResp::requiredTimeDelayNotExpired), // requiredTimeDelayNotExpired
		"requiredTimeDelayNotExpired",
		"This NRC indicates that the requested action will not be taken because the client's latest attempt to gain security access was initiated before the server's required timeout period had elapsed."
	);
	const ServSub servNegRespSubSecureDataTxRequired (
		static_cast<uint8_t>(ServSubNegResp::secureDataTxRequired), // secureDataTransmissionRequired
		"secureDataTransmissionRequired",
		"This NRC indicates that the requested service will not be taken because the requested action is required to be sent using a secured communication channel (i.e. A_MType is equal to secure {remote} diagnostics). This NRC is in general supported by each diagnostic service, therefore it is not listed in the list of applicable response codes of the diagnostic services."
	);
	const ServSub servNegRespSubSecureDataTxNotAllowed (
		static_cast<uint8_t>(ServSubNegResp::secureDataTxNotAllowed), // secureDataTransmissionNotAllowed
		"secureDataTransmissionNotAllowed",
		"This NRC indicates that this message was received using the SecuredDataTransmission (8416) service. However, the requested action is not allowed to be sent using the SecuredDataTransmission (8416) service."
	);
	const ServSub servNegRespSubSecureDataVerifFail (
		static_cast<uint8_t>(ServSubNegResp::secureDataVerifFail), // secureDataVerificationFailed
		"secureDataVerificationFailed",
		"This NRC indicates that the message failed in the security sub-layer. For example, invalid Apar - Administrative parameter or “Signature/Encryption Calculation” parameter, verification error of anti-replay counter or Signature/MAC, decryption or encryption error."
	);
	const ServSub servNegRespSubReserved6 (
		static_cast<uint8_t>(ServSubNegResp::reserved6Start), // ISOSAEReserved
		static_cast<uint8_t>(ServSubNegResp::reserved6End), // ISOSAEReserved
		"ISOSAEReserved",
		"This range of values is reserved by this document for future definition."
	);
	const ServSub servNegRespSubCertifInvalidPeriod (
		static_cast<uint8_t>(ServSubNegResp::certifInvalidPeriod), // Certificate verification failed - Invalid Time Period
		"Certificate verification failed - Invalid Time Period",
		"Date and time of the server does not match the validity period of the Certificate."
	);
	const ServSub servNegRespSubCertifInvalidSig (
		static_cast<uint8_t>(ServSubNegResp::certifInvalidSig), // Certificate verification failed - Invalid Signature
		"Certificate verification failed - Invalid Signature",
		"Signature of the Certificate could not be verified."
	);
	const ServSub servNegRespSubCertifInvalidChainOfTrust (
		static_cast<uint8_t>(ServSubNegResp::certifInvalidChainOfTrust), // Certificate verification failed - Invalid Chain of Trust
		"Certificate verification failed - Invalid Chain of Trust",
		"Certificate could not be verified against stored information about the issuing authority."
	);
	const ServSub servNegRespSubCertifInvalidType (
		static_cast<uint8_t>(ServSubNegResp::certifInvalidType), // Certificate verification failed - Invalid Type
		"Certificate verification failed - Invalid Type",
		"Certificate does not match the current requested use case."
	);
	const ServSub servNegRespSubCertifInvalidFormat (
		static_cast<uint8_t>(ServSubNegResp::certifInvalidFormat), // Certificate verification failed - Invalid Format
		"Certificate verification failed - Invalid Format",
		"Certificate could not be evaluated because the format requirement has not been met."
	);
	const ServSub servNegRespSubCertifInvalidContent (
		static_cast<uint8_t>(ServSubNegResp::certifInvalidContent), // Certificate verification failed - Invalid Content
		"Certificate verification failed - Invalid Content",
		"Certificate could not be verified because the content does not match."
	);
	const ServSub servNegRespSubCertifInvalidScope (
		static_cast<uint8_t>(ServSubNegResp::certifInvalidScope), // Certificate verification failed - Invalid Scope
		"Certificate verification failed - Invalid Scope",
		"The scope of the Certificate does not match the contents of the server."
	);
	const ServSub servNegRespSubCertifInvalidCertif (
		static_cast<uint8_t>(ServSubNegResp::certifInvalidCertif), // Certificate verification failed – Invalid Certificate (revoked)
		"Certificate verification failed – Invalid Certificate (revoked)",
		"Certificate received from client is invalid, because the server has revoked access for some reason."
	);
	const ServSub servNegRespSubOwnerVerifFail (
		static_cast<uint8_t>(ServSubNegResp::ownerVerifFail), // Ownership verification failed
		"Ownership verification failed",
		"Delivered Ownership does not match the provided challenge or could not verified with the own private key."
	);
	const ServSub servNegRespSubChallengeCalcFail (
		static_cast<uint8_t>(ServSubNegResp::challengeCalcFail), // Challenge calculation failed
		"Challenge calculation failed",
		"The challenge could not be calculated on the server side."
	);
	const ServSub servNegRespSubSetAccessRightFail (
		static_cast<uint8_t>(ServSubNegResp::setAccessRightFail), // Setting Access Rights failed
		"Setting Access Rights failed",
		"The server could not set the access rights."
	);
	const ServSub servNegRespSubSessKeyCreateFail (
		static_cast<uint8_t>(ServSubNegResp::sessKeyCreateFail), // Session key creation/derivation failed
		"Session key creation/derivation failed",
		"The server could not create or derive a session key."
	);
	const ServSub servNegRespSubConfDataUsageFail (
		static_cast<uint8_t>(ServSubNegResp::confDataUsageFail), // Configuration data usage failed
		"Configuration data usage failed",
		"The server could not work with the provided configuration data."
	);
	const ServSub servNegRespSubDeAuthFail (
		static_cast<uint8_t>(ServSubNegResp::deAuthFail), // DeAuthentication failed
		"DeAuthentication failed",
		"DeAuthentication was not successful, server could still be unprotected."
	);
	const ServSub servNegRespSubReserved7 (
		static_cast<uint8_t>(ServSubNegResp::reserved7Start), // ISOSAEReserved
		static_cast<uint8_t>(ServSubNegResp::reserved7End), // ISOSAEReserved
		"ISOSAEReserved",
		"This range of values is reserved by this document for future definition."
	);
	const ServSub servNegRespSubUpDownNotAccept (
		static_cast<uint8_t>(ServSubNegResp::upDownNotAccept), // uploadDownloadNotAccepted
		"uploadDownloadNotAccepted",
		"This NRC indicates that an attempt to upload/download to a server's memory cannot be accomplished due to some fault conditions."
	);
	const ServSub servNegRespSubTxDataSuspend (
		static_cast<uint8_t>(ServSubNegResp::txDataSuspend), // transferDataSuspended
		"transferDataSuspended",
		"This NRC indicates that a data transfer operation was halted due to some fault. The active transferData sequence shall be aborted."
	);
	const ServSub servNegRespSubGenProgFail (
		static_cast<uint8_t>(ServSubNegResp::genProgFail), // generalProgrammingFailure
		"generalProgrammingFailure",
		"This NRC indicates that the server detected an error when erasing or programming a memory location in the permanent memory device (e.g. Flash Memory)."
	);
	const ServSub servNegRespSubWrongBlockSeqCounter (
		static_cast<uint8_t>(ServSubNegResp::wrongBlockSeqCounter), // wrongBlockSequenceCounter
		"wrongBlockSequenceCounter",
		"This NRC indicates that the server detected an error in the sequence of blockSequenceCounter values. Note that the repetition of a TransferData request message with a blockSequenceCounter equal to the one included in the previous TransferData request message shall be accepted by the server."
	);
	const ServSub servNegRespSubReserved8 (
		static_cast<uint8_t>(ServSubNegResp::reserved8Start), // ISOSAEReserved
		static_cast<uint8_t>(ServSubNegResp::reserved8End), // ISOSAEReserved
		"ISOSAEReserved",
		"This range of values is reserved by this document for future definition."
	);
	const ServSub servNegRespSubReqCorrectRxRespPending (
		static_cast<uint8_t>(ServSubNegResp::reqCorrectRxRespPending), // requestCorrectlyReceived-ResponsePending
		"requestCorrectlyReceived-ResponsePending",
		"This NRC indicates that the request message was received correctly, and that all parameters in the request message were valid (these checks can be delayed until after sending this NRC if executing the boot software), but the action to be performed is not yet completed and the server is not yet ready to receive another request. As soon as the requested service has been completed, the server shall send a positive response message or negative response message with a response code different from this. The negative response message with this NRC may be repeated by the server until the requested service is completed and the final response message is sent. This NRC might impact the application layer timing parameter values. The detailed specification shall be included in the data link specific implementation document. This NRC shall only be used in a negative response message if the server will not be able to receive further request messages from the client while completing the requested diagnostic service. When this NRC is used, the server shall always send a final response (positive or negative) independent of the suppressPosRspMsgIndicationBit value or the suppress requirement for responses with NRCs SNS, SFNS, SNSIAS, SFNSIAS and ROOR on functionally addressed requests. A typical example where this NRC may be used is when the client has sent a request message, which includes data to be programmed or erased in flash memory of the server. If the programming/erasing routine (usually executed out of RAM) is not able to support serial communication while writing to the flash memory the server shall send a negative response message with this response code. This NRC is in general supported by each diagnostic service, as not otherwise stated in the data link specific implementation document, therefore it is not listed in the list of applicable response codes of the diagnostic services."
	);
	const ServSub servNegRespSubReserved9 (
		static_cast<uint8_t>(ServSubNegResp::reserved9Start), // ISOSAEReserved
		static_cast<uint8_t>(ServSubNegResp::reserved9End), // ISOSAEReserved
		"ISOSAEReserved",
		"This range of values is reserved by this document for future definition."
	);
	const ServSub servNegRespSubSubFuncNotActInCurrSess (
		static_cast<uint8_t>(ServSubNegResp::subFuncNotActInCurrSess), // SubFunctionNotSupportedInActiveSession
		"SubFunctionNotSupportedInActiveSession",
		"This NRC indicates that the requested action will not be taken because the server does not support the requested SubFunction in the session currently active. This NRC shall only be used when the requested SubFunction is known to be supported in another session, otherwise response code SFNS (SubFunctionNotSupported) shall be used (e.g. servers executing the boot software generally do not know which SubFunctions are supported in the application (and vice versa) and therefore may need to respond with NRC 1216 instead). This NRC shall be supported by each diagnostic service with a SubFunction parameter, if not otherwise stated in the data link specific implementation document, therefore it is not listed in the list of applicable response codes of the diagnostic services."
	);
	const ServSub servNegRespSubServNotActInCurrSess (
		static_cast<uint8_t>(ServSubNegResp::servNotActInCurrSess), // serviceNotSupportedInActiveSession
		"serviceNotSupportedInActiveSession",
		"This NRC indicates that the requested action will not be taken because the server does not support the requested service in the session currently active. This NRC shall only be used when the requested service is known to be supported in another session, otherwise response code SNS (serviceNotSupported) shall be used (e.g. servers executing the boot software generally do not know which services are supported in the application (and vice versa) and therefore may need to respond with NRC 1116 instead). This NRC is in general supported by each diagnostic service, as not otherwise stated in the data link specific implementation document, therefore it is not listed in the list of applicable response codes of the diagnostic services."
	);
	const ServSub servNegRespSubReserved10 (
		static_cast<uint8_t>(ServSubNegResp::reserved10), // ISOSAEReserved
		"ISOSAEReserved",
		"This range of values is reserved by this document for future definition."
	);
	const ServSub servNegRespSubRpmTooHigh (
		static_cast<uint8_t>(ServSubNegResp::rpmTooHigh), // rpmTooHigh
		"rpmTooHigh",
		"This NRC indicates that the requested action will not be taken because the server prerequisite condition for RPM is not met (current RPM is above a preprogrammed maximum threshold)."
	);
	const ServSub servNegRespSubRpmTooLow (
		static_cast<uint8_t>(ServSubNegResp::rpmTooLow), // rpmTooLow
		"rpmTooLow",
		"This NRC indicates that the requested action will not be taken because the server prerequisite condition for RPM is not met (current RPM is below a preprogrammed minimum threshold)."
	);
	const ServSub servNegRespSubEngineRunning (
		static_cast<uint8_t>(ServSubNegResp::engineRunning), // engineIsRunning
		"engineIsRunning",
		"This NRC is required for those actuator tests which cannot be actuated while the Engine is running. This is different from RPM too high negative response, and shall be allowed."
	);
	const ServSub servNegRespSubEngineNotRunning (
		static_cast<uint8_t>(ServSubNegResp::engineNotRunning), // engineIsNotRunning
		"engineIsNotRunning",
		"This NRC is required for those actuator tests which cannot be actuated unless the Engine is running. This is different from RPM too low negative response, and shall be allowed."
	);
	const ServSub servNegRespSubEngineRunTimeLow (
		static_cast<uint8_t>(ServSubNegResp::engineRunTimeLow), // engineRunTimeTooLow
		"engineRunTimeTooLow",
		"This NRC indicates that the requested action will not be taken because the server prerequisite condition for engine run time is not met (current engine run time is below a preprogrammed limit)."
	);
	const ServSub servNegRespSubTempTooHigh (
		static_cast<uint8_t>(ServSubNegResp::tempTooHigh), // temperatureTooHigh
		"temperatureTooHigh",
		"This NRC indicates that the requested action will not be taken because the server prerequisite condition for temperature is not met (current temperature is above a preprogrammed maximum threshold)."
	);
	const ServSub servNegRespSubTempTooLow (
		static_cast<uint8_t>(ServSubNegResp::tempTooLow), // temperatureTooLow
		"temperatureTooLow",
		"This NRC indicates that the requested action will not be taken because the server prerequisite condition for temperature is not met (current temperature is below a preprogrammed minimum threshold)."
	);
	const ServSub servNegRespSubVehicleSpeedTooHigh (
		static_cast<uint8_t>(ServSubNegResp::vehicleSpeedTooHigh), // vehicleSpeedTooHigh
		"vehicleSpeedTooHigh",
		"This NRC indicates that the requested action will not be taken because the server prerequisite condition for vehicle speed is not met (current VS is above a preprogrammed maximum threshold)."
	);
	const ServSub servNegRespSubVehicleSpeedTooLow (
		static_cast<uint8_t>(ServSubNegResp::vehicleSpeedTooLow), // vehicleSpeedTooLow
		"vehicleSpeedTooLow",
		"This NRC indicates that the requested action will not be taken because the server prerequisite condition for vehicle speed is not met (current VS is below a preprogrammed minimum threshold)."
	);
	const ServSub servNegRespSubThrottleTooHigh (
		static_cast<uint8_t>(ServSubNegResp::throttleTooHigh), // throttle/PedalTooHigh
		"throttle/PedalTooHigh",
		"This NRC indicates that the requested action will not be taken because the server prerequisite condition for throttle/pedal position is not met (current TP/APP is above a preprogrammed maximum threshold)."
	);
	const ServSub servNegRespSubThrottleTooLow (
		static_cast<uint8_t>(ServSubNegResp::throttleTooLow), // throttle/PedalTooLow
		"throttle/PedalTooLow",
		"This NRC indicates that the requested action will not be taken because the server prerequisite condition for throttle/pedal position is not met (current TP/APP is below a preprogrammed minimum threshold)."
	);
	const ServSub servNegRespSubTransmissionNotInNeutral (
		static_cast<uint8_t>(ServSubNegResp::transmissionNotInNeutral), // transmissionRangeNotInNeutral
		"transmissionRangeNotInNeutral",
		"This NRC indicates that the requested action will not be taken because the server prerequisite condition for being in neutral is not met (current transmission range is not in neutral)."
	);
	const ServSub servNegRespSubTransmissionNotInGear (
		static_cast<uint8_t>(ServSubNegResp::transmissionNotInGear), // transmissionRangeNotInGear
		"transmissionRangeNotInGear",
		"This NRC indicates that the requested action will not be taken because the server prerequisite condition for being in gear is not met (current transmission range is not in gear)."
	);
	const ServSub servNegRespSubReserved11 (
		static_cast<uint8_t>(ServSubNegResp::reserved11), // ISOSAEReserved
		"ISOSAEReserved",
		"This range of values is reserved by this document for future definition."
	);
	const ServSub servNegRespSubBrakeNotApplied (
		static_cast<uint8_t>(ServSubNegResp::brakeNotApplied), // brakeSwitch(es)NotClosed (Brake Pedal not pressed or not applied)
		"brakeSwitch(es)NotClosed (Brake Pedal not pressed or not applied)",
		"This NRC indicates that for safety reasons, this is required for certain tests before it begins, and shall be maintained for the entire duration of the test."
	);
	const ServSub servNegRespSubShifterLevelNotInPark (
		static_cast<uint8_t>(ServSubNegResp::shifterLevelNotInPark), // shifterLeverNotInPark
		"shifterLeverNotInPark",
		"This NRC indicates that for safety reasons, this is required for certain tests before it begins, and shall be maintained for the entire duration of the test."
	);
	const ServSub servNegRespSubTorqConvClutchLocked (
		static_cast<uint8_t>(ServSubNegResp::torqConvClutchLocked), // torqueConverterClutchLocked
		"torqueConverterClutchLocked",
		"This NRC indicates that the requested action will not be taken because the server prerequisite condition for torque converter clutch is not met (current TCC status above a preprogrammed limit or locked)."
	);
	const ServSub servNegRespSubVoltageTooHigh (
		static_cast<uint8_t>(ServSubNegResp::voltageTooHigh), // voltageTooHigh
		"voltageTooHigh",
		"This NRC indicates that the requested action will not be taken because the server prerequisite condition for voltage at the primary pin of the server (ECU) is not met (current voltage is above a preprogrammed maximum threshold)."
	);
	const ServSub servNegRespSubVoltageTooLow (
		static_cast<uint8_t>(ServSubNegResp::voltageTooLow), // voltageTooLow
		"voltageTooLow",
		"This NRC indicates that the requested action will not be taken because the server prerequisite condition for voltage at the primary pin of the server (ECU) is not met (current voltage is below a preprogrammed minimum threshold)."
	);
	const ServSub servNegRespSubResourceTempNotAvail (
		static_cast<uint8_t>(ServSubNegResp::resourceTempNotAvail), // ResourceTemporarilyNotAvailable
		"ResourceTemporarilyNotAvailable",
		"This NRC indicates that the server has received the request but the requested action could not be performed by the server because an application which is necessary to supply the requested information is temporality not available. This NRC is in general supported by each diagnostic service, as not otherwise stated in the data link specific implementation document, therefore it is not listed in the list of applicable response codes of the diagnostic services."
	);
	const ServSub servNegRespSubReservedForSpecificCondsNotCorrect (
		static_cast<uint8_t>(ServSubNegResp::reservedForSpecificCondsNotCorrectStart), // reservedForSpecificConditionsNotCorrect
		static_cast<uint8_t>(ServSubNegResp::reservedForSpecificCondsNotCorrectEnd), // reservedForSpecificConditionsNotCorrect
		"reservedForSpecificConditionsNotCorrect",
		"This range of values is reserved by this document for future definition."
	);
	const ServSub servNegRespSubVehicleMfrSpecificCondsNotCorrect (
		static_cast<uint8_t>(ServSubNegResp::vehicleMfrSpecificCondsNotCorrectStart), // vehicleManufacturerSpecificConditionsNotCorrect
		static_cast<uint8_t>(ServSubNegResp::vehicleMfrSpecificCondsNotCorrectEnd), // vehicleManufacturerSpecificConditionsNotCorrect
		"vehicleManufacturerSpecificConditionsNotCorrect",
		"This range of values is reserved for vehicle manufacturer specific condition not correct scenarios."
	);
	const ServSub servNegRespSubReserved12 (
		static_cast<uint8_t>(ServSubNegResp::reserved12), // ISOSAEReserved
		"ISOSAEReserved",
		"This range of values is reserved by this document for future definition."
	);



	const QVector<const ServSub *> servDiagSessCtrlSub = {
		&servDiagSessCtrlSubReserved1,
		&servDiagSessCtrlSubDef,
		&servDiagSessCtrlSubProg,
		&servDiagSessCtrlSubExtdDiag,
		&servDiagSessCtrlSubSafetySysDiag,
		&servDiagSessCtrlSubReserved2,
		&servDiagSessCtrlSubVehicleManufacturerSpecific,
		&servDiagSessCtrlSubSysSupplierSpecific,
		&servDiagSessCtrlSubReserved3,

	};

	const QVector<const ServSub *> servEcuResetSub = {
		&servEcuResetSubReserved1,
		&servEcuResetSubHardReset,
		&servEcuResetSubKeyOffOnReset,
		&servEcuResetSubSoftReset,
		&servEcuResetSubEnRapidPwrShutDown,
		&servEcuResetSubDisableRapidPwrShutdown,
		&servEcuResetSubReserved2,
		&servEcuResetSubVehicleManufacturerSpecific,
		&servEcuResetSubSysSuppSpecific,
		&servEcuResetSubReserved3,

	};

	const QVector<const ServSub *> servSecurityAccessSub = {
		&servSecurityAccessSubReserved1,
		&servSecurityAccessSubReqSeedDef,
		&servSecurityAccessSubSendKeyDef,
		&servSecurityAccessSubReqSeedLevel,
		&servSecurityAccessSubReqSeedLevel1,
		&servSecurityAccessSubReqSeedLevel2,
		&servSecurityAccessSubSendKeyLevel,
		&servSecurityAccessSubSendKeyLevel1,
		&servSecurityAccessSubSendKeyLevel2,
		&servSecurityAccessSubReserved2,
		&servSecurityAccessSubIso26021ReqSeed,
		&servSecurityAccessSubIso26021SendKey,
		&servSecurityAccessSubSysSuppSpecific,
		&servSecurityAccessSubReserved3,

	};

	const QVector<const ServSub *> servCommCtrlSub = {
		&servCommCtrlSubEnRxAndTx,
		&servCommCtrlSubEnRxAndDisTx,
		&servCommCtrlSubDisRxAndEnTx,
		&servCommCtrlSubDisRxAndTx,
		&servCommCtrlSubEnRxAndDisTxWEnchancedAddrInfo,
		&servCommCtrlSubEnRxAndTxWEnhancedAddrInfo,
		&servCommCtrlSubReserved1,
		&servCommCtrlSubVehicleMfrSpecific,
		&servCommCtrlSubSysSuppSpecific,
		&servCommCtrlSubReserved2,

	};

	const QVector<const ServSub *> servTesterPresentSub = {
		&servTesterPresentSubNone,
		&servTesterPresentSubReserved1,

	};

	const QVector<const ServSub *> servAuthSub = {
	};

	const QVector<const ServSub *> servSecuredDataTxSub = {
	};

	const QVector<const ServSub *> servCtrlDtcSettingsSub = {
	};

	const QVector<const ServSub *> servRespOnEventSub = {
	};

	const QVector<const ServSub *> servLinkCtrlSub = {
	};

	const QVector<const ServSub *> servReadDataByIdSub = {
	};

	const QVector<const ServSub *> servReadMemByAddrSub = {
	};

	const QVector<const ServSub *> servReadScalingDataByIdSub = {
	};

	const QVector<const ServSub *> servReadDataByPeriodicIdSub = {
	};

	const QVector<const ServSub *> servDynDefDataIdSub = {
	};

	const QVector<const ServSub *> servWriteDataByIdSub = {
	};

	const QVector<const ServSub *> servWriteMemByAddrSub = {
	};

	const QVector<const ServSub *> servClearDiagInfoSub = {
	};

	const QVector<const ServSub *> servReadDtcInfoSub = {
		&servReadDtcInfoSubReserved1,
		&servReadDtcInfoSubReportNumOfDtcByStatusMask,
		&servReadDtcInfoSubReportDtcByStatusMask,
		&servReadDtcInfoSubReportDtcSnapshotId,
		&servReadDtcInfoSubReportDtcSnapshotRecordByDtcNum,
		&servReadDtcInfoSubReportDtcStoredDataByRecordNum,
		&servReadDtcInfoSubReportDtcExtDataRecordByDtcNum,
		&servReadDtcInfoSubReportNumOfDtcBySeverityMaskRecord,
		&servReadDtcInfoSubReportDtcBySeverityMaskRecord,
		&servReadDtcInfoSubReportSeverityInformationOfDtc,
		&servReadDtcInfoSubReportSuppDtc,
		&servReadDtcInfoSubReportFirstTestFailedDtc,
		&servReadDtcInfoSubReportFirstConfirmedDtc,
		&servReadDtcInfoSubReportMostRecentTestFailedDtc,
		&servReadDtcInfoSubReportMostRecentConfirmedDtc,
		&servReadDtcInfoSubReportDtcFaultDetectionCounter,
		&servReadDtcInfoSubReportDtcWithPermanentStatus,
		&servReadDtcInfoSubReportDtcExtDataRecordByRecordNumber,
		&servReadDtcInfoSubReportUserDefMemDtcByStatusMask,
		&servReadDtcInfoSubReportUserDefMemDtcSnapshotRecordByDtcNum,
		&servReadDtcInfoSubReportUserDefMemDtcExtDataRecordByDtcNum,
		&servReadDtcInfoSubReportDtcExtendedDataRecordId,
		&servReadDtcInfoSubReserved2,
		&servReadDtcInfoSubReportWWHOBDDTCByMaskRecord,
		&servReadDtcInfoSubReserved3,
		&servReadDtcInfoSubReportWWHOBDDTCWithPermanentStatus,
		&servReadDtcInfoSubReportDtcInfoByDtcReadinessGroupId,
		&servReadDtcInfoSubReserved4,

	};

	const QVector<const ServSub *> servIoCtrlByIdSub = {
	};

	const QVector<const ServSub *> servRoutineCtrlSub = {
		&servRoutineCtrlSubReserved1,
		&servRoutineCtrlSubStartRoutine,
		&servRoutineCtrlSubStopRoutine,
		&servRoutineCtrlSubRequestRoutineResults,
		&servRoutineCtrlSubReserved2,

	};

	const QVector<const ServSub *> servRoutineDownloadSub = {
	};

	const QVector<const ServSub *> servReqUploadSub = {
	};

	const QVector<const ServSub *> servTransferDataSub = {
	};

	const QVector<const ServSub *> servReqTransferExitSub = {
	};

	const QVector<const ServSub *> servReqFileTransferSub = {
	};

	const QVector<const ServSub *> servNegRespSub = {
		&servNegRespSubPosResp,
		&servNegRespSubReserved1,
		&servNegRespSubGenReject,
		&servNegRespSubServNotSupp,
		&servNegRespSubSubFuncNotSupp,
		&servNegRespSubIncorrectMsg,
		&servNegRespSubRespTooLong,
		&servNegRespSubReserved2,
		&servNegRespSubBusyRepeat,
		&servNegRespSubCondNotCorrect,
		&servNegRespSubReserved3,
		&servNegRespSubReqSeqErr,
		&servNegRespSubNoRespFromSubComp,
		&servNegRespSubFailPreventsExec,
		&servNegRespSubReserved4,
		&servNegRespSubReqOutOfRange,
		&servNegRespSubReserved5,
		&servNegRespSubSecurityAccessDenied,
		&servNegRespSubAuthRequried,
		&servNegRespSubInvavaliKKey,
		&servNegRespSubExceedNumOfAttemps,
		&servNegRespSubRequiredTimeDelayNotExpired,
		&servNegRespSubSecureDataTxRequired,
		&servNegRespSubSecureDataTxNotAllowed,
		&servNegRespSubSecureDataVerifFail,
		&servNegRespSubReserved6,
		&servNegRespSubCertifInvalidPeriod,
		&servNegRespSubCertifInvalidSig,
		&servNegRespSubCertifInvalidChainOfTrust,
		&servNegRespSubCertifInvalidType,
		&servNegRespSubCertifInvalidFormat,
		&servNegRespSubCertifInvalidContent,
		&servNegRespSubCertifInvalidScope,
		&servNegRespSubCertifInvalidCertif,
		&servNegRespSubOwnerVerifFail,
		&servNegRespSubChallengeCalcFail,
		&servNegRespSubSetAccessRightFail,
		&servNegRespSubSessKeyCreateFail,
		&servNegRespSubConfDataUsageFail,
		&servNegRespSubDeAuthFail,
		&servNegRespSubReserved7,
		&servNegRespSubUpDownNotAccept,
		&servNegRespSubTxDataSuspend,
		&servNegRespSubGenProgFail,
		&servNegRespSubWrongBlockSeqCounter,
		&servNegRespSubReserved8,
		&servNegRespSubReqCorrectRxRespPending,
		&servNegRespSubReserved9,
		&servNegRespSubSubFuncNotActInCurrSess,
		&servNegRespSubServNotActInCurrSess,
		&servNegRespSubReserved10,
		&servNegRespSubRpmTooHigh,
		&servNegRespSubRpmTooLow,
		&servNegRespSubEngineRunning,
		&servNegRespSubEngineNotRunning,
		&servNegRespSubEngineRunTimeLow,
		&servNegRespSubTempTooHigh,
		&servNegRespSubTempTooLow,
		&servNegRespSubVehicleSpeedTooHigh,
		&servNegRespSubVehicleSpeedTooLow,
		&servNegRespSubThrottleTooHigh,
		&servNegRespSubThrottleTooLow,
		&servNegRespSubTransmissionNotInNeutral,
		&servNegRespSubTransmissionNotInGear,
		&servNegRespSubReserved11,
		&servNegRespSubBrakeNotApplied,
		&servNegRespSubShifterLevelNotInPark,
		&servNegRespSubTorqConvClutchLocked,
		&servNegRespSubVoltageTooHigh,
		&servNegRespSubVoltageTooLow,
		&servNegRespSubResourceTempNotAvail,
		&servNegRespSubReservedForSpecificCondsNotCorrect,
		&servNegRespSubVehicleMfrSpecificCondsNotCorrect,
		&servNegRespSubReserved12,

	};



	const Serv servDiagSessCtrl(ServEnum::diagSessCtrl, "DiagnosticSessionControl", &servDiagSessCtrlSub);
	const Serv servEcuReset(ServEnum::ecuReset, "ECUReset", &servEcuResetSub);
	const Serv servSecurityAccess(ServEnum::securityAccess, "SecurityAccess", &servSecurityAccessSub);
	const Serv servCommCtrl(ServEnum::commCtrl, "CommunicationControl", &servCommCtrlSub);
	const Serv servTesterPresent(ServEnum::testerPresent, "TesterPresent", &servTesterPresentSub);
	const Serv servAuth(ServEnum::auth, "Authentication", &servAuthSub);
	const Serv servSecuredDataTx(ServEnum::securedDataTx, "SecuredDataTransmission", &servSecuredDataTxSub);
	const Serv servCtrlDtcSettings(ServEnum::ctrlDtcSettings, "ControlDTCSetting", &servCtrlDtcSettingsSub);
	const Serv servRespOnEvent(ServEnum::respOnEvent, "ResponseOnEvent", &servRespOnEventSub);
	const Serv servLinkCtrl(ServEnum::linkCtrl, "LinkControl", &servLinkCtrlSub);
	const Serv servReadDataById(ServEnum::readDataById, "ReadDataByIdentifier", &servReadDataByIdSub);
	const Serv servReadMemByAddr(ServEnum::readMemByAddr, "ReadMemoryByAddress", &servReadMemByAddrSub);
	const Serv servReadScalingDataById(ServEnum::readScalingDataById, "ReadScalingDataByIdentifier", &servReadScalingDataByIdSub);
	const Serv servReadDataByPeriodicId(ServEnum::readDataByPeriodicId, "ReadDataByPeriodicIdentifier", &servReadDataByPeriodicIdSub);
	const Serv servDynDefDataId(ServEnum::dynDefDataId, "DynamicallyDefineDataIdentifier", &servDynDefDataIdSub);
	const Serv servWriteDataById(ServEnum::writeDataById, "WriteDataByIdentifier", &servWriteDataByIdSub);
	const Serv servWriteMemByAddr(ServEnum::writeMemByAddr, "WriteMemoryByAddress", &servWriteMemByAddrSub);
	const Serv servClearDiagInfo(ServEnum::clearDiagInfo, "ClearDiagnosticInformation", &servClearDiagInfoSub);
	const Serv servReadDtcInfo(ServEnum::readDtcInfo, "ReadDTCInformation", &servReadDtcInfoSub);
	const Serv servIoCtrlById(ServEnum::ioCtrlById, "InputOutputControlByIdentifier", &servIoCtrlByIdSub);
	const Serv servRoutineCtrl(ServEnum::routineCtrl, "RoutineControl", &servRoutineCtrlSub);
	const Serv servRoutineDownload(ServEnum::routineDownload, "RequestDownload", &servRoutineDownloadSub);
	const Serv servReqUpload(ServEnum::reqUpload, "RequestUpload", &servReqUploadSub);
	const Serv servTransferData(ServEnum::transferData, "TransferData", &servTransferDataSub);
	const Serv servReqTransferExit(ServEnum::reqTransferExit, "RequestTransferExit", &servReqTransferExitSub);
	const Serv servReqFileTransfer(ServEnum::reqFileTransfer, "RequestFileTransfer", &servReqFileTransferSub);
	const Serv servNegResp(ServEnum::negResp, "NegativeResponce", &servNegRespSub);

	const QMap<ServEnum, const Serv *> serv = {
		{ServEnum::diagSessCtrl, &servDiagSessCtrl},
		{ServEnum::ecuReset, &servEcuReset},
		{ServEnum::securityAccess, &servSecurityAccess},
		{ServEnum::commCtrl, &servCommCtrl},
		{ServEnum::testerPresent, &servTesterPresent},
		{ServEnum::auth, &servAuth},
		{ServEnum::securedDataTx, &servSecuredDataTx},
		{ServEnum::ctrlDtcSettings, &servCtrlDtcSettings},
		{ServEnum::respOnEvent, &servRespOnEvent},
		{ServEnum::linkCtrl, &servLinkCtrl},
		{ServEnum::readDataById, &servReadDataById},
		{ServEnum::readMemByAddr, &servReadMemByAddr},
		{ServEnum::readScalingDataById, &servReadScalingDataById},
		{ServEnum::readDataByPeriodicId, &servReadDataByPeriodicId},
		{ServEnum::dynDefDataId, &servDynDefDataId},
		{ServEnum::writeDataById, &servWriteDataById},
		{ServEnum::writeMemByAddr, &servWriteMemByAddr},
		{ServEnum::clearDiagInfo, &servClearDiagInfo},
		{ServEnum::readDtcInfo, &servReadDtcInfo},
		{ServEnum::ioCtrlById, &servIoCtrlById},
		{ServEnum::routineCtrl, &servRoutineCtrl},
		{ServEnum::routineDownload, &servRoutineDownload},
		{ServEnum::reqUpload, &servReqUpload},
		{ServEnum::transferData, &servTransferData},
		{ServEnum::reqTransferExit, &servReqTransferExit},
		{ServEnum::reqFileTransfer, &servReqFileTransfer},
		{ServEnum::negResp, &servNegResp},
	};
}