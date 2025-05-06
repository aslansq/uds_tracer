#ifndef UDS_H
#define UDS_H

#include <QObject>

class UdsInfo
{
public:
	UdsInfo();
	QString name;
	QString detail;
	QVector<uint8_t> hex;
	uint32_t hexIdx;

	void clear(void);
	QString getHexStr(void) const;
	QString getHexStr(int numOfBytes) const;
};

class Uds : public QObject
{
	Q_OBJECT
public:
	explicit Uds(QObject *parent = nullptr);
	void getReqInfo(const QVector<uint8_t> &packetRef, QVector<UdsInfo> &packetInfoRef);
	void getRespInfo(const QVector<uint8_t> &packetRef, QVector<UdsInfo> &packetInfoRef);
private:
	QVector<uint8_t> reqPacket;
	QVector<UdsInfo> reqPacketInfo;
	QVector<uint8_t> respPacket;
	QVector<UdsInfo> respPacketInfo;

	void getServSubInfo();
	void getDiagSessCtrl();
	void getEcuReset();
	void getSecAcc();
	void getCommCtrl();
	void getTesterPresent();
	void getAuth();
	void getSecDataTx();
	void getCtrlDtcSettings();
	void getRespOnEvent();
	void getLinkCtrl();
	void getReadDataById();
	void getReadMemByAddr();
	void getReadScalingDataById();
	void getReadDataByPeriodicId();
	void getDynDefDataId();
	void getWriteDataById();
	void getWriteMemByAddr();
	void getClearDiagInfo();
	void getReadDtcInfo();
	void getIoCtrlById();
	void getRoutineCtrl();
	void getRoutineDownload();
	void getReqUpload();
	void getTransferData();
	void getReqTransferExit();
	void getReqFileTransfer();

	void getNegResp();
	void getDiagSessCtrlResp();
	void getEcuResetResp();
	void getSecAccResp();
	void getRoutineCtrlResp();
	void getWriteDataByIdResp();
	void getTransferDataResp();
signals:

};

#endif // UDS_H
