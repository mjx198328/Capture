// Capture.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "windows.h"
#include "windowsx.h"
#include <list>
#include "Process.h"
#include <conio.h>


extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libavutil/avassert.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include "libavdevice/avdevice.h"
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
}

#include "ScreenCap\ScreenCapture.h"
#include "AudioCap\AudioCapT.h"

#include <iostream>
using namespace std;

#pragma comment(lib,"avformat.lib")
#pragma comment(lib,"avcodec.lib")
#pragma comment(lib,"avutil.lib")
#pragma comment(lib,"swscale.lib")
#pragma comment(lib,"swresample.lib")
#pragma comment(lib, "avdevice.lib")
#pragma comment(lib, "avfilter.lib")

CScreenCapture sc;

//mic
CAudioCapT ac;
//spk
CAudioCapT ac2;

//视频流
AVStream * vst = NULL;
//音频流
AVStream * ast = NULL;

AVFrame * frame = NULL;

//过滤器
AVFilterGraph * filter_graph = NULL;
AVFilterContext* filter_ctx_src_spk = NULL;
AVFilterContext* filter_ctx_src_mic = NULL;
AVFilterContext* filter_ctx_sink = NULL;

typedef struct OutputStream
{
	AVCodecContext *  enc;
	/* pts of the next frame that will be generated */
	int64_t next_pts;
	int samples_count;
	AVFrame *frame;
	struct SwsContext *sws_ctx;
	struct SwrContext *swr_ctx;
	int nPcmBufferLen;
	char * pPcmBuffer;
	int nSrcSamples;
	int nPcmBufferSize;
	Audio_Data * pAudio;
	int nAudioLeftLen;
} OutputStream;


static int write_frame(AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt)
{
	/* rescale output packet timestamp values from codec to stream timebase */
	av_packet_rescale_ts(pkt, *time_base, st->time_base);
	pkt->stream_index = st->index;
	return av_interleaved_write_frame(fmt_ctx, pkt);
}

static void add_stream(OutputStream *ost, AVFormatContext *oc, AVCodec **codec, enum AVCodecID codec_id, int nStreamType, bool bCreateAudioStream = false, OutputStream *ost2=NULL)
{
	AVStream * st = NULL;
	AVCodecContext *c;
	int i;
	/* find the encoder */
	*codec = avcodec_find_encoder(codec_id);
	if (!(*codec)) {
		fprintf(stderr, "Could not find encoder for '%s'\n",avcodec_get_name(codec_id));
		exit(1);
	}
	if (nStreamType == 1)
	{
		//视频流
		st = avformat_new_stream(oc, NULL);
		vst = st;
	}
	else if (nStreamType == 2)
	{
		//音频流
		if (bCreateAudioStream)
		{
			st = avformat_new_stream(oc, NULL);
			ast = st;
		}
		else
			st = ast;
	}
	else
	{
		printf("stream type not right.\n");
		exit(1);
	}		

	if (!st)
	{
		fprintf(stderr, "Could not allocate stream\n");
		exit(1);
	}

	st->id = oc->nb_streams - 1;
	if (nStreamType == 2 && bCreateAudioStream == false)
	{
		c = ost2->enc;
	}
	else
		c = avcodec_alloc_context3(*codec);
	if (!c) {
		fprintf(stderr, "Could not alloc an encoding context\n");
		exit(1);
	}
	ost->enc = c;
	switch ((*codec)->type) {
	case AVMEDIA_TYPE_AUDIO:
		if (nStreamType == 2 && bCreateAudioStream)
		{
			c->sample_fmt = AV_SAMPLE_FMT_FLTP;
			c->bit_rate = 64000;
			c->sample_rate = 44100;
			c->channels = 2;
			c->channel_layout = AV_CH_LAYOUT_STEREO;
			st->time_base.den = c->sample_rate;
			st->time_base.num = 1;
		}
		break;
	case AVMEDIA_TYPE_VIDEO:
		c->codec_id = codec_id;
		c->bit_rate = 400000;
		/* Resolution must be a multiple of two. */
		c->width = GetSystemMetrics(SM_CXSCREEN);;
		c->height = GetSystemMetrics(SM_CYSCREEN);;
		/* timebase: This is the fundamental unit of time (in seconds) in terms
		* of which frame timestamps are represented. For fixed-fps content,
		* timebase should be 1/framerate and timestamp increments should be
		* identical to 1. */
		st->time_base.den = 15;
		st->time_base.num = 1;
		c->time_base = st->time_base;
		c->gop_size = 12; /* emit one intra frame every twelve frames at most */
		c->pix_fmt = AV_PIX_FMT_YUV420P;
		break;
	default:
		break;
	}
	/* Some formats want stream headers to be separate. */
	if (oc->oformat->flags & AVFMT_GLOBALHEADER)
		if (nStreamType == 1 || nStreamType == 2 && bCreateAudioStream)
			c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
}

static AVFrame *alloc_audio_frame(enum AVSampleFormat sample_fmt,
	uint64_t channel_layout,
	int sample_rate, int nb_samples)
{
	AVFrame *frame = av_frame_alloc();
	int ret;
	if (!frame) {
		fprintf(stderr, "Error allocating an audio frame\n");
		exit(1);
	}
	frame->format = sample_fmt;
	frame->channel_layout = channel_layout;
	frame->sample_rate = sample_rate;
	frame->nb_samples = nb_samples;
	if (nb_samples) {
		ret = av_frame_get_buffer(frame, 0);
		if (ret < 0) {
			fprintf(stderr, "Error allocating an audio buffer\n");
			exit(1);
		}
	}
	return frame;
}
static void open_audio(AVFormatContext *oc, AVCodec *codec, OutputStream *  ost, AVDictionary *opt_arg)
{
	int nInit = 0;
	static AVCodecContext *c;
	int nb_samples;
	int ret;
	AVDictionary *opt = NULL;
	c = ost->enc;
	/* open it */

	if (nInit == 0)
	{
		av_dict_copy(&opt, opt_arg, 0);
		ret = avcodec_open2(c, codec, &opt);
		av_dict_free(&opt);
		if (ret < 0) {
			exit(1);
		}
	}
	nb_samples = c->frame_size;
	ost->frame = alloc_audio_frame(c->sample_fmt, c->channel_layout,c->sample_rate, nb_samples);
	/* copy the stream parameters to the muxer */
	ret = avcodec_parameters_from_context(ast->codecpar, c);
	if (ret < 0) {
		fprintf(stderr, "Could not copy the stream parameters\n");
		exit(1);
	}
	/* create resampler context */
	ost->swr_ctx = swr_alloc();
	if (!ost->swr_ctx) {
		fprintf(stderr, "Could not allocate resampler context\n");
		exit(1);
	}
	/* set options */
	av_opt_set_int(ost->swr_ctx, "in_channel_count", c->channels, 0);
	av_opt_set_int(ost->swr_ctx, "in_sample_rate", 48000, 0);
	av_opt_set_sample_fmt(ost->swr_ctx, "in_sample_fmt", AV_SAMPLE_FMT_FLT, 0);
	av_opt_set_int(ost->swr_ctx, "out_channel_count", c->channels, 0);
	av_opt_set_int(ost->swr_ctx, "out_sample_rate", c->sample_rate, 0);
	av_opt_set_sample_fmt(ost->swr_ctx, "out_sample_fmt", c->sample_fmt, 0);
	/* initialize the resampling context */
	if ((ret = swr_init(ost->swr_ctx)) < 0) {
		fprintf(stderr, "Failed to initialize the resampling context\n");
		exit(1);
	}

	int	nSrcSamples = av_rescale_rnd(swr_get_delay(ost->swr_ctx, 44100) + nb_samples, 48000, 44100, AV_ROUND_UP);
	ost->nSrcSamples = nSrcSamples;

	int nPcmBufferSize = nSrcSamples * 2 * 4;

	char * pPcmBuffer = new char[nPcmBufferSize];
	
	if (!pPcmBuffer)
	{
		exit(1);
	}

	ost->pPcmBuffer = pPcmBuffer;

	ost->nPcmBufferSize = nPcmBufferSize;

	ost->pAudio = NULL;

	nInit = 1;

	return;
}
/* Prepare a 16 bit dummy audio frame of 'frame_size' samples and
* 'nb_channels' channels. */
static AVFrame *get_audio_frame(OutputStream *ost, CAudioCapT & ac)
{
	AVFrame *frame = ost->frame;

	int & nPcmBufferLen = ost->nPcmBufferLen;

	int nReadSize = ost->nPcmBufferSize;

	int nLen = 0;

	//默认为静默音
	memset(ost->pPcmBuffer, 0, ost->nPcmBufferSize);

	while (nPcmBufferLen != nReadSize)
	{
		if (ost->pAudio == NULL)
		{
			ost->pAudio = ac.GetAudio();
			if (ost->pAudio)
			{
				ost->nAudioLeftLen = ost->pAudio->iDataLen;
			}
			continue;
		}
		else
		{
			nLen = nReadSize - nPcmBufferLen;
			if (nLen >= ost->nAudioLeftLen)
			{
				memcpy(ost->pPcmBuffer + nPcmBufferLen, ost->pAudio->pData + (ost->pAudio->iDataLen - ost->nAudioLeftLen), ost->nAudioLeftLen);
				nPcmBufferLen += ost->nAudioLeftLen;
				ost->nAudioLeftLen -= ost->nAudioLeftLen;
				if (ost->pAudio)
				{
					if (ost->pAudio->pData)
					{
						delete[] ost->pAudio->pData;
						ost->pAudio->pData = NULL;
					}
					delete ost->pAudio;
					ost->pAudio = NULL;
				}
				continue;
			}
			else
			{
				memcpy(ost->pPcmBuffer + nPcmBufferLen, ost->pAudio->pData + (ost->pAudio->iDataLen - ost->nAudioLeftLen), nLen);
				nPcmBufferLen += nLen;
				ost->nAudioLeftLen -= nLen;
			}
		}
	}


	const uint8_t *data[1];
	data[0] = (uint8_t*)ost->pPcmBuffer;

	int len = swr_convert(ost->swr_ctx, ost->frame->data, ost->frame->nb_samples, data, ost->nSrcSamples);

	nPcmBufferLen = 0;

	if (len <= 0)
		return NULL;

	frame->pts = ost->next_pts;
	ost->next_pts += frame->nb_samples;
	return frame;
}
/*
* encode one audio frame and send it to the muxer
* return 1 when encoding is finished, 0 otherwise
*/
static int write_audio_frame(AVFormatContext *oc, OutputStream *ost)
{
	AVCodecContext *c;
	AVPacket pkt = { 0 }; // data and size must be 0;
	AVFrame *frame;
	int ret;
	int got_packet;
	int dst_nb_samples;
	av_init_packet(&pkt);
	c = ost->enc;

	frame = get_audio_frame(ost,ac);
	if (!frame)
	{
		exit(1);
	}

	ret = avcodec_encode_audio2(c, &pkt, frame, &got_packet);
	if (ret < 0) {
		//fprintf(stderr, "Error encoding audio frame: %s\n", av_err2str(ret));
		exit(1);
	}
	if (got_packet) {
		ret = write_frame(oc, &c->time_base, ast, &pkt);
		if (ret < 0) {
			exit(1);
		}
	}
	return (frame || got_packet) ? 0 : 1;
}


// 失败返回0
int InitFilter(char* filter_desc, AVFilterGraph ** ppfilter_graph, AVFilterContext** ppfilter_ctx_src_spk, AVFilterContext** ppfilter_ctx_src_mic, AVFilterContext** ppfilter_ctx_sink)
{
	char args_spk[512];
	char* pad_name_spk = "in0";
	char args_mic[512];
	char* pad_name_mic = "in1";

	if (!ppfilter_graph || !ppfilter_ctx_src_spk || !ppfilter_ctx_src_mic || !ppfilter_ctx_sink)
		return 0;

	const AVFilter* filter_src_spk = avfilter_get_by_name("abuffer");
	const AVFilter* filter_src_mic = avfilter_get_by_name("abuffer");
	const AVFilter* filter_sink = avfilter_get_by_name("abuffersink");

	AVFilterInOut* filter_output_spk = avfilter_inout_alloc();
	AVFilterInOut* filter_output_mic = avfilter_inout_alloc();
	AVFilterInOut* filter_input = avfilter_inout_alloc();

	*ppfilter_graph = avfilter_graph_alloc();

	uint64_t channel_layout = 3;

	sprintf_s(args_spk, sizeof(args_spk), "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%I64x",
		1, 44100,
		44100,
		av_get_sample_fmt_name(AV_SAMPLE_FMT_FLTP),
		channel_layout);
	sprintf_s(args_mic, sizeof(args_mic), "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%I64x",
		1,
		44100,
		44100,
		av_get_sample_fmt_name(AV_SAMPLE_FMT_FLTP),
		channel_layout);


	int nRet = 0;
	nRet = avfilter_graph_create_filter(ppfilter_ctx_src_spk, filter_src_spk, pad_name_spk, args_spk, NULL, *ppfilter_graph);
	if (nRet < 0)
	{
		printf("Filter: failed to call avfilter_graph_create_filter -- src spk\n");
		return 0;
	}
	nRet = avfilter_graph_create_filter(ppfilter_ctx_src_mic, filter_src_mic, pad_name_mic, args_mic, NULL, *ppfilter_graph);
	if (nRet < 0)
	{
		printf("Filter: failed to call avfilter_graph_create_filter -- src mic\n");
		return 0;
	}

	nRet = avfilter_graph_create_filter(ppfilter_ctx_sink, filter_sink, "out", NULL, NULL, *ppfilter_graph);
	if (nRet < 0)
	{
		printf("Filter: failed to call avfilter_graph_create_filter -- sink\n");
		return 0;
	}

	enum AVSampleFormat outFormat = AV_SAMPLE_FMT_FLTP;

	nRet = av_opt_set_bin(*ppfilter_ctx_sink, "sample_fmts", (uint8_t*)&outFormat, sizeof(outFormat), AV_OPT_SEARCH_CHILDREN);
	if (nRet < 0)
	{
		printf("Filter: failed to call av_opt_set_bin -- sample_fmts\n");
		return 0;
	}

	uint64_t outLayout = 3;

	nRet = av_opt_set_bin(*ppfilter_ctx_sink, "channel_layouts", (uint8_t*)&outLayout, sizeof(outLayout), AV_OPT_SEARCH_CHILDREN);
	if (nRet < 0)
	{
		printf("Filter: failed to call av_opt_set_bin -- channel_layouts\n");
		return 0;
	}

	int nSampleRate = 44100;

	nRet = av_opt_set_bin(*ppfilter_ctx_sink, "sample_rates", (uint8_t*)&nSampleRate, sizeof(nSampleRate), AV_OPT_SEARCH_CHILDREN);
	if (nRet < 0)
	{
		printf("Filter: failed to call av_opt_set_bin -- sample_rates\n");
		return 0;
	}

	filter_output_spk->name = av_strdup(pad_name_spk);
	filter_output_spk->filter_ctx = *ppfilter_ctx_src_spk;
	filter_output_spk->pad_idx = 0;
	filter_output_spk->next = filter_output_mic;

	filter_output_mic->name = av_strdup(pad_name_mic);
	filter_output_mic->filter_ctx = *ppfilter_ctx_src_mic;
	filter_output_mic->pad_idx = 0;
	filter_output_mic->next = NULL;

	filter_input->name = av_strdup("out");
	filter_input->filter_ctx = *ppfilter_ctx_sink;
	filter_input->pad_idx = 0;
	filter_input->next = NULL;

	AVFilterInOut* filter_outputs[2];
	filter_outputs[0] = filter_output_spk;
	filter_outputs[1] = filter_output_mic;

	nRet = avfilter_graph_parse_ptr(*ppfilter_graph, filter_desc, &filter_input, filter_outputs, NULL);

	if (nRet < 0)
	{
		printf("Filter: failed to call avfilter_graph_parse_ptr\n");
		return 0;
	}

	nRet = avfilter_graph_config(*ppfilter_graph, NULL);
	if (nRet < 0)
	{
		printf("Filter: failed to call avfilter_graph_config\n");
		return 0;
	}

	avfilter_inout_free(&filter_input);
	avfilter_inout_free(filter_outputs);

	return 1;
}


static int write_audio_frame2(AVFormatContext *oc, OutputStream *ost, OutputStream * ost2)
{
	int nRet = 0;
	AVCodecContext *c;
	AVPacket pkt = { 0 }; // data and size must be 0;
	AVFrame *frame1;
	AVFrame * frame2;
	int ret;
	int got_packet = 0;
	int dst_nb_samples;
	av_init_packet(&pkt);
	c = ost->enc;

	frame1 = get_audio_frame(ost,ac);
	if (!frame1)
	{
		goto end;
	}

	frame2 = get_audio_frame(ost2,ac2);
	if (!frame2)
	{
		goto end;
	}

	nRet = av_buffersrc_add_frame(filter_ctx_src_mic, frame1);
	if (nRet < 0) {
		goto end;
	}

	ost->frame = alloc_audio_frame(ost->enc->sample_fmt, ost->enc->channel_layout, ost->enc->sample_rate, ost->enc->frame_size);

	nRet = av_buffersrc_add_frame(filter_ctx_src_spk, frame2);
	if (nRet < 0) {
		goto end;
	}

	ost2->frame = alloc_audio_frame(ost2->enc->sample_fmt, ost2->enc->channel_layout, ost2->enc->sample_rate, ost2->enc->frame_size);
	
	while ((nRet = av_buffersink_get_frame(filter_ctx_sink, frame)) >= 0)
	{
		ret = avcodec_encode_audio2(c, &pkt, frame, &got_packet);
		if (ret < 0) {
			//fprintf(stderr, "Error encoding audio frame: %s\n", av_err2str(ret));
			goto end;
		}
		if (got_packet) {
			ret = write_frame(oc, &c->time_base, ast, &pkt);
			if (ret < 0) {
				goto end;
			}
		}
	}
end:

	return (frame || got_packet) ? 0 : 1;
}






/**************************************************************/
/* video output */
static AVFrame *alloc_picture(enum AVPixelFormat pix_fmt, int width, int height)
{
	AVFrame *picture;
	int ret;
	picture = av_frame_alloc();
	if (!picture)
		return NULL;
	picture->format = pix_fmt;
	picture->width = width;
	picture->height = height;
	/* allocate the buffers for the frame data */
	ret = av_frame_get_buffer(picture, 32);
	if (ret < 0) {
		fprintf(stderr, "Could not allocate frame data.\n");
		exit(1);
	}
	return picture;
}
static void open_video(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg)
{
	int ret;
	AVCodecContext *c = ost->enc;
	AVDictionary *opt = NULL;
	av_dict_copy(&opt, opt_arg, 0);
	/* open the codec */
	ret = avcodec_open2(c, codec, &opt);
	av_dict_free(&opt);
	if (ret < 0) {
		exit(1);
	}
	/* allocate and init a re-usable frame */
	ost->frame = alloc_picture(c->pix_fmt, c->width, c->height);
	if (!ost->frame) {
		fprintf(stderr, "Could not allocate video frame\n");
		exit(1);
	}
	/* If the output format is not YUV420P, then a temporary YUV420P
	* picture is needed too. It is then converted to the required
	* output format. */
	/* copy the stream parameters to the muxer */
	ret = avcodec_parameters_from_context(vst->codecpar, c);
	if (ret < 0) {
		fprintf(stderr, "Could not copy the stream parameters\n");
		exit(1);
	}
}

static AVFrame *get_video_frame(OutputStream *ost)
{
	AVCodecContext *c = ost->enc;

	/* when we pass a frame to the encoder, it may keep a reference to it
	* internally; make sure we do not overwrite it here */
	if (av_frame_make_writable(ost->frame) < 0)
		exit(1);

	if (!ost->sws_ctx)
	{
		ost->sws_ctx = sws_getCachedContext(ost->sws_ctx, ost->frame->width, ost->frame->height, AV_PIX_FMT_BGRA, ost->frame->width, ost->frame->height,AV_PIX_FMT_YUV420P,SWS_BICUBIC, NULL, NULL, NULL);

		if (!ost->sws_ctx)
		return NULL;
	}

	Screen_Data * pScreenData = NULL;

	do
	{
		pScreenData = sc.GetRGB();

	} while (!pScreenData);

	uint8_t *indata[AV_NUM_DATA_POINTERS] = { 0 };
	indata[0] = pScreenData->m_pData;
	int inlinesize[AV_NUM_DATA_POINTERS] = { 0 };
	inlinesize[0] = pScreenData->m_nW * 4;
	sws_scale(ost->sws_ctx, (const uint8_t * const *)indata,
		inlinesize, 0, pScreenData->m_nH, ost->frame->data,
		ost->frame->linesize);

	if (pScreenData)
	{
		if (pScreenData->m_pData)
		{
			delete[] pScreenData->m_pData;
		}

		delete pScreenData;

		pScreenData = NULL;
	}

	ost->frame->pts = ost->next_pts++;
	return ost->frame;
}
/*
* encode one video frame and send it to the muxer
* return 1 when encoding is finished, 0 otherwise
*/
static int write_video_frame(AVFormatContext *oc, OutputStream *ost)
{
	int ret;
	AVCodecContext *c;
	AVFrame *frame;
	int got_packet = 0;
	AVPacket pkt = { 0 };
	c = ost->enc;
	frame = get_video_frame(ost);
	av_init_packet(&pkt);
	/* encode the image */
	ret = avcodec_encode_video2(c, &pkt, frame, &got_packet);
	if (ret < 0) {

		exit(1);
	}
	if (got_packet) {
		ret = write_frame(oc, &c->time_base, vst, &pkt);
	}
	else {
		ret = 0;
	}
	if (ret < 0) {

		exit(1);
	}
	return (frame || got_packet) ? 0 : 1;
}
static void close_stream(AVFormatContext *oc, OutputStream *ost)
{
	if (ost->enc)
		avcodec_free_context(&ost->enc);
	av_frame_free(&ost->frame);
	sws_freeContext(ost->sws_ctx);
	swr_free(&ost->swr_ctx);
	if (ost->pPcmBuffer)
	{
		delete[] ost->pPcmBuffer;
		ost->pPcmBuffer = NULL;
	}
	if (ost->pAudio)
	{
		if (ost->pAudio->pData)
		{
			delete[] ost->pAudio->pData;
			ost->pAudio->pData = NULL;
		}
		delete[] ost->pAudio;
		ost->pAudio = NULL;
	}
}
/**************************************************************/
/* media file output */
int main(int argc, char **argv)
{
	av_register_all();
	avdevice_register_all();
	avfilter_register_all();

	char* filter_desc = "[in0][in1]amix=inputs=2[out]";

	if(! InitFilter(filter_desc, &filter_graph, &filter_ctx_src_spk, &filter_ctx_src_mic, &filter_ctx_sink))
	{
		printf("init filter failed.\n");
		return 0;
	}

	OutputStream video_st = { 0 }, audio_st = { 0 }, audio_st2 = { 0 };
	const char *filename;
	AVOutputFormat *fmt;
	AVFormatContext *oc;
	AVCodec *audio_codec, *video_codec;
	int ret;
	int have_video = 0, have_audio = 0;
	int encode_video = 0, encode_audio = 0;
	AVDictionary *opt = NULL;
	int i;

	//工作模式
	int nMode = 0;

	nMode = atoi(argv[1]);

	if (argc < 3)
	{
		printf("usage:programe.exe mode path\n");
		printf("mode=1 micphone\n");
		printf("mode=2 speaker\n");
		printf("mode=3 mic + spk\n");
		printf("path mp4 save path\n");
		return 0;
	}

	filename = argv[2];

	if(!filename)
	{
		printf("file not empty!\n");
		return 0;
	}

	/* allocate the output media context */
	::CoInitialize(NULL);
	avformat_alloc_output_context2(&oc, NULL, NULL, filename);
	if (!oc) {
		printf("Could not deduce output format from file extension: using MPEG.\n");
		avformat_alloc_output_context2(&oc, NULL, "mpeg", filename);
	}
	if (!oc)
		return 1;
	fmt = oc->oformat;
	/* Add the audio and video streams using the default format codecs
	* and initialize the codecs. */
	if (fmt->video_codec != AV_CODEC_ID_NONE) {
		add_stream(&video_st, oc, &video_codec, fmt->video_codec,1);
		have_video = 1;
		encode_video = 1;
	}
	if (fmt->audio_codec != AV_CODEC_ID_NONE) {
		add_stream(&audio_st, oc, &audio_codec, fmt->audio_codec,2,true);
		if (nMode == 3)
			add_stream(&audio_st2, oc, &audio_codec, fmt->audio_codec, 2,false,&audio_st);
		have_audio = 1;
		encode_audio = 1;
	}
	/* Now that all the parameters are set, we can open the audio and
	* video codecs and allocate the necessary encode buffers. */
	if (have_video)
		open_video(oc, video_codec, &video_st, opt);
	if (have_audio)
	{
		open_audio(oc, audio_codec, &audio_st, opt);

		if (nMode == 3)
		{
			open_audio(oc, audio_codec, &audio_st2, opt);
			frame = alloc_audio_frame(AV_SAMPLE_FMT_FLTP, av_get_default_channel_layout(2), 44100, 1024);
		}
	}

	av_dump_format(oc, 0, filename, 1);
	/* open the output file, if needed */
	if (!(fmt->flags & AVFMT_NOFILE)) {
		ret = avio_open(&oc->pb, filename, AVIO_FLAG_WRITE);
		if (ret < 0) {

			return 1;
		}
	}
	/* Write the stream header, if any. */
	ret = avformat_write_header(oc, &opt);
	if (ret < 0) {

		return 1;
	}

	if (nMode == 1)
		ac.SetDeiveType(CAudioCapT::MICPHONE);
	else if (nMode == 2)
		ac.SetDeiveType(CAudioCapT::SPEAKER);
	else if (nMode == 3)
	{
		ac.SetDeiveType(CAudioCapT::MICPHONE);
		ac2.SetDeiveType(CAudioCapT::SPEAKER);
	}
	else
	{
		printf("mode not right!\n");
		return 0;
	}

	ac.Init();
	ac.Start();

	if (nMode == 3)
	{
		ac2.Init();
		ac2.Start();
	}

	sc.Start();
	while (encode_video || encode_audio) 
	{
		if (_kbhit())
		{
			ac.Stop();

			if (nMode == 3)
			{
				ac2.Stop();
			}

			sc.Stop();
			break;
		}
		/* select the stream to encode */
		if (av_compare_ts(video_st.next_pts, video_st.enc->time_base,audio_st.next_pts, audio_st.enc->time_base) <= 0)
		{
			write_video_frame(oc, &video_st);
			printf("+");			
		}
		else {

			if (nMode == 1 || nMode == 2)
				write_audio_frame(oc, &audio_st);
			else
				write_audio_frame2(oc, &audio_st,&audio_st2);

			printf("-");
		}
	}
	/* Write the trailer, if any. The trailer must be written before you
	* close the CodecContexts open when you wrote the header; otherwise
	* av_write_trailer() may try to use memory that was freed on
	* av_codec_close(). */
	av_write_trailer(oc);
	/* Close each codec. */
	if (have_video)
		close_stream(oc, &video_st);
	if (have_audio)
	{
		close_stream(oc, &audio_st);
		if (nMode == 3)
		{
			close_stream(oc, &audio_st2);
		}
	}
	if (!(fmt->flags & AVFMT_NOFILE))
		/* Close the output file. */
		avio_closep(&oc->pb);
	/* free the stream */
	if (frame)
		av_frame_free(&frame);

	avformat_free_context(oc);
	ac.ClearAudioList();
	sc.ClearList();

	avfilter_graph_free(&filter_graph);

	::CoUninitialize();

	return 0;
}