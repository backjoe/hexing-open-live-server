#ifndef __RTP_H__
#define __RTP_H__

#include "Udp.h"
#include "VBuffer.h"

const UINT16 RTP_HEADER_SIZE = 12;
const UINT16 RTP_H263_HEADER_SIZE = 2;

class Rtp : public Udp
{
public:
	Rtp(UINT mtu = 1500);
	virtual ~Rtp();

	virtual int Read(BYTE* pBuffer, UINT16 bufferSize,
		INT8* pPayloadType, UINT16* pSequenceNumber, INT32* pTimeStamp, INT32* pSsrc, UINT nTimeOut = 500000);

	virtual int Write(PBYTE pBuffer, UINT16 bufferSize, 
		INT8 payloadType = 0, INT32 timeStamp = 0, INT32 ssrc = 0, BOOL marker = FALSE, UINT nTimeOut = 500000);

protected:

	void CreateRtpPacket(PBYTE pData, UINT16 dataSize, UINT8 nPayloadType, INT32 timeStamp, INT32 nSSRC, BOOL marker = FALSE);
	BOOL ParseRtpHeader(PBYTE pRtpHeader, UINT8* pPayloadType, UINT16* pSequenceNumber, INT32* pTimeStamp, INT32* pSsrc);

	UINT16	m_SequenceNumber;

	Buffer	m_RtpPacket;
};

#endif //__RTP_H__