#include "../include/video_processing.h"
#include "../include/display.h"
#include "../include/terminal.h"
#include "../include/utils.h"
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <math.h>

extern volatile sig_atomic_t keep_running;

static double get_current_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

static double get_frame_delay(VideoState* video) {
    if (video->fps > 0) {
        return 1.0 / video->fps;
    }
    return 1.0 / 30.0;
}

double video_get_pts(VideoState* video, AVFrame* frame) {
    double pts = 0;
    if (frame && frame->pts != AV_NOPTS_VALUE) {
        if (video->format_ctx && video->video_stream_index >= 0) {
            AVRational time_base = video->format_ctx->streams[video->video_stream_index]->time_base;
            pts = frame->pts * av_q2d(time_base);
        }
    }
    return pts;
}

double video_sync_adjust(VideoState* video, double pts) {
    double frame_delay = get_frame_delay(video);
    
    if (!video->audio_state.initialized || !video->audio_state.playing) {
        video->skip_next_frame = 0;
        video->repeat_frame = 0;
        return frame_delay;
    }
    
    double audio_time = get_audio_time(&video->audio_state);
    
    double diff = pts - audio_time;
    video->av_diff = diff;
    
    video->skip_next_frame = 0;
    video->repeat_frame = 0;
    
    // Более агрессивная синхронизация для начальных кадров
    double threshold = (video->frame_count < 60) ? 0.02 : VIDEO_SYNC_THRESHOLD;
    
    if (fabs(diff) > threshold) {
        double correction = fmin(fabs(diff) * 0.5, VIDEO_SYNC_MAX_DIFF);
        
        if (diff < 0) {
            // Видео отстает - ускоряемся
            frame_delay -= correction;
            if (frame_delay < 0.001) frame_delay = 0.001;
            
            // При сильном отставании пропускаем кадр
            if (diff < -0.1) {
                video->skip_next_frame = 1;
            }
        } else {
            // Видео опережает - замедляемся
            frame_delay += correction;
            
            // При сильном опережении повторяем кадр
            if (diff > 0.1) {
                video->repeat_frame = 1;
            }
        }
    }
    
    video->last_frame_delay = frame_delay;
    return frame_delay;
}

bool init_video(VideoState* video, const char* filename) {
    memset(video, 0, sizeof(VideoState));
    
    avformat_network_init();
    
    int ret = avformat_open_input(&video->format_ctx, filename, NULL, NULL);
    if (ret != 0) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        fprintf(stderr, "Failed to open file %s: %s\n", filename, errbuf);
        return false;
    }
    
    if (avformat_find_stream_info(video->format_ctx, NULL) < 0) {
        fprintf(stderr, "Failed to find stream info\n");
        avformat_close_input(&video->format_ctx);
        return false;
    }
    
    video->video_stream_index = -1;
    for (unsigned int i = 0; i < video->format_ctx->nb_streams; i++) {
        if (video->format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video->video_stream_index = i;
            break;
        }
    }
    
    if (video->video_stream_index == -1) {
        fprintf(stderr, "No video stream found\n");
        avformat_close_input(&video->format_ctx);
        return false;
    }
    
    AVCodecParameters* video_codec_params = video->format_ctx->streams[video->video_stream_index]->codecpar;
    const AVCodec* video_codec = avcodec_find_decoder(video_codec_params->codec_id);
    if (!video_codec) {
        fprintf(stderr, "Video codec not found\n");
        avformat_close_input(&video->format_ctx);
        return false;
    }
    
    video->video_codec_ctx = avcodec_alloc_context3(video_codec);
    if (!video->video_codec_ctx) {
        fprintf(stderr, "Failed to allocate codec context\n");
        avformat_close_input(&video->format_ctx);
        return false;
    }
    
    if (avcodec_parameters_to_context(video->video_codec_ctx, video_codec_params) < 0) {
        fprintf(stderr, "Failed to copy codec parameters\n");
        avcodec_free_context(&video->video_codec_ctx);
        avformat_close_input(&video->format_ctx);
        return false;
    }
    
    if (avcodec_open2(video->video_codec_ctx, video_codec, NULL) < 0) {
        fprintf(stderr, "Failed to open codec\n");
        avcodec_free_context(&video->video_codec_ctx);
        avformat_close_input(&video->format_ctx);
        return false;
    }
    
    video->video_frame = av_frame_alloc();
    video->rgb_frame = av_frame_alloc();
    
    if (!video->video_frame || !video->rgb_frame) {
        fprintf(stderr, "Failed to allocate frames\n");
        avcodec_free_context(&video->video_codec_ctx);
        avformat_close_input(&video->format_ctx);
        return false;
    }
    
    video->sws_ctx = sws_getContext(
        video->video_codec_ctx->width, 
        video->video_codec_ctx->height, 
        video->video_codec_ctx->pix_fmt,
        video->video_codec_ctx->width, 
        video->video_codec_ctx->height, 
        AV_PIX_FMT_RGB24,
        SWS_BILINEAR, NULL, NULL, NULL
    );
    
    if (!video->sws_ctx) {
        fprintf(stderr, "Failed to create scaling context\n");
        av_frame_free(&video->video_frame);
        av_frame_free(&video->rgb_frame);
        avcodec_free_context(&video->video_codec_ctx);
        avformat_close_input(&video->format_ctx);
        return false;
    }
    
    int buffer_size = av_image_get_buffer_size(AV_PIX_FMT_RGB24, 
                                             video->video_codec_ctx->width, 
                                             video->video_codec_ctx->height, 
                                             1);
    video->video_buffer = (uint8_t*)av_malloc(buffer_size);
    if (!video->video_buffer) {
        fprintf(stderr, "Failed to allocate buffer\n");
        sws_freeContext(video->sws_ctx);
        av_frame_free(&video->video_frame);
        av_frame_free(&video->rgb_frame);
        avcodec_free_context(&video->video_codec_ctx);
        avformat_close_input(&video->format_ctx);
        return false;
    }
    
    av_image_fill_arrays(video->rgb_frame->data, 
                        video->rgb_frame->linesize, 
                        video->video_buffer,
                        AV_PIX_FMT_RGB24, 
                        video->video_codec_ctx->width, 
                        video->video_codec_ctx->height, 
                        1);
    
    AVStream* video_stream = video->format_ctx->streams[video->video_stream_index];
    
    if (video_stream->avg_frame_rate.num > 0 && video_stream->avg_frame_rate.den > 0) {
        video->fps = av_q2d(video_stream->avg_frame_rate);
    } else if (video_stream->r_frame_rate.num > 0 && video_stream->r_frame_rate.den > 0) {
        video->fps = av_q2d(video_stream->r_frame_rate);
    } else {
        video->fps = 30.0;
    }
    
    video->av_diff = 0;
    video->last_pts = 0;
    video->last_frame_delay = get_frame_delay(video);
    video->frame_timer = 0;
    video->video_clock = 0;
    video->frame_count = 0;
    video->skip_next_frame = 0;
    video->repeat_frame = 0;
    
    memset(&video->audio_state, 0, sizeof(AudioState));
    if (has_audio_stream(filename)) {
        if (!init_audio(&video->audio_state, filename)) {
            fprintf(stderr, "Failed to initialize audio\n");
        }
    }
    
    printf("Video info:\n");
    printf("  Size: %dx%d\n", video->video_codec_ctx->width, video->video_codec_ctx->height);
    printf("  FPS: %.2f\n", video->fps);
    printf("  Frame delay: %.1f ms\n", get_frame_delay(video) * 1000);
    printf("  Duration: %.2f sec\n", 
           (double)video->format_ctx->duration / AV_TIME_BASE);
    printf("  Audio: %s\n", video->audio_state.initialized ? "available" : "unavailable");
    
    return true;
}

void close_video(VideoState* video) {
    if (!video) return;
    
    if (video->audio_state.initialized) {
        cleanup_audio(&video->audio_state);
    }
    
    if (video->sws_ctx) {
        sws_freeContext(video->sws_ctx);
        video->sws_ctx = NULL;
    }
    
    if (video->video_buffer) {
        av_free(video->video_buffer);
        video->video_buffer = NULL;
    }
    
    if (video->rgb_frame) {
        av_frame_free(&video->rgb_frame);
    }
    
    if (video->video_frame) {
        av_frame_free(&video->video_frame);
    }
    
    if (video->video_codec_ctx) {
        avcodec_free_context(&video->video_codec_ctx);
    }
    
    if (video->format_ctx) {
        avformat_close_input(&video->format_ctx);
    }
}

void process_video(const char* filename, AppState* state) {
    VideoState video;
    if (!init_video(&video, filename)) {
        fprintf(stderr, "Video initialization failed\n");
        return;
    }
    
    state->is_video = true;
    state->pause_video = false;
    state->playback_speed = 1.0f;
    
    double base_frame_delay = get_frame_delay(&video);
    double video_clock = 0;
    int frames_displayed = 0;
    
    // Получаем PTS первого видео кадра для синхронизации
    double first_video_pts = 0;
    AVPacket first_packet;
    int found_first_pts = 0;
    
    // Читаем первый пакет чтобы получить PTS
    while (!found_first_pts && av_read_frame(video.format_ctx, &first_packet) >= 0) {
        if (first_packet.stream_index == video.video_stream_index) {
            if (avcodec_send_packet(video.video_codec_ctx, &first_packet) == 0) {
                if (avcodec_receive_frame(video.video_codec_ctx, video.video_frame) == 0) {
                    first_video_pts = video_get_pts(&video, video.video_frame);
                    found_first_pts = 1;
                }
            }
        }
        av_packet_unref(&first_packet);
        if (found_first_pts) break;
    }
    
    // Возвращаемся в начало файла
    av_seek_frame(video.format_ctx, -1, 0, AVSEEK_FLAG_BACKWARD);
    avcodec_flush_buffers(video.video_codec_ctx);
    
    printf("First video PTS: %.3f seconds\n", first_video_pts);
    
    // Запускаем аудио с правильной синхронизацией
    if (video.audio_state.initialized && state->audio_enabled) {
        printf("Starting audio with delay: %.3f seconds\n", first_video_pts);
        // Запускаем аудио с задержкой равной PTS первого видео кадра
        if (audio_play(video.audio_state.system, filename, first_video_pts)) {
            video.audio_state.playing = true;
            state->audio_playing = true;
            printf("Audio started successfully\n");
        } else {
            printf("Failed to start audio\n");
        }
    }
    
    // Небольшая задержка перед стартом видео, чтобы аудио успело запуститься
    precise_usleep(50000); // 50ms задержка
    
    double last_frame_time = get_current_time();
    double next_frame_time = last_frame_time;
    
    printf("\nControls:\n");
    printf("  SPACE - pause/resume\n");
    printf("  M - toggle audio\n");
    printf("  R - reset sync\n");
    printf("  > - increase speed\n");
    printf("  < - decrease speed\n");
    printf("  ESC - exit\n");
    printf("\nPlaying...\n");
    printf("\033[s");
    
    AVPacket packet;
    int frames_skipped = 0;
    int frames_repeated = 0;
    
    while (keep_running) {
        if (state->pause_video) {
            if (video.audio_state.initialized && video.audio_state.playing) {
                stop_audio(&video.audio_state);
            }
            precise_usleep(100000);
            handle_user_input(state, &video.video_codec_ctx->width, 
                            &video.video_codec_ctx->height);
            continue;
        }
        
        double current_time = get_current_time();
        
        if (current_time >= next_frame_time) {
            memset(&packet, 0, sizeof(packet));
            
            int ret = av_read_frame(video.format_ctx, &packet);
            if (ret >= 0) {
                if (packet.stream_index == video.video_stream_index) {
                    ret = avcodec_send_packet(video.video_codec_ctx, &packet);
                    if (ret == 0) {
                        ret = avcodec_receive_frame(video.video_codec_ctx, video.video_frame);
                        if (ret == 0) {
                            video.frame_count++;
                            
                            double pts = video_get_pts(&video, video.video_frame);
                            if (pts > 0) {
                                video_clock = pts;
                            } else {
                                video_clock = frames_displayed * base_frame_delay;
                            }
                            
                            double frame_delay;
                            if (video.audio_state.initialized && video.audio_state.playing) {
                                frame_delay = video_sync_adjust(&video, video_clock);
                            } else {
                                frame_delay = base_frame_delay;
                            }
                            
                            int show_frame = 1;
                            
                            if (video.skip_next_frame) {
                                show_frame = 0;
                                frames_skipped++;
                                video.skip_next_frame = 0;
                            } else if (video.repeat_frame) {
                                frames_repeated++;
                                video.repeat_frame = 0;
                            }
                            
                            if (show_frame) {
                                sws_scale(video.sws_ctx,
                                         (const uint8_t* const*)video.video_frame->data,
                                         video.video_frame->linesize,
                                         0,
                                         video.video_codec_ctx->height,
                                         video.rgb_frame->data,
                                         video.rgb_frame->linesize);
                                
                                display_image_incremental(state, video.video_buffer,
                                                       video.video_codec_ctx->width,
                                                       video.video_codec_ctx->height,
                                                       3);
                                
                                frames_displayed++;
                            }
                            
                            frame_delay = frame_delay / state->playback_speed;
                            next_frame_time = current_time + frame_delay;
                            
                            if (frames_displayed % 30 == 0 && frames_displayed > 0) {
                                double audio_time = get_audio_time(&video.audio_state);
                                printf("\033[u\033[K");
                                
                                char sync_indicator = ' ';
                                if (fabs(video_clock - audio_time) < 0.05) {
                                    sync_indicator = '=';
                                } else if (video_clock < audio_time) {
                                    sync_indicator = '<';
                                } else {
                                    sync_indicator = '>';
                                }
                                
                                printf("[%c] Frames: %4d | Video: %6.2fs | Audio: %6.2fs | Diff: %+6.3fs | Delay: %4.0fms | Speed: %.1fx | Skip: %d | Repeat: %d",
                                       sync_indicator,
                                       frames_displayed, 
                                       video_clock, 
                                       audio_time,
                                       video_clock - audio_time,
                                       frame_delay * 1000,
                                       (double)state->playback_speed,
                                       frames_skipped,
                                       frames_repeated);
                                fflush(stdout);
                            }
                        }
                    }
                }
                av_packet_unref(&packet);
            } else {
                if (ret == AVERROR_EOF) {
                    printf("\n=== End of video, looping ===\n");
                    av_seek_frame(video.format_ctx, -1, 0, AVSEEK_FLAG_BACKWARD);
                    avcodec_flush_buffers(video.video_codec_ctx);
                    
                    if (video.audio_state.initialized && state->audio_playing) {
                        stop_audio(&video.audio_state);
                        // При перемотке снова запускаем аудио с правильной задержкой
                        if (audio_play(video.audio_state.system, filename, first_video_pts)) {
                            video.audio_state.playing = true;
                        }
                    }
                    
                    frames_displayed = 0;
                    frames_skipped = 0;
                    frames_repeated = 0;
                    video_clock = 0;
                    last_frame_time = get_current_time();
                    next_frame_time = last_frame_time;
                } else {
                    char errbuf[256];
                    av_strerror(ret, errbuf, sizeof(errbuf));
                    fprintf(stderr, "\nFrame read error: %s\n", errbuf);
                    break;
                }
            }
        }
        
        struct timeval tv = {0, 0};
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        
        if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0) {
            int ch = getchar();
            switch (ch) {
                case ' ':
                    state->pause_video = !state->pause_video;
                    if (video.audio_state.initialized) {
                        if (state->pause_video) {
                            stop_audio(&video.audio_state);
                        } else {
                            // При возобновлении синхронизируем с текущей позиции
                            double current_audio_time = get_audio_time(&video.audio_state);
                            if (current_audio_time > 0) {
                                av_seek_frame(video.format_ctx, -1, 
                                             (int64_t)(current_audio_time * AV_TIME_BASE), 
                                             AVSEEK_FLAG_BACKWARD);
                                avcodec_flush_buffers(video.video_codec_ctx);
                            }
                            start_audio(&video.audio_state);
                            last_frame_time = get_current_time();
                            next_frame_time = last_frame_time;
                        }
                    }
                    break;
                    
                case 'm': case 'M':
                    if (video.audio_state.initialized) {
                        toggle_audio(&video.audio_state);
                        state->audio_playing = video.audio_state.playing;
                    }
                    break;
                    
                case 'r': case 'R':
                    printf("\n=== Resetting sync ===\n");
                    if (video.audio_state.initialized) {
                        stop_audio(&video.audio_state);
                        // Запускаем аудио с правильной задержкой
                        if (audio_play(video.audio_state.system, filename, first_video_pts)) {
                            video.audio_state.playing = true;
                        }
                    }
                    av_seek_frame(video.format_ctx, -1, 0, AVSEEK_FLAG_BACKWARD);
                    avcodec_flush_buffers(video.video_codec_ctx);
                    frames_displayed = 0;
                    frames_skipped = 0;
                    frames_repeated = 0;
                    video_clock = 0;
                    last_frame_time = get_current_time();
                    next_frame_time = last_frame_time;
                    break;
                    
                case '>':
                    state->playback_speed = fmin(2.0, state->playback_speed + 0.1);
                    break;
                    
                case '<':
                    state->playback_speed = fmax(0.5, state->playback_speed - 0.1);
                    break;
                    
                case ESC_KEY:
                    keep_running = 0;
                    break;
                    
                default:
                    handle_user_input(state, &video.video_codec_ctx->width,
                                    &video.video_codec_ctx->height);
                    break;
            }
        }
        
        precise_usleep(1000);
    }
    
    printf("\n\nStopping playback...\n");
    close_video(&video);
}