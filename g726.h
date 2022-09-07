#include <stdio.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "libavcodec/avcodec.h"

#include "libavutil/channel_layout.h"
#include "libavutil/common.h"
#include "libavutil/frame.h"
#include "libavutil/samplefmt.h"
#include "libavutil/mem.h"

typedef struct{
	int sampleRate;
	int widthBit;
	int channel;
	int arg;
}audio_param;

int ff_g726_encodec_init(audio_param *attr);
int32_t ff_g726_encodec_do(int8_t *InAudioData,int8_t *OutAudioData,int32_t DataLen);
void ff_g726_encodec_destroy();
int ff_g726_decodec_init(audio_param *attr);
int32_t ff_g726_decodec_do(int8_t *InAudioData,int8_t *OutAudioData,int32_t DataLen);
void ff_g726_decodec_destroy();







