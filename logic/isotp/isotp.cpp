#include "isotp.hpp"
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <chrono>
#include <cassert>

///////////////////////////////////////////////////////
///                 PRIVATE FUNCTIONS               ///
///////////////////////////////////////////////////////

uint8_t IsoTp::ms_to_st_min(uint8_t ms)
{
	uint8_t st_min;

	st_min = ms;
	if (st_min > 0x7F) {
		st_min = 0x7F;
	}
	
	return st_min;
}
uint8_t IsoTp::st_min_to_ms(uint8_t st_min)
{
	uint8_t ms;
	
	if (st_min >= 0xF1 && st_min <= 0xF9) {
		ms = 1;
	} else if (st_min <= 0x7F) {
		ms = st_min;
	} else {
		ms = 0;
	}
	
	return ms;
}
IsoTpRet IsoTp::send_flow_control(
	IsoTpFlowStatus flow_status,
	uint8_t block_size,
	uint8_t st_min_ms
)
{
	IsoTpCanMessage message;
	IsoTpRet ret;

	/* setup message  */
	message.as.flow_control.type = static_cast<uint8_t>(IsoTpProtocolControlInformation::FLOW_CONTROL_FRAME);
	message.as.flow_control.FS = static_cast<uint8_t>(flow_status);
	message.as.flow_control.BS = block_size;
	message.as.flow_control.STmin = ms_to_st_min(st_min_ms);

	/* send message */
#ifdef ISO_TP_FRAME_PADDING
	(void) memset(message.as.flow_control.reserve, 0, sizeof(message.as.flow_control.reserve));
	ret = user_send_can(link.send_arbitration_id, message.as.data_array.ptr, sizeof(message));
#else
	ret = user_send_can(
		link.send_arbitration_id,
		message.as.data_array.ptr,
		3
	);
#endif

	return ret;
}
IsoTpRet IsoTp::send_single_frame(uint32_t id)
{
	IsoTpCanMessage message;
	IsoTpRet ret;

	/* multi frame message length must greater than 7  */
	assert(link.send_size <= 7);

	/* setup message  */
	message.as.single_frame.type = static_cast<uint8_t>(IsoTpProtocolControlInformation::SINGLE);
	message.as.single_frame.SF_DL = (uint8_t) link.send_size;
	(void) memcpy(message.as.single_frame.data, link.send_buffer, link.send_size);

	/* send message */
#ifdef ISO_TP_FRAME_PADDING
	(void) memset(message.as.single_frame.data + link.send_size, 0, sizeof(message.as.single_frame.data) - link.send_size);
	ret = user_send_can(id, message.as.data_array.ptr, sizeof(message));
#else
	ret = user_send_can(
		id,
		message.as.data_array.ptr,
		link.send_size + 1
	);
#endif

	return ret;
}
IsoTpRet IsoTp::send_first_frame(uint32_t id)
{
	IsoTpCanMessage message;
	IsoTpRet ret;
	
	/* multi frame message length must greater than 7  */
	assert(link.send_size > 7);
	
	/* setup message  */
	message.as.first_frame.type = static_cast<uint8_t>(IsoTpProtocolControlInformation::FIRST_FRAME);
	message.as.first_frame.FF_DL_low = (uint8_t) link.send_size;
	message.as.first_frame.FF_DL_high = (uint8_t) (0x0F & (link.send_size >> 8));
	(void) memcpy(message.as.first_frame.data, link.send_buffer, sizeof(message.as.first_frame.data));
	
	/* send message */
	ret = user_send_can(id, message.as.data_array.ptr, sizeof(message));
	if (IsoTpRet::OK == ret) {
		link.send_offset += sizeof(message.as.first_frame.data);
		link.send_sn = 1;
	}
	
	return ret;
}
IsoTpRet IsoTp::send_consecutive_frame()
{
	IsoTpCanMessage message;
	uint16_t data_length;
	IsoTpRet ret;

	/* multi frame message length must greater than 7  */
	assert(link.send_size > 7);

	/* setup message  */
	message.as.consecutive_frame.type = static_cast<uint8_t>(IsoTpProtocolControlInformation::CONSECUTIVE_FRAME);
	message.as.consecutive_frame.SN = link.send_sn;
	data_length = link.send_size - link.send_offset;
	if (data_length > sizeof(message.as.consecutive_frame.data)) {
		data_length = sizeof(message.as.consecutive_frame.data);
	}
	(void) memcpy(message.as.consecutive_frame.data, link.send_buffer + link.send_offset, data_length);

	/* send message */
#ifdef ISO_TP_FRAME_PADDING
	(void) memset(message.as.consecutive_frame.data + data_length, 0, sizeof(message.as.consecutive_frame.data) - data_length);
	ret = user_send_can(link.send_arbitration_id, message.as.data_array.ptr, sizeof(message));
#else
	ret = user_send_can(link.send_arbitration_id,
		message.as.data_array.ptr,
		data_length + 1);
#endif
	if (IsoTpRet::OK == ret) {
		link.send_offset += data_length;
		if (++(link.send_sn) > 0x0F) {
			link.send_sn = 0;
		}
	}
	
	return ret;
}
IsoTpRet IsoTp::receive_single_frame(IsoTpCanMessage *message, uint8_t len)
{
	/* check data length */
	if ((0 == message->as.single_frame.SF_DL) || (message->as.single_frame.SF_DL > (len - 1))) {
		user_debug("Single-frame length too small.");
		return IsoTpRet::LENGTH;
	}

	/* copying data */
	(void) memcpy(link.receive_buffer, message->as.single_frame.data, message->as.single_frame.SF_DL);
	link.receive_size = message->as.single_frame.SF_DL;
	
	return IsoTpRet::OK;
}
IsoTpRet IsoTp::receive_first_frame(IsoTpCanMessage *message, uint8_t len)
{
	uint16_t payload_length;

	if (8 != len) {
		user_debug("First frame should be 8 bytes in length.");
		return IsoTpRet::LENGTH;
	}
	
	/* check data length */
	payload_length = message->as.first_frame.FF_DL_high;
	payload_length = (payload_length << 8) + message->as.first_frame.FF_DL_low;
	
	/* should not use multiple frame transmition */
	if (payload_length <= 7) {
		user_debug("Should not use multiple frame transmission.");
		return IsoTpRet::LENGTH;
	}
	
	if (payload_length > link.receive_buf_size) {
		user_debug("Multi-frame response too large for receiving buffer.");
		return IsoTpRet::OVERFLOW;
	}
	
	/* copying data */
	(void) memcpy(link.receive_buffer, message->as.first_frame.data, sizeof(message->as.first_frame.data));
	link.receive_size = payload_length;
	link.receive_offset = sizeof(message->as.first_frame.data);
	link.receive_sn = 1;
	
	return IsoTpRet::OK;
}
IsoTpRet IsoTp::receive_consecutive_frame(IsoTpCanMessage *message, uint8_t len)
{
	uint16_t remaining_bytes;
	
	/* check sn */
	if (link.receive_sn != message->as.consecutive_frame.SN) {
		return IsoTpRet::WRONG_SN;
	}
	
	/* check data length */
	remaining_bytes = link.receive_size - link.receive_offset;
	if (remaining_bytes > sizeof(message->as.consecutive_frame.data)) {
		remaining_bytes = sizeof(message->as.consecutive_frame.data);
	}
	if (remaining_bytes > len - 1) {
		user_debug("Consecutive frame too short.");
		return IsoTpRet::LENGTH;
	}
	
	/* copying data */
	(void) memcpy(link.receive_buffer + link.receive_offset, message->as.consecutive_frame.data, remaining_bytes);
	
	link.receive_offset += remaining_bytes;
	if (++(link.receive_sn) > 0x0F) {
		link.receive_sn = 0;
	}
	
	return IsoTpRet::OK;
}
IsoTpRet IsoTp::receive_flow_control_frame(IsoTpCanMessage *message, uint8_t len)
{
	(void)message;
	/* check message length */
	if (len < 3) {
		user_debug("Flow control frame too short.");
		return IsoTpRet::LENGTH;
	}

	return IsoTpRet::OK;
}

///////////////////////////////////////////////////////
///                 PUBLIC FUNCTIONS                ///
///////////////////////////////////////////////////////

IsoTp::IsoTp()
{
	memset((void *)&this->link, 0, sizeof(this->link));
}

void IsoTp::init(
	uint32_t sendid,
	uint8_t *sendbuf,
	uint16_t sendbufsize,
	uint8_t *recvbuf,
	uint16_t recvbufsize
)
{
	memset(&link, 0, sizeof(link));
	link.receive_status = IsoTpReceiveStatus::IDLE;
	link.send_status = IsoTpSendStatus::IDLE;
	link.send_arbitration_id = sendid;
	link.send_buffer = sendbuf;
	link.send_buf_size = sendbufsize;
	link.receive_buffer = recvbuf;
	link.receive_buf_size = recvbufsize;
}

void IsoTp::poll()
{
	IsoTpRet ret;

	/* only polling when operation in progress */
	if (IsoTpSendStatus::INPROGRESS == link.send_status) {
	
		/* continue send data */
		if (/* send data if bs_remain is invalid or bs_remain large than zero */
		(IsoTpGeneral::INVALID_BS == link.send_bs_remain || link.send_bs_remain > 0) &&
		/* and if st_min is zero or go beyond interval time */
		(0 == link.send_st_min || (0 != link.send_st_min && IsoTpTimeAfter(user_get_ms(), link.send_timer_st)))) {
		
		ret = send_consecutive_frame();
		if (IsoTpRet::OK == ret) {
			if (IsoTpGeneral::INVALID_BS != link.send_bs_remain) {
			link.send_bs_remain -= 1;
			}
			link.send_timer_bs = user_get_ms() + IsoTpConfig::DEFAULT_RESPONSE_TIMEOUT;
			link.send_timer_st = user_get_ms() + link.send_st_min;
	
			/* check if send finish */
			if (link.send_offset >= link.send_size) {
			link.send_status = IsoTpSendStatus::IDLE;
			}
		} else {
			link.send_status = IsoTpSendStatus::ERROR;
		}
		}
	
		/* check timeout */
		if (IsoTpTimeAfter(user_get_ms(), link.send_timer_bs)) {
		link.send_protocol_result = IsoTpProtocolResult::TIMEOUT_BS;
		link.send_status = IsoTpSendStatus::ERROR;
		}
	}
	
	/* only polling when operation in progress */
	if (IsoTpReceiveStatus::INPROGRESS == link.receive_status) {
		
		/* check timeout */
		if (IsoTpTimeAfter(user_get_ms(), link.receive_timer_cr)) {
		link.receive_protocol_result = IsoTpProtocolResult::TIMEOUT_CR;
		link.receive_status = IsoTpReceiveStatus::IDLE;
		}
	}
	
	return;

}
void IsoTp::on_can_message(const uint8_t *data, uint8_t len)
{
	IsoTpCanMessage message;
	IsoTpRet ret;
	
	if (len < 2 || len > 8) {
		return;
	}
	
	memcpy(message.as.data_array.ptr, data, len);
	memset(message.as.data_array.ptr + len, 0, sizeof(message.as.data_array.ptr) - len);
	
	switch (message.as.common.type) {
	case static_cast<uint8_t>(IsoTpProtocolControlInformation::SINGLE): {
		/* update protocol result */
		if (IsoTpReceiveStatus::INPROGRESS == link.receive_status) {
			link.receive_protocol_result = IsoTpProtocolResult::UNEXP_PDU;
		} else {
			link.receive_protocol_result = IsoTpProtocolResult::OK;
		}
	
		/* handle message */
		ret = receive_single_frame(&message, len);
		
		if (IsoTpRet::OK == ret) {
			/* change status */
			link.receive_status = IsoTpReceiveStatus::FULL;
		}
		break;
	}
	case static_cast<uint8_t>(IsoTpProtocolControlInformation::FIRST_FRAME): {
		/* update protocol result */
		if (IsoTpReceiveStatus::INPROGRESS == link.receive_status) {
			link.receive_protocol_result = IsoTpProtocolResult::UNEXP_PDU;
		} else {
			link.receive_protocol_result = IsoTpProtocolResult::OK;
		}
	
		/* handle message */
		ret = receive_first_frame(&message, len);
	
		/* if overflow happened */
		if (IsoTpRet::OVERFLOW == ret) {
			/* update protocol result */
			link.receive_protocol_result = IsoTpProtocolResult::BUFFER_OVFLW;
			/* change status */
			link.receive_status = IsoTpReceiveStatus::IDLE;
			/* send error message */
			send_flow_control(IsoTpFlowStatus::OVERFLOW, 0, 0);
			break;
		}
	
		/* if receive successful */
		if (IsoTpRet::OK == ret) {
			/* change status */
			link.receive_status = IsoTpReceiveStatus::INPROGRESS;
			/* send fc frame */
			link.receive_bs_count = IsoTpConfig::DEFAULT_BLOCK_SIZE;
			send_flow_control(IsoTpFlowStatus::CONTINUE, link.receive_bs_count, IsoTpConfig::DEFAULT_ST_MIN);
			/* refresh timer cs */
			link.receive_timer_cr = user_get_ms() + IsoTpConfig::DEFAULT_RESPONSE_TIMEOUT;
		}
		
		break;
	}
	case static_cast<uint8_t>(IsoTpProtocolControlInformation::CONSECUTIVE_FRAME): {
		/* check if in receiving status */
		if (IsoTpReceiveStatus::INPROGRESS != link.receive_status) {
			link.receive_protocol_result = IsoTpProtocolResult::UNEXP_PDU;
			break;
		}
	
		/* handle message */
		ret = receive_consecutive_frame(&message, len);
	
		/* if wrong sn */
		if (IsoTpRet::WRONG_SN == ret) {
			link.receive_protocol_result = IsoTpProtocolResult::WRONG_SN;
			link.receive_status = IsoTpReceiveStatus::IDLE;
			break;
		}
	
		/* if success */
		if (IsoTpRet::OK == ret) {
			/* refresh timer cs */
			link.receive_timer_cr = user_get_ms() + IsoTpConfig::DEFAULT_RESPONSE_TIMEOUT;
			
			/* receive finished */
			if (link.receive_offset >= link.receive_size) {
			link.receive_status = IsoTpReceiveStatus::FULL;
			} else {
			/* send fc when bs reaches limit */
			if (0 == --link.receive_bs_count) {
				link.receive_bs_count = IsoTpConfig::DEFAULT_BLOCK_SIZE;
				send_flow_control(IsoTpFlowStatus::CONTINUE, link.receive_bs_count, IsoTpConfig::DEFAULT_ST_MIN);
			}
			}
		}
		
		break;
	}
	case static_cast<uint8_t>(IsoTpProtocolControlInformation::FLOW_CONTROL_FRAME):
		/* handle fc frame only when sending in progress  */
		if (IsoTpSendStatus::INPROGRESS != link.send_status) {
			break;
		}
	
		/* handle message */
		ret = receive_flow_control_frame(&message, len);
		
		if (IsoTpRet::OK == ret) {
			/* refresh bs timer */
			link.send_timer_bs = user_get_ms() + IsoTpConfig::DEFAULT_RESPONSE_TIMEOUT;
	
			/* overflow */
			if (static_cast<uint8_t>(IsoTpFlowStatus::OVERFLOW) == message.as.flow_control.FS) {
				link.send_protocol_result = IsoTpProtocolResult::BUFFER_OVFLW;
				link.send_status = IsoTpSendStatus::ERROR;
			}
	
			/* wait */
			else if (static_cast<uint8_t>(IsoTpFlowStatus::WAIT) == message.as.flow_control.FS) {
				link.send_wtf_count += 1;
				/* wait exceed allowed count */
				if (link.send_wtf_count > IsoTpConfig::MAX_WFT_NUMBER) {
					link.send_protocol_result = IsoTpProtocolResult::WFT_OVRN;
					link.send_status = IsoTpSendStatus::ERROR;
				}
			}
	
			/* permit send */
			else if (static_cast<uint8_t>(IsoTpFlowStatus::CONTINUE) == message.as.flow_control.FS) {
				if (0 == message.as.flow_control.BS) {
					link.send_bs_remain = IsoTpGeneral::INVALID_BS;
				} else {
					link.send_bs_remain = message.as.flow_control.BS;
				}
				link.send_st_min = st_min_to_ms(message.as.flow_control.STmin);
				link.send_wtf_count = 0;
			}
			break;
		}
	default:
		break;
	};
	
	return;
}
IsoTpRet IsoTp::send(const uint8_t payload[], uint16_t size)
{
	return send_with_id(link.send_arbitration_id, payload, size);
}
IsoTpRet IsoTp::send_with_id(uint32_t id, const uint8_t payload[], uint16_t size)
{
	IsoTpRet ret;
	
	if (size > link.send_buf_size) {
		user_debug("Message size too large. Increase ISO_TP_MAX_MESSAGE_SIZE to set a larger buffer\n");
		char message[128];
		sprintf(&message[0], "Attempted to send %d bytes; max size is %d!\n", size, link.send_buf_size);
		return IsoTpRet::OVERFLOW;
	}
	
	if (IsoTpSendStatus::INPROGRESS == link.send_status) {
		user_debug("Abort previous message, transmission in progress.\n");
		return IsoTpRet::INPROGRESS;
	}
	
	/* copy into local buffer */
	link.send_size = size;
	link.send_offset = 0;
	(void) memcpy(link.send_buffer, payload, size);
	
	if (link.send_size < 8) {
		/* send single frame */
		ret = send_single_frame(id);
	} else {
		/* send multi-frame */
		ret = send_first_frame(id);
	
		/* init multi-frame control flags */
		if (IsoTpRet::OK == ret) {
		link.send_bs_remain = 0;
		link.send_st_min = 0;
		link.send_wtf_count = 0;
		link.send_timer_st = user_get_ms();
		link.send_timer_bs = user_get_ms() + IsoTpConfig::DEFAULT_RESPONSE_TIMEOUT;
		link.send_protocol_result = IsoTpProtocolResult::OK;
		link.send_status = IsoTpSendStatus::INPROGRESS;
		}
	}
	
	return ret;
}
IsoTpRet IsoTp::receive(
	uint8_t *payload,
	const uint16_t payload_size,
	uint16_t *out_size
)
{
	uint16_t copylen;
	
	if (IsoTpReceiveStatus::FULL != link.receive_status) {
		return IsoTpRet::NO_DATA;
	}
	
	copylen = link.receive_size;
	if (copylen > payload_size) {
		copylen = payload_size;
	}
	
	memcpy(payload, link.receive_buffer, copylen);
	*out_size = copylen;
	
	link.receive_status = IsoTpReceiveStatus::IDLE;
	
	return IsoTpRet::OK;
}

///////////////////////////////////////////////////////
///                 VIRTUAL FUNCTIONS               ///
///////////////////////////////////////////////////////

void IsoTp::user_debug(const char* message, ...)
{
	va_list args;
	va_start(args, message);
	vprintf(message, args);
	va_end(args);
}
IsoTpRet IsoTp::user_send_can(
	const uint32_t arbitration_id,
	const uint8_t* data,
	const uint8_t size
)
{
	(void)arbitration_id;
	(void)data;
	(void)size;
	//std::cout << "arbitration_id: " << arbitration_id << ", data: ";
	//for (uint8_t i = 0; i < size; ++i) {
	//	std::cout << std::hex << static_cast<int>(data[i]) << " ";
	//}
	//std::cout << ", size: " << static_cast<int>(size) << std::endl;
	return IsoTpRet::OK;
}
uint32_t IsoTp::user_get_ms(void)
{
	
	return static_cast<uint32_t>(
		std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now().time_since_epoch()
		).count()
	);
}
