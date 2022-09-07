/*
 * Copyright (c) 2001 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * @file
 * audio encoding with libavcodec API example.
 *
 * @example encode_audio.c
 */


#include "g726.h"

typedef struct {
    AVCodec *codec;
    AVCodecContext *dec_ctx;
    AVFrame *decoded_frame;
    AVPacket avpkt;
} ffmpeg_g726ctx;


/* check that a given sample format is supported by the encoder */
static int check_sample_fmt(const AVCodec *codec, enum AVSampleFormat sample_fmt)
{
    const enum AVSampleFormat *p = codec->sample_fmts;

    while (*p != AV_SAMPLE_FMT_NONE) {
        if (*p == sample_fmt)
            return 1;
        p++;
    }
    return 0;
}

/* select layout with the highest channel count */
static int select_channel_layout(const AVCodec *codec, AVChannelLayout *dst)
{
    const AVChannelLayout *p, *best_ch_layout;
    int best_nb_channels   = 0;

    if (!codec->ch_layouts)
        return av_channel_layout_copy(dst, &(AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO);

    p = codec->ch_layouts;
    while (p->nb_channels) {
        int nb_channels = p->nb_channels;

        if (nb_channels > best_nb_channels) {
            best_ch_layout   = p;
            best_nb_channels = nb_channels;
        }
        p++;
    }
    return av_channel_layout_copy(dst, best_ch_layout);
}

AVCodec *pEncodec = NULL;
AVCodecContext *pEnContext = NULL;
AVFrame *pEnFrame = NULL;
AVPacket *pEnPkt = NULL;

int ff_g726_encodec_init(audio_param *attr)
{
	int ret = 0;
	/* find the g726le encodec */
	pEncodec = avcodec_find_encoder(AV_CODEC_ID_ADPCM_G726LE);
	if(NULL == pEncodec){
		fprintf(stderr, "G726LE Codec not found!\n");
		return -1;
	}

	pEnContext = avcodec_alloc_context3(pEncodec);
	if(NULL == pEnContext){
		fprintf(stderr, "Could not allocate audio codec context!\n");
		return -1;
	}

    /* check that the encoder supports s16 pcm input */
    pEnContext->sample_fmt = AV_SAMPLE_FMT_S16;
    if (!check_sample_fmt(pEncodec, pEnContext->sample_fmt)) {
        fprintf(stderr, "Encoder does not support sample format %s",
                av_get_sample_fmt_name(pEnContext->sample_fmt));
        return -1;
    }

    /* select other audio parameters supported by the encoder */
    pEnContext->sample_rate    = attr->sampleRate;
    ret = select_channel_layout(pEncodec, &pEnContext->ch_layout);
    if (ret < 0){
		fprintf(stderr, "Could not select channel layout!\n");
        return -1;
	}

    /* put sample parameters */
    pEnContext->bit_rate = pEnContext->sample_rate * 1 * 16;
    
	/* open it */
    if (avcodec_open2(pEnContext, pEncodec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        return -1;
    }

    /* packet for holding encoded output */
    pEnPkt = av_packet_alloc();
    if (!pEnPkt) {
        fprintf(stderr, "could not allocate the packet\n");
        return -1;
    }

    /* frame containing input raw audio */
    pEnFrame = av_frame_alloc();
    if (!pEnFrame) {
        fprintf(stderr, "Could not allocate audio frame\n");
        return -1;
    }

    pEnFrame->nb_samples     = pEnContext->frame_size;
    pEnFrame->format         = pEnContext->sample_fmt;
    ret = av_channel_layout_copy(&pEnFrame->ch_layout, &pEnContext->ch_layout);
    if (ret < 0)
        return -1;

    /* allocate the data buffers */
    ret = av_frame_get_buffer(pEnFrame, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate audio data buffers\n");
        return -1;
    }
	
	return 0;
}

int32_t ff_g726_encodec_do(int8_t *InAudioData,int8_t *OutAudioData,int32_t DataLen)
{
	printf("func:%s, line:%d\n", __func__, __LINE__);
	int ret = 0;
	/* make sure the frame is writable -- makes a copy if the encoder
	 * kept a reference internally */
	ret = av_frame_make_writable(pEnFrame);
	if (ret < 0){
        fprintf(stderr, "Could not make frame writeable!\n");
		return -1;
	}
	
	printf("func:%s, line:%d\n", __func__, __LINE__);
	memcpy(pEnFrame->data, InAudioData, DataLen);
	printf("func:%s, line:%d\n", __func__, __LINE__);
	//pEnFrame->data = &InAudioData;

    /* send the frame for encoding */
    ret = avcodec_send_frame(pEnContext, pEnFrame);
    if (ret < 0) {
        fprintf(stderr, "Error sending the frame to the encoder\n");
        return -1;
    }

    /* read all the available output packets (in general there may be any
     * number of them */
    while (ret >= 0) {
        ret = avcodec_receive_packet(pEnContext, pEnPkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return -1;
        else if (ret < 0) {
            fprintf(stderr, "Error encoding audio frame\n");
            return -1;
        }

		OutAudioData = pEnPkt->data;
		
        //fwrite(pkt->data, 1, pkt->size, output);
        av_packet_unref(pEnPkt);
    }
	return pEnPkt->size;
}

void ff_g726_encodec_destroy()
{
    av_frame_free(&pEnFrame);
    av_packet_free(&pEnPkt);
    avcodec_free_context(&pEnContext);
}


static int get_format_from_sample_fmt(const char **fmt, enum AVSampleFormat sample_fmt)
{
    int i;
    struct sample_fmt_entry {
        enum AVSampleFormat sample_fmt; const char *fmt_be, *fmt_le;
    } sample_fmt_entries[] = {
        { AV_SAMPLE_FMT_U8,  "u8",    "u8"    },
        { AV_SAMPLE_FMT_S16, "s16be", "s16le" },
        { AV_SAMPLE_FMT_S32, "s32be", "s32le" },
        { AV_SAMPLE_FMT_FLT, "f32be", "f32le" },
        { AV_SAMPLE_FMT_DBL, "f64be", "f64le" },
    };
    *fmt = NULL;

    for (i = 0; i < FF_ARRAY_ELEMS(sample_fmt_entries); i++) {
        struct sample_fmt_entry *entry = &sample_fmt_entries[i];
        if (sample_fmt == entry->sample_fmt) {
            *fmt = AV_NE(entry->fmt_be, entry->fmt_le);
            return 0;
        }
    }

    fprintf(stderr,
            "sample format %s is not supported as output format\n",
            av_get_sample_fmt_name(sample_fmt));
    return -1;
}

AVCodec *pDecodec = NULL;
AVCodecContext *pDeContext = NULL;
//AVCodecParserContext *pDeParser = NULL;
AVFrame *pDeFrame = NULL;
AVPacket *pDePkt = NULL;

int ff_g726_decodec_init(audio_param *attr)
{
#if 1
	printf("func:%s, line:%d\n", __func__, __LINE__);
	//av_init_packet(&pDePkt);
	printf("func:%s, line:%d\n", __func__, __LINE__);
    /* packet for holding encoded output */
    pDePkt = av_packet_alloc();
    if (!pDePkt) {
        fprintf(stderr, "could not allocate the packet\n");
        return -1;
    }
	/* find the MPEG audio decoder */
	pDecodec = avcodec_find_decoder(AV_CODEC_ID_ADPCM_G726LE);
    if (!pDecodec) {
        fprintf(stderr, "Codec not found\n");
		return -1;
    }
/*
    pDeParser = av_parser_init(AV_CODEC_ID_ADPCM_G726LE);
    if (!pDeParser) {
        fprintf(stderr, "Parser not found\n");
		return -1;
    }
*/
	printf("func:%s, line:%d\n", __func__, __LINE__);
    pDeContext = avcodec_alloc_context3(pDecodec);
    if (!pDeContext) {
        fprintf(stderr, "Could not allocate audio codec context\n");
		return -1;
    }
	printf("func:%s, line:%d\n", __func__, __LINE__);

	pDeContext->bits_per_coded_sample = 2;
	pDeContext->channels = 1;
	pDeContext->sample_fmt = AV_SAMPLE_FMT_S16;
	pDeContext->sample_rate = 8000;
	pDeContext->codec_type = AVMEDIA_TYPE_AUDIO;
    pDeContext->bit_rate = pDeContext->sample_rate * pDeContext->bits_per_coded_sample;
#if 0
    /* check that the encoder supports s16 pcm input */
    if (!check_sample_fmt(pDecodec, pDeContext->sample_fmt)) {
        fprintf(stderr, "Encoder does not support sample format %s",
                av_get_sample_fmt_name(pDeContext->sample_fmt));
        return -1;
    }
    int ret = select_channel_layout(pDecodec, &pDeContext->ch_layout);
    if (ret < 0){
		fprintf(stderr, "Could not select channel layout!\n");
        return -1;
	}
#endif
    /* open it */
    if (avcodec_open2(pDeContext, pDecodec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
		return -1;
    }
/*
	if (!(pDeFrame = av_frame_alloc())) {
		fprintf(stderr, "Could not allocate audio frame\n");
		return -1;
	}
*/
	printf("func:%s, line:%d\n", __func__, __LINE__);
#endif
	return 0;
}

int32_t ff_g726_decodec_do(int8_t *InAudioData,int8_t *OutAudioData,int32_t DataLen)
{
    uint8_t *data;
	int ret, data_size = 0;

#if 0
	ret = av_parser_parse2(pDeParser, pDeContext, &pDePkt->data, &pDePkt->size, InAudioData, DataLen, AV_NOPTS_VALUE, AV_NOPTS_VALUE, AV_NOPTS_VALUE);
	if (ret < 0) {
		fprintf(stderr, "Error while parsing\n");
		return -1;
	}
	data      += ret;
	data_size -= ret;
#endif
	
	printf("func:%s, line:%d\n", __func__, __LINE__);
	/* flush the decoder */
	pDePkt->data = &InAudioData;
	//memcpy(pDePkt->data, InAudioData, DataLen);
	printf("func:%s, line:%d\n", __func__, __LINE__);
	pDePkt->size = DataLen;

	printf("func:%s, line:%d\n", __func__, __LINE__);
	if(!pDeFrame){
		printf("func:%s, line:%d\n", __func__, __LINE__);
		if(!(pDeFrame = av_frame_alloc()))
		{
			fprintf(stderr, "Failed to alloc frame\n");
			return -1;
		}
	}
	else
	{
		printf("func:%s, line:%d\n", __func__, __LINE__);
		av_frame_unref(pDeFrame);
		//avcodec_get_frame_defaults(pDeFrame);
	}
	printf("func:%s, line:%d\n", __func__, __LINE__);
	int got_frame = 0;
#if 0
	int len = avcodec_decode_audio4(pDeContext, pDeFrame, &got_frame, pDePkt);
	if(len < 0)
	{
		fprintf(stderr, "Failed to decode frame\n");
		return -1;
	}
#else
	if (pDePkt) {
		ret = avcodec_send_packet(pDeContext, pDePkt);
		// In particular, we don't expect AVERROR(EAGAIN), because we read all
		// decoded frames with avcodec_receive_frame() until done.
		if (ret < 0 && ret != AVERROR_EOF){
	printf("func:%s, line:%d\n", __func__, __LINE__);
			return ret;
		}
	}

	printf("func:%s, line:%d\n", __func__, __LINE__);
	while(ret > 0){
		ret = avcodec_receive_frame(pDeContext, pDeFrame);
		if (ret < 0 && ret != AVERROR(EAGAIN))
			return ret;
		if (ret >= 0)
			got_frame = 1;
		printf("$$$$$$$$$$$$$$$line:%d", __LINE__);
	}
#endif
	if(got_frame)
	{
	printf("func:%s, line:%d\n", __func__, __LINE__);
		/*if a frame has been decoded, output it*/
		data_size = av_samples_get_buffer_size(NULL, pDeContext->channels, pDeFrame->nb_samples, pDeContext->sample_fmt, 1);
		memcpy(OutAudioData, pDeFrame, data_size);
	}



#if 0
	data_size = av_get_bytes_per_sample(pDeContext->sample_fmt);
	if (data_size < 0) {
		/* This should not occur, checking just for paranoia */
		fprintf(stderr, "Failed to calculate data size\n");
		return -1;
	}

	/* send the packet with the compressed data to the decoder */
	ret = avcodec_send_packet(pDeContext, pDePkt);
	if (ret < 0) {
		fprintf(stderr, "Error submitting the packet to the decoder:%d\n", ret);
		return -1;
	}
	printf("func:%s, line:%d\n", __func__, __LINE__);
	
	if(pDeFrame == NULL && ((pDeFrame = av_frame_alloc()) == NULL))
	{
		printf("alloc frame failed \n");
		return -1;
	}
	/* read all the output frames (in general there may be any number of them */
	ret = avcodec_receive_frame(pDeContext, pDeFrame);
	if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
		return -1;
	else if (ret < 0) {
		fprintf(stderr, "Error during decoding\n");
		return -1;
	}

	printf("func:%s, line:%d\n", __func__, __LINE__);

	memcpy(OutAudioData, pDeFrame->data, data_size);
 /* print output pcm infomations, because there have no metadata of pcm */
    sfmt = c->sample_fmt;

    if (av_sample_fmt_is_planar(sfmt)) {
        const char *packed = av_get_sample_fmt_name(sfmt);
        printf("Warning: the sample format the decoder produced is planar "
               "(%s). This example will output the first channel only.\n",
               packed ? packed : "?");
        sfmt = av_get_packed_sample_fmt(sfmt);
    }

    n_channels = c->ch_layout.nb_channels;
    if ((ret = get_format_from_sample_fmt(&fmt, sfmt)) < 0)
        goto end;

    printf("Play the output audio file with the command:\n"
           "ffplay -f %s -ac %d -ar %d %s\n",
           fmt, n_channels, c->sample_rate,
           outfilename);
#endif
	return data_size;
}

void ff_g726_decodec_destroy()
{
	avcodec_free_context(&pDeContext);
	//av_parser_close(pDeParser);
	av_frame_free(&pDeFrame);
	av_packet_free(&pDePkt);
}


