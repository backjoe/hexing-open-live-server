#pragma once

#include "RtspService_t.h"
#include "H264.h"
#include "BaseEncoder.h"
#include "MediaStreamH264.h"

#define NETWORK_MTU 1482
#define FILE_READ_SIZE 1024*1024*2

static UINT WINAPI H264File_Read2SendThread(LPVOID pUser)
{
	RtspService_t* pRtspService = (RtspService_t*)pUser;
	
	Buffer		read_buff;			// �ļ���ȡ���ݻ�����
	Buffer		h264_buff;			// H264���ݻ�����
	VBuffer		h264_nal_buff_list;	// H264 nal���ݻ�����
	Buffer		h264_frame_buff;	// H264ÿ֡������nal����

	string		sps_base64;
	string		pps_base64;

	bool_t		b_result;

	bool_t		b_find_sps = FALSE;
	bool_t		b_find_pps = FALSE;
	bool_t		found_frame_start = FALSE;
	uint32_t	i_begin_time = 0;
	uint32_t	i_current_time = 0;
	int64_t		frame_count = 0;
	int64_t		iframe_offset = 0;

	h264_sps_t	sps;
	
	read_buff.AllocateBuffer(FILE_READ_SIZE);
	h264_buff.AllocateBuffer(FILE_READ_SIZE*2);
	memset(&sps, 0, sizeof(h264_sps_t));

	while( !pRtspService->m_bVideoFile_ReadThread_Stop )
	{
		// ��H264 nal���ݲ���ʱ�����ļ��ж�ȡ����
		if (h264_nal_buff_list.GetDataSize() < FILE_READ_SIZE)
		{
			read_buff.ClearData();
			b_result = ReadFile(pRtspService->m_hVideoFile, 
				read_buff.m_pBuffer, 
				read_buff.m_nBufferSize, 
				(DWORD*)&read_buff.m_nDataSize, 
				NULL);

			if (b_result && !read_buff.m_nDataSize)
			{
				SetFilePointer(pRtspService->m_hVideoFile, 0, NULL, FILE_BEGIN);
				continue;
			}
			h264_buff.AppendData(read_buff.m_pData, read_buff.m_nDataSize);

			// ����ȡ��H264�������ָ�Ϊnal�����������ݻ�������������
			h264_split_nal(&h264_buff, &h264_nal_buff_list);
		}

		while(TRUE)
		{
			if (b_find_sps && b_find_pps)
			{
				// ����֡�ʣ����Ʒ����ٶ�
				i_current_time = GetTickCount();
				if (i_current_time < i_begin_time)
				{
					// ���... �������...
					// ����˵��ĳ���ι�˾��ĳ��ְcoderд�ķ���ÿ����2���¾͹ң���ΪRPWT����...
					i_begin_time = i_current_time;
					frame_count = 0;
				}
				
				if (i_current_time - i_begin_time < frame_count * 1000 * 2 * sps.num_units_in_tick / sps.time_scale )
				{
					Sleep(10);
					break;
				}
			}
			
			// ��ȡһ��nal
			Buffer* p_h264_nal = h264_nal_buff_list.GetFullBuffer();
			if (!p_h264_nal)
				break;

			// ��ȡnal type
			int i_nal_type;
			h264_get_nal_type(&i_nal_type, p_h264_nal->m_pData);

			// ��nal��֡���飬ע�⣺�Һ��Ե���SEI��PPS��SPS
			if (i_nal_type != 6 && i_nal_type != 7 && i_nal_type != 8)
				h264_frame_buff.AppendData(p_h264_nal->m_pData, p_h264_nal->m_nDataSize);

			// ����sps nal���Ի�ȡ֡�ʣ��������ݷ����ٶ�
			if ( !b_find_sps )
			{
				// �ҵ�sps nal
				if ( i_nal_type == 7)
				{
					Buffer sps_buff;
					sps_buff.AllocateBuffer(p_h264_nal->m_nDataSize);
					
					h264_decode_annexb( sps_buff.m_pBuffer, (INT *)&sps_buff.m_nDataSize, 
						p_h264_nal->m_pData+4, p_h264_nal->m_nDataSize-4);
					
					h264_decode_seq_parameter_set(sps_buff.m_pData, sps_buff.m_nDataSize, &sps);
					if (sps.time_scale)	// Ԥ��sps���ݴ��󣬵���sps.time_scaleΪ0��Ϊ����
					{
						BaseEncoder::Base64Encode(p_h264_nal->m_pData, p_h264_nal->m_nDataSize, &sps_base64);
						b_find_sps = TRUE;
					}
					else
					{
						// SPSû��֡����Ϣ (��Щα�ӱ�����̫�ɶ��ˣ�Ӧ�ó�����������:( )
						printf("The SPS's time_scale field info is error!\n");
					}
				}
			}

			// ����pps������ʼ����Ƶ����Ϣ
			if ( !b_find_pps )
			{
				// �ҵ�pps nal
				if ( i_nal_type == 8 )
				{
					BaseEncoder::Base64Encode(p_h264_nal->m_pData, p_h264_nal->m_nDataSize, &pps_base64);
					b_find_pps = TRUE;
				}

				// ��ʼ����Ƶ����Ϣ
				if (b_find_sps && b_find_pps)
				{
					string sprop_parameter_sets;
					sprop_parameter_sets = sps_base64;
					sprop_parameter_sets += ',';
					sprop_parameter_sets += pps_base64;
					
					((MediaStreamH264*)(pRtspService->m_pMediaStream_Video))->Init(0,
						NETWORK_MTU, sps.mb_width, sps.mb_height, sps.profile_idc, sprop_parameter_sets);

					i_begin_time = GetTickCount();
				}
			}
			
			// ֡�����Ѿ�����һ֡����
			if ( h264_find_frame_end(&found_frame_start, p_h264_nal->m_pData, p_h264_nal->m_nDataSize, i_nal_type) )
			{
				frame_count++;
				
				// �������ݣ�ע�������ʱ��Ƶ���ǰ�90k����
				if (b_find_sps && b_find_pps)
				{
					int64_t i_pts = h264_get_pts_rtp(&iframe_offset, frame_count, &sps, p_h264_nal->m_pData, p_h264_nal->m_nDataSize, i_nal_type);
					pRtspService->m_pMediaStream_Video->TransportData(h264_frame_buff.m_pData, h264_frame_buff.m_nBufferSize, i_pts);
					h264_frame_buff.ClearData();
				}
			}

			h264_nal_buff_list.AddEmptyBuffer(p_h264_nal);
		}
	}
	
	return 0;
}

static UINT WINAPI AACFile_Read2SendThread(LPVOID pUser)
{
	RtspService_t* pRtspService = (RtspService_t*)pUser;
	
	Buffer		read_buff;			// �ļ���ȡ���ݻ�����

	read_buff.AllocateBuffer(FILE_READ_SIZE);
	
	while( !pRtspService->m_bAudioFile_ReadThread_Stop )
	{
		Sleep(10);



		//pRtspService->m_pMediaStream_Audio->TransportData(pData, nDataSize, nPts);
	}
	
	return 0;
}
