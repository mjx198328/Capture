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

CScreenCapture sc;

CAudioCapT ac;



typedef struct OutputStream {
	AVStream *st;
	AVCodecContext *enc;
	/* pts of the next frame that will be generated */
	int64_t next_pts;
	int samples_count;
	AVFrame *frame;
	AVFrame *tmp_frame;
	float t, tincr, tincr2;
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

static void add_stream(OutputStream *ost, AVFormatContext *oc,AVCodec **codec,enum AVCodecID codec_id)
{
	AVCodecContext *c;
	int i;
	/* find the encoder */
	*codec = avcodec_find_encoder(codec_id);
	if (!(*codec)) {
		fprintf(stderr, "Could not find encoder for '%s'\n",
			avcodec_get_name(codec_id));
		exit(1);
	}
	ost->st = avformat_new_stream(oc, NULL);
	if (!ost->st) {
		fprintf(stderr, "Could not allocate stream\n");
		exit(1);
	}
	ost->st->id = oc->nb_streams - 1;
	c = avcodec_alloc_context3(*codec);
	if (!c) {
		fprintf(stderr, "Could not alloc an encoding context\n");
		exit(1);
	}
	ost->enc = c;
	switch ((*codec)->type) {
	case AVMEDIA_TYPE_AUDIO:
		c->sample_fmt = (*codec)->sample_fmts ?
			(*codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
		c->bit_rate = 64000;
		c->sample_rate = 44100;
		c->channels = 2;
		c->channel_layout = AV_CH_LAYOUT_STEREO;
		ost->st->time_base.den = c->sample_rate;
		ost->st->time_base.num = 1;
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
		ost->st->time_base.den = 15;
		ost->st->time_base.num = 1;
		c->time_base = ost->st->time_base;
		c->gop_size = 12; /* emit one intra frame every twelve frames at most */
		c->pix_fmt = AV_PIX_FMT_YUV420P;
		break;
	default:
		break;
	}
	/* Some formats want stream headers to be separate. */
	if (oc->oformat->flags & AVFMT_GLOBALHEADER)
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
static void open_audio(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg)
{
	AVCodecContext *c;
	int nb_samples;
	int ret;
	AVDictionary *opt = NULL;
	c = ost->enc;
	/* open it */
	av_dict_copy(&opt, opt_arg, 0);
	ret = avcodec_open2(c, codec, &opt);
	av_dict_free(&opt);
	if (ret < 0) {
		exit(1);
	}
	nb_samples = c->frame_size;
	ost->frame = alloc_audio_frame(c->sample_fmt, c->channel_layout,c->sample_rate, nb_samples);
	/* copy the stream parameters to the muxer */
	ret = avcodec_parameters_from_context(ost->st->codecpar, c);
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

	return;
}
/* Prepare a 16 bit dummy audio frame of 'frame_size' samples and
* 'nb_channels' channels. */
static AVFrame *get_audio_frame(OutputStream *ost)
{
	AVFrame *frame = ost->frame;

	int & nPcmBufferLen = ost->nPcmBufferLen;

	int nReadSize = ost->nPcmBufferSize;

	int nLen = 0;

	while (nPcmBufferLen != nReadSize)
	{
		if (ost->pAudio == NULL)
		{
			ost->pAudio = ac.GetAudio();
			if (ost->pAudio)
				ost->nAudioLeftLen = ost->pAudio->iDataLen;
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
	frame = get_audio_frame(ost);
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
		ret = write_frame(oc, &c->time_base, ost->st, &pkt);
		if (ret < 0) {
			exit(1);
		}
	}
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
	ost->tmp_frame = NULL;
	/* copy the stream parameters to the muxer */
	ret = avcodec_parameters_from_context(ost->st->codecpar, c);
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
		ret = write_frame(oc, &c->time_base, ost->st, &pkt);
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
	avcodec_free_context(&ost->enc);
	av_frame_free(&ost->frame);
	av_frame_free(&ost->tmp_frame);
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
	OutputStream video_st = { 0 }, audio_st = { 0 };
	const char *filename;
	AVOutputFormat *fmt;
	AVFormatContext *oc;
	AVCodec *audio_codec, *video_codec;
	int ret;
	int have_video = 0, have_audio = 0;
	int encode_video = 0, encode_audio = 0;
	AVDictionary *opt = NULL;
	int i;

	if (argc < 3)
	{
		printf("usage:programe.exe mode path\n");
		printf("mode=1 micphone\n");
		printf("mode=2 speaker\n");
		printf("path mp4 save path\n");
		return 0;
	}

	filename = argv[2];

	if (!filename)
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
		add_stream(&video_st, oc, &video_codec, fmt->video_codec);
		have_video = 1;
		encode_video = 1;
	}
	if (fmt->audio_codec != AV_CODEC_ID_NONE) {
		add_stream(&audio_st, oc, &audio_codec, fmt->audio_codec);
		have_audio = 1;
		encode_audio = 1;
	}
	/* Now that all the parameters are set, we can open the audio and
	* video codecs and allocate the necessary encode buffers. */
	if (have_video)
		open_video(oc, video_codec, &video_st, opt);
	if (have_audio)
		open_audio(oc, audio_codec, &audio_st, opt);
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

	int nMode = 0;

	nMode = atoi(argv[1]);

	if (nMode == 1)
		ac.SetDeiveType(CAudioCapT::MICPHONE);
	else if (nMode == 2)
		ac.SetDeiveType(CAudioCapT::SPEAKER);
	else
	{
		printf("mode not right!\n");
		return 0;
	}

	ac.Init();
	ac.Start();
	sc.Start();
	while (encode_video || encode_audio) 
	{
		if (_kbhit())
		{
			ac.Stop();
			sc.Stop();
			break;
		}
		/* select the stream to encode */
		if (encode_video &&
			(!encode_audio || av_compare_ts(video_st.next_pts, video_st.enc->time_base,
			audio_st.next_pts, audio_st.enc->time_base) <= 0)) {
			encode_video = !write_video_frame(oc, &video_st);
			printf("+");
			
		}
		else {
			encode_audio = !write_audio_frame(oc, &audio_st);

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
		close_stream(oc, &audio_st);
	if (!(fmt->flags & AVFMT_NOFILE))
		/* Close the output file. */
		avio_closep(&oc->pb);
	/* free the stream */
	avformat_free_context(oc);
	ac.ClearAudioList();
	sc.ClearList();
	::CoUninitialize();

	return 0;
}