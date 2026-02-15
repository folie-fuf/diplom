#ifndef VIDEO_PROCESSING_H
#define VIDEO_PROCESSING_H

#include "app_state.h"
#include "audio.h"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>

// Свои константы синхронизации (не конфликтуют с app_state.h)
#define VIDEO_SYNC_THRESHOLD 0.04      // 40ms - порог для коррекции
#define VIDEO_SYNC_MAX_DIFF 0.1        // 100ms - максимальная коррекция за раз
#define VIDEO_AUDIO_DELAY_BUFFER 0.15  // 150ms - буфер для аудио

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
    
    // Для синхронизации
    double av_diff;
    double last_pts;
    double last_frame_delay;
    int skip_next_frame;  // Флаг пропуска следующего кадра при сильном отставании
    int repeat_frame;      // Флаг повтора текущего кадра
    
    AudioState audio_state;
} VideoState;

bool init_video(VideoState* video, const char* filename);
void close_video(VideoState* video);
bool read_video_frame(VideoState* video, AppState* state);
void process_video(const char* filename, AppState* state);

double video_get_pts(VideoState* video, AVFrame* frame);
double video_sync_adjust(VideoState* video, double pts);

#endif