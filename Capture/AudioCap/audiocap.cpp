//// audiocap.cpp : 定义控制台应用程序的入口点。
////
//
#include "stdafx.h"
//#include "AudioCapT.h"
//#include <conio.h>
//
//extern "C"
//{
//#include <libavformat/avformat.h>
//#include <libswscale/swscale.h>
//#include <libswresample/swresample.h>
//#include <libavutil/opt.h>
//#include <libavutil/channel_layout.h>
//#include <libavutil/samplefmt.h>
//}
//
//extern bool bStop;
//
//extern CRITICAL_SECTION csMp4;
//
//extern AVStream * pAudioStream;
//
//extern AVCodecContext * pAACContext;
//
//extern AVFormatContext * pAACFormat;
//
//extern AVCodecContext * pH264CodeContext;
//
//#include <iostream>
//using namespace std;
//#pragma comment(lib,"avformat.lib")
//#pragma comment(lib,"avcodec.lib")
//#pragma comment(lib,"avutil.lib")
//#pragma comment(lib,"swscale.lib")
//#pragma comment(lib,"swresample.lib")
//
//UINT __stdcall CaptureAudioThreadProc(LPVOID param)
//{
//	int64_t src_ch_layout = av_get_default_channel_layout(2), dst_ch_layout = AV_CH_LAYOUT_STEREO;
//	int src_rate = 48000, dst_rate = 44100;
//	uint8_t **src_data = NULL, **dst_data = NULL;
//	int src_nb_channels = 0, dst_nb_channels = 0;
//	int src_linesize, dst_linesize;
//	int src_nb_samples = 1024, dst_nb_samples, max_dst_nb_samples;
//	enum AVSampleFormat src_sample_fmt = AV_SAMPLE_FMT_FLT, dst_sample_fmt = AV_SAMPLE_FMT_FLTP;
//	char pszDstFileName[256] = "D:\\test123.aac";
//	FILE * fDstFile = NULL;
//	int nDstBufSize = 0;
//	const char * strFmt = NULL;
//	struct SwrContext * swr_ctx = NULL;
//	int nRet = 0;
//
//	Audio_Data * pAudio = NULL;
//
//	int nAudioLeftLen = 0;
//
//	int nPcmBufferLen = 0;
//
//	int nLen = 0;
//
//	int nReadSize = 0;
//
//	CAudioCapT ac;
//
//	CoInitialize(NULL);
//
//	av_register_all();
//
//	avcodec_register_all();
//
//	src_nb_channels = av_get_channel_layout_nb_channels(src_ch_layout);
//
//	//AVCodec * pAACCodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
//
//	//if (!pAACCodec)
//	//{
//	//	printf("aac coder not found.\n");
//	//	return -1;
//	//}
//
//	//AVCodecContext * pAACContext = avcodec_alloc_context3(pAACCodec);
//	//if (!pAACContext)
//	//{
//	//	printf("alloc aac context failed.n");
//	//	goto end;
//	//}
//
//	//pAACContext->bit_rate = 64000;
//	//pAACContext->sample_rate = 44100;
//	//pAACContext->sample_fmt = AV_SAMPLE_FMT_FLTP;
//	//pAACContext->channel_layout = AV_CH_LAYOUT_STEREO;
//	//pAACContext->channels = 2;
//	//pAACContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
//	//pAACContext->codec_type = AVMEDIA_TYPE_AUDIO;
//	//pAACContext->codec_id = AV_CODEC_ID_AAC;
//	//pAACContext->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
//
//
//	//nRet = avcodec_open2(pAACContext, pAACCodec, NULL);
//	//if (nRet < 0)
//	//{
//	//	printf("open aac coder failed.\n");
//	//	goto end;
//	//}
//
//	
//	//AVFormatContext * pAACFormat = (AVFormatContext *)param;
//
//	//avformat_alloc_output_context2(&pAACFormat, NULL, NULL, pszDstFileName);
//
//	//if (!pAACFormat)
//	//{
//	//	printf("alloc aac format failed.\n");
//	//	goto end;
//	//}
//
//	/*AVStream * pAudioStream = avformat_new_stream(pAACFormat, NULL);
//	pAudioStream->codecpar->codec_tag = 0;
//	avcodec_parameters_from_context(pAudioStream->codecpar, pAACContext);
//
//	nRet = avio_open(&pAACFormat->pb, pszDstFileName, AVIO_FLAG_WRITE);
//	if (nRet < 0)
//	{
//		printf("open file failed.\n");
//		goto end;
//	}
//
//	nRet = avformat_write_header(pAACFormat, NULL);*/
//
//	//3  open io ,write head
//
//	AVFrame * pFrame = av_frame_alloc();
//
//	if (!pFrame)
//	{
//		printf("alloc frame failed.\n");
//		goto end;
//	}
//
//	pFrame->format = AV_SAMPLE_FMT_FLTP;
//	pFrame->channels = 2;
//	pFrame->channel_layout = AV_CH_LAYOUT_STEREO;
//	pFrame->nb_samples = 1024;
//
//	nRet = av_frame_get_buffer(pFrame, 0);
//
//	if (nRet < 0)
//	{
//		printf("alloc frame buffer failed.\n");
//		goto end;
//	}
//
//	swr_ctx = swr_alloc_set_opts(swr_ctx, dst_ch_layout, AV_SAMPLE_FMT_FLTP, 44100,	//输出格式
//		AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_FLT, 48000,		//输入格式
//		0, 0);
//
//	if (!swr_ctx)
//	{
//		printf("swr_alloc_set_opts error\n");
//		goto end;
//	}
//	
//	nRet = swr_init(swr_ctx);
//
//	if (nRet < 0)
//	{
//		printf("swr init failed.\n");
//
//		goto end;
//	}
//	
//	ac.SetDeiveType(CAudioCapT::SPEAKER);
//	ac.Init();
//	ac.Start();
//	
//	int	nSrcSamples = av_rescale_rnd(swr_get_delay(swr_ctx, 44100) + 1024, 48000, 44100, AV_ROUND_UP);
//
//	nReadSize = nSrcSamples * 2 * 4;
//
//	char * pPcmBuffer = new char[nReadSize];
//
//	if (!pPcmBuffer)
//	{
//		printf("Pcm Buffer failed.\n");
//		goto end;
//	}
//
//	int nVpts = 0;
//
//	while (1)
//	{
//		if (bStop == true) 
//		{
//			ac.Stop();
//			break;
//		}
//
//		//缓冲区是否已经满了
//		while (nPcmBufferLen != nReadSize)
//		{
//			if (pAudio == NULL)
//			{
//				pAudio = ac.GetAudio();
//				if (pAudio)
//					nAudioLeftLen = pAudio->iDataLen;
//				continue;
//			}
//			else
//			{
//				nLen = nReadSize - nPcmBufferLen;
//				if (nLen >= nAudioLeftLen)
//				{
//					memcpy(pPcmBuffer + nPcmBufferLen, pAudio->pData + (pAudio->iDataLen - nAudioLeftLen), nAudioLeftLen);
//					nPcmBufferLen += nAudioLeftLen;
//					nAudioLeftLen -= nAudioLeftLen;
//					delete [] pAudio->pData;
//					pAudio->pData = NULL;
//					delete pAudio;
//					pAudio = NULL;	
//					continue;
//				}
//				else
//				{
//					memcpy(pPcmBuffer + nPcmBufferLen, pAudio->pData + (pAudio->iDataLen - nAudioLeftLen), nLen);
//					nPcmBufferLen += nLen;
//					nAudioLeftLen -= nLen;
//				}
//			}
//		}
//
//		const uint8_t *data[1];
//		data[0] = (uint8_t*)pPcmBuffer;
//
//		int len = swr_convert(swr_ctx, pFrame->data, pFrame->nb_samples, data, nSrcSamples);
//		if (len <= 0)
//			break;	
//
//		AVPacket pkt;
//		av_init_packet(&pkt);
//
//		//pFrame->pts += nVpts;
//
//	
//
//		//6 音频编码
//		nRet = avcodec_send_frame(pAACContext, pFrame);
//		
//		while (nRet >= 0)
//		{
//			nRet = avcodec_receive_packet(pAACContext, &pkt);
//			if (nRet == AVERROR(EAGAIN) || nRet == AVERROR_EOF)
//				break;
//			pkt.pts = nVpts;
//			nVpts += 1024;
//			pkt.dts = pkt.pts;
//			pkt.stream_index = pAudioStream->index;
//			::EnterCriticalSection(&csMp4);
//			nRet = av_interleaved_write_frame(pAACFormat, &pkt);
//			::LeaveCriticalSection(&csMp4);
//		}
//
//		nPcmBufferLen = 0;
//		printf("A+");
//	}
//
//	AVPacket pkt;
//	av_init_packet(&pkt);
//
//	nRet = avcodec_send_frame(pAACContext, NULL);
//
//	while (nRet >= 0)
//	{
//		nRet = avcodec_receive_packet(pAACContext, &pkt);
//		if (nRet == AVERROR(EAGAIN) || nRet == AVERROR_EOF)
//			break;
//		pkt.stream_index = pAudioStream->index;
//		pkt.pts = 0;
//		pkt.dts = 0;
//		::EnterCriticalSection(&csMp4);
//		nRet = av_interleaved_write_frame(pAACFormat, &pkt);
//		::LeaveCriticalSection(&csMp4);
//	}
//
//end:
//	if (pAudio)
//	{
//		if (pAudio->pData)
//			delete[] pAudio->pData;
//		pAudio->pData = NULL;
//		delete pAudio;
//		pAudio = NULL;
//	}
//
//	if (pPcmBuffer)
//	{
//		delete[] pPcmBuffer;
//		pPcmBuffer = NULL;
//	}
//	
//	if (pFrame)
//	{
//		av_frame_free(&pFrame);
//	}
//	
//	
//
//	swr_free(&swr_ctx);
//
//	::CoUninitialize();
//
//	printf("\n\n");
//
//	printf("audio program exit!\n");
//
//	return 0;
//}