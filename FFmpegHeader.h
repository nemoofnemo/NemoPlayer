#pragma once

extern "C" {
#include <stdio.h>
#include <stdlib.h>
#include <libavutil/opt.h>
#include <libavutil/dict.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
#include <libavutil/hwcontext.h>
#include <libavutil/parseutils.h>
}