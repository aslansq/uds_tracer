#ifndef __ISOTP_TYPES__HPP__
#define __ISOTP_TYPES__HPP__

#include "isotp_config.hpp"

/**************************************************************
 * compiler specific defines
 *************************************************************/
#ifdef __GNUC__
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define ISOTP_BYTE_ORDER_LITTLE_ENDIAN
#else 
#error "unsupported byte ordering.\n" \
	   "I care about arm MCU and intex x86 pc which is little endian.\n" \
	   "If it is important for you port it"
#endif // __BYTE_ORDER__
#endif //__GNUC__

/**************************************************************
 * OS specific defines
 *************************************************************/
// dont care about Windows

/**************************************************************
 * internal used defines
 *************************************************************/
enum class IsoTpRet {
	OK         = 0,
	ISO_ERROR  = -1,
	INPROGRESS = -2,
	ISO_OVERFLOW   = -3,
	WRONG_SN   = -4,
	ISO_NO_DATA    = -5,
	TIMEOUT    = -6,
	LENGTH     = -7
};

/* Return true if 'a' is after 'b' */
constexpr bool IsoTpTimeAfter(int32_t a, int32_t b) {
	return ((b - a) < 0);
}

class IsoTpGeneral {
public:
	static const uint16_t INVALID_BS = 0xFFFF;
};

/* ISOTP sender status */
enum class IsoTpSendStatus {
	IDLE,
	INPROGRESS,
	ISO_ERROR
};

/* ISOTP receiver status */
enum class IsoTpReceiveStatus {
	IDLE,
	INPROGRESS,
	FULL
};

/* can fram defination */
#if defined(ISOTP_BYTE_ORDER_LITTLE_ENDIAN)
typedef struct {
	uint8_t reserve_1:4;
	uint8_t type:4;
	uint8_t reserve_2[7];
} IsoTpPciType;

typedef struct {
	uint8_t SF_DL:4;
	uint8_t type:4;
	uint8_t data[7];
} IsoTpSingleFrame;

typedef struct {
	uint8_t FF_DL_high:4;
	uint8_t type:4;
	uint8_t FF_DL_low;
	uint8_t data[6];
} IsoTpFirstFrame;

typedef struct {
	uint8_t SN:4;
	uint8_t type:4;
	uint8_t data[7];
} IsoTpConsecutiveFrame;

typedef struct {
	uint8_t FS:4;
	uint8_t type:4;
	uint8_t BS;
	uint8_t STmin;
	uint8_t reserve[5];
} IsoTpFlowControl;

#else
#error "Unsupported byte ordering"
#endif

typedef struct {
	uint8_t ptr[8];
} IsoTpDataArray;

typedef struct {
	union {
		IsoTpPciType          common;
		IsoTpSingleFrame      single_frame;
		IsoTpFirstFrame       first_frame;
		IsoTpConsecutiveFrame consecutive_frame;
		IsoTpFlowControl      flow_control;
		IsoTpDataArray        data_array;
	} as;
} IsoTpCanMessage;

/**************************************************************
 * protocol specific defines
 *************************************************************/

/* Private: Protocol Control Information (PCI) types, for identifying each frame of an ISO-TP message.
 */

enum class IsoTpProtocolControlInformation {
	SINGLE             = 0,
	FIRST_FRAME        = 1,
	CONSECUTIVE_FRAME  = 2,
	FLOW_CONTROL_FRAME = 3
};

/* Private: Protocol Control Information (PCI) flow control identifiers.
 */
enum class IsoTpFlowStatus {
	CONTINUE = 0,
	WAIT     = 1,
	ISO_OVERFLOW = 2
};

/* Private: network layer resault code.
 */
enum class IsoTpProtocolResult {
	OK           =  0,
	TIMEOUT_A    = -1,
	TIMEOUT_BS   = -2,
	TIMEOUT_CR   = -3,
	WRONG_SN     = -4,
	INVALID_FS   = -5,
	UNEXP_PDU    = -6,
	WFT_OVRN     = -7,
	BUFFER_OVFLW = -8,
	ISO_ERROR    = -9
};

#endif // __ISOTP_TYPES__HPP__

