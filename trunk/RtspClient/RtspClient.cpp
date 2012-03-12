// RtspClient.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include "RtspRequest.h"   
#include "Rtp.h"

#include "VBuffer.h"

RtspRequest g_RtspRequest; 

int main(int argc, char* argv[])
{
	 // ½ÓÊÕ            
	string url = "rtsp://192.168.1.1:554/aacAudioTest";
	string setupName = "aacAudioTest";
	INT rtpPort = 8080;
	INT rtcpPort = rtpPort + 2;
	string sdp;
	INT64 sess;
	
	g_RtspRequest.Open(url.c_str(), "127.0.0.0", 0);
	g_RtspRequest.RequestOptions();
	g_RtspRequest.RequestDescribe(&sdp);
	g_RtspRequest.RequestSetup(setupName.c_str(), transportModeRtpUdp, rtpPort , rtcpPort , &sess);
	g_RtspRequest.RequestPlay();
	
	Rtp* pRtp = new Rtp();
	pRtp->Open("127.0.0.0", rtpPort);

	Buffer buff;
	int iRead;

	INT8 nPayloadType;
	WORD nSequenceNumber;
	INT32 nTimeStamp;
	INT32 nSsrc;

	buff.AllocateBuffer(1500);

	while(TRUE)
	{
		iRead = pRtp->Read(buff.m_pBuffer, 1500, &nPayloadType, &nSequenceNumber, &nTimeStamp, &nSsrc);
		if (iRead > 0)
		{
			// save buff        
		}
	}
	
	delete pRtp;

        g_RtspRequest.RequestPause();
	g_RtspRequest.RequestTeardown();
	g_RtspRequest.Close();
	
	return 0;
}

