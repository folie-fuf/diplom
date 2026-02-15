#ifndef VIDEO_PROCESSING_H
#define VIDEO_PROCESSING_H

#include "app_state.h"
#include "audio.h"  // Добавляем новый аудио заголовок
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>

typedef struct VideoState {
    AVFormatContext* format_ctx;
    AVCodecContext* video_codec_ctx;
    int video_stream_index;
    AVFrame* video_frame;
    AVFrame* rgb_frame;
    struct SwsContext* sws_ctx;
    uint8_t* video_buffer;
    
    double video_time_base;
    double fps;
    double video_clock;
    double frame_timer;
    int64_t frame_count;
    
    AudioState audio_state;  // Встроенное аудио состояние
} VideoState;

void process_video(const char* filename, AppState* state);
bool init_video(VideoState* video, const char* filename);
void close_video(VideoState* video);
bool read_video_frame(VideoState* video, AppState* state);

#endif