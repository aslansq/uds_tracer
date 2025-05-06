#include <QVector>
#include "peakbasiccan.h"
#include "util.h"

PeakBasicCan::PeakBasicCan(void) :
	statusStrings({
		{PCAN_ERROR_OK, "No error"},
		{PCAN_ERROR_XMTFULL, "Transmit buffer in CAN controller is full"},
		{PCAN_ERROR_OVERRUN, "CAN controller was read too late"},
		{PCAN_ERROR_BUSLIGHT, "Bus error: error counter reached the 'light' limit"},
		{PCAN_ERROR_BUSHEAVY, "Bus error: error counter reached the 'heavy' limit"},
		{PCAN_ERROR_BUSWARNING, "Bus error: error counter reached the 'warning' limit"},
		{PCAN_ERROR_BUSPASSIVE, "Bus error: CAN controller is error passive"},
		{PCAN_ERROR_BUSOFF, "Bus error: CAN controller is in bus-off state"},
		{PCAN_ERROR_ANYBUSERR, "Bus error: any bus error occurred"},
		{PCAN_ERROR_QRCVEMPTY, "Receive queue is empty"},
		{PCAN_ERROR_QOVERRUN, "Receive queue was read too late"},
		{PCAN_ERROR_QXMTFULL, "Transmit queue is full"},
		{PCAN_ERROR_REGTEST, "Hardware registers test failed"},
		{PCAN_ERROR_NODRIVER, "Driver not loaded"},
		{PCAN_ERROR_HWINUSE, "Hardware is already in use"},
		{PCAN_ERROR_NETINUSE, "Network resource is already in use"},
		{PCAN_ERROR_ILLHW, "Invalid hardware handle"},
		{PCAN_ERROR_ILLNET, "Invalid network handle"},
		{PCAN_ERROR_ILLCLIENT, "Invalid client handle"},
		{PCAN_ERROR_ILLHANDLE, "Invalid handle (hardware, network, or client)"},
		{PCAN_ERROR_RESOURCE, "Resource cannot be created"},
		{PCAN_ERROR_ILLPARAMTYPE, "Invalid parameter type"},
		{PCAN_ERROR_ILLPARAMVAL, "Invalid parameter value"},
		{PCAN_ERROR_UNKNOWN, "Unknown error"},
		{PCAN_ERROR_ILLDATA, "Invalid data, function, or action"},
		{PCAN_ERROR_ILLMODE, "Driver object state is wrong for the attempted operation"},
		{PCAN_ERROR_CAUTION, "Operation succeeded with irregularities"},
		{PCAN_ERROR_INITIALIZE, "Channel is not initialized"},
		{PCAN_ERROR_ILLOPERATION, "Invalid operation"}
	})
{

}

QString PeakBasicCan::getStatusStr(TPCANStatus st)
{
	QString retStr = "";
	if(st == PCAN_ERROR_OK) {
		retStr = statusStrings[PCAN_ERROR_OK];
	} else {
		for(TPCANStatus key : statusStrings.keys()) {
			QString value = statusStrings[key];
			if((st & key) != 0) {
				retStr += value + "\n";
			}
		}
	}

	return retStr;
}


TPCANHandle PeakBasicCan::getPeakHandleId(int devNum)
{
	QVector<TPCANHandle> handleVector = {
		this->invalidPcanHandle, // dummy
		PCAN_USBBUS1,
		PCAN_USBBUS2,
		PCAN_USBBUS3,
		PCAN_USBBUS4,
		PCAN_USBBUS5,
		PCAN_USBBUS6,
		PCAN_USBBUS7,
		PCAN_USBBUS8,
		PCAN_USBBUS9,
		PCAN_USBBUS10,
		PCAN_USBBUS11,
		PCAN_USBBUS12,
		PCAN_USBBUS13,
		PCAN_USBBUS14,
		PCAN_USBBUS15,
		PCAN_USBBUS16,
	};

	if(devNum >= handleVector.length()) {
		return this->invalidPcanHandle;
	}

	return handleVector[devNum];
}

TPCANHandle PeakBasicCan::getPeakHandleId(QString devStr)
{
	int deviceNum;
	QString deviceNumStr;
	if(devStr.length() <= 2) {
		Util::log(LogType::GenericThrow, LogSt::Nok, devStr + " does not have 2 chars");
		return this->invalidPcanHandle;
	}
	deviceNumStr = devStr.right(2);
	deviceNum = deviceNumStr.toInt() - 31;

	return getPeakHandleId(deviceNum);
}
