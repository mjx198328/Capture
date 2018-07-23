//// ScreenCap.cpp : 定义控制台应用程序的入口点。
////
//
#include "stdafx.h"
//#include <windows.h>
//#include <windowsx.h>
//#include <conio.h>
//
//extern "C"
//{
//	#include <libavformat/avformat.h>
//	#include <libswscale/swscale.h>
//}
//
//extern bool bStop;
//
//extern CRITICAL_SECTION csMp4;
//
//extern int nW;
//
//extern int nH;
//
//extern int nFps;
//
//extern int nDep;
//
//extern AVFormatContext * pAACFormat;
//
//extern AVStream * pStream;
//
//extern AVCodecContext * pH264CodeContext;
//
//#include "ScreenCapture.h"
//
//
//#pragma comment(lib,"avformat.lib")
//#pragma comment(lib,"avcodec.lib")
//#pragma comment(lib,"avutil.lib")
//#pragma comment(lib,"swscale.lib")
//
//UINT __stdcall CaptureScreenThreadProc(LPVOID param)
//{
//	CScreenCapture sc;
//
//
//	int nRet = 0;
//		
//	char outfile[] = "D:\\rgb.mp4";
//
//	av_register_all();
//	avcodec_register_all();
//
//
//
//	//4 rgb to yuv
//	SwsContext *ctx = NULL;
//	ctx = sws_getCachedContext(ctx,
//		nW, nH, AV_PIX_FMT_BGRA,
//		nW, nH, AV_PIX_FMT_YUV420P,
//		SWS_BICUBIC,
//		NULL, NULL, NULL
//		);
//
//	//输出的空间
//	AVFrame * pYuv = av_frame_alloc();
//	if (!pYuv)
//		goto end;
//
	//pYuv->format = AV_PIX_FMT_YUV420P;
//	pYuv->width = nW;
//	pYuv->height = nH;
//
//	nRet = av_frame_get_buffer(pYuv, 0);
//
//	if (nRet < 0)
//	{
//		printf("alloc yuv buffer failed.\n");
//		goto end;
//	}
//
//	AVFormatContext * pOutFormatContext = pAACFormat;
//	
//	//5 wirte mp4 headpOutFormatContext
//	/*nRet = avio_open(&->pb, outfile, AVIO_FLAG_WRITE);
//	if (nRet < 0)
//	{
//		printf("avio_open  failed!\n");
//		goto end;
//	}*/
//
//	//nRet = avformat_write_header(pOutFormatContext, NULL);
//	//if (nRet < 0)
//	//{
//	//	printf("write header failed.\n");
//	//	goto end;
//	//}
//
//	int nPts = 0;
//
//	sc.Start();
//
//	while (true)
//	{
//		if (bStop == true)
//		{
//			sc.Stop();		
//		}
//
//		if (bStop && (sc.GetLeftImageSize() == 0))
//		{
//			break;
//		}
//
//		Screen_Data * pScreenData = sc.GetRGB();
//
//		if (!pScreenData)
//		{
//			continue;
//		}
//
//		uint8_t *indata[AV_NUM_DATA_POINTERS] = { 0 };
//		indata[0] = pScreenData->m_pData;
//		int inlinesize[AV_NUM_DATA_POINTERS] = { 0 };
//		inlinesize[0] = nW * nDep/8;
//
//		nRet = sws_scale(ctx, indata, inlinesize, 0, nH,pYuv->data, pYuv->linesize);
//		if (nRet <= 0)
//			break;
//
//		//6 encode frame
//		pYuv->pts = nPts;
//		nPts = nPts + 6000;
//
//		nRet = avcodec_send_frame(pH264CodeContext, pYuv);
//		if (nRet != 0)
//		{
//			continue;
//		}
//		AVPacket pkt;
//		av_init_packet(&pkt);
//
//		while (nRet >= 0)
//		{
//			nRet = avcodec_receive_packet(pH264CodeContext, &pkt);
//			if (nRet == AVERROR(EAGAIN) || nRet == AVERROR_EOF)
//				break;
//
//			::EnterCriticalSection(&csMp4);
//			pkt.stream_index = pStream->index;
//			av_interleaved_write_frame(pOutFormatContext, &pkt);
//			::LeaveCriticalSection(&csMp4);
//			printf("S+");
//		}		
//
//		if (pScreenData)
//		{
//			if (pScreenData->m_pData)
//			{
//				delete[] pScreenData->m_pData;
//				pScreenData->m_pData = NULL;
//			}
//			delete pScreenData;
//			pScreenData = NULL;
//		}
//	}
//
//	//写入视频索引
////	av_write_trailer(pOutFormatContext);
//
//	//关闭视频输出io
//	//avio_close(pOutFormatContext->pb);
//
//	//清理封装输出上下文
//	//avformat_free_context(pOutFormatContext);
//
//	if (pYuv)
//	{
//		av_frame_free(&pYuv);
//		pYuv = NULL;
//	}
//
//end:
//
//
//	//清理视频重采样上下文
//	sws_freeContext(ctx);
//
//	printf("screen exit program!\n");
//
//	return 0;
//}
//
