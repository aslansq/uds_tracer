#ifndef __ISOTP_CONFIG__HPP__
#define __ISOTP_CONFIG__HPP__

#include <cstdint>

class IsoTpConfig{
public:
	/* Max number of messages the receiver can receive at one time, this value 
 	* is affectied by can driver queue length
	*/
	static const uint8_t DEFAULT_BLOCK_SIZE = 8;
	/* The STmin parameter value specifies the minimum time gap allowed between 
	* the transmission of consecutive frame network protocol data units
 	*/
	static const uint8_t DEFAULT_ST_MIN = 0;
	/* This parameter indicate how many FC N_PDU WTs can be transmitted by the 
	* receiver in a row.
	*/
	static const uint8_t MAX_WFT_NUMBER = 1;
	/* Private: The default timeout to use when waiting for a response during a
	* multi-frame send or receive.
	*/
	static const uint32_t DEFAULT_RESPONSE_TIMEOUT = 100;
};

/* Private: Determines if by default, padding is added to ISO-TP message frames.
 */
//#define ISO_TP_FRAME_PADDING

#endif // __ISOTP_CONFIG__HPP__