#include "../include/video_processing.h"
#include "../include/display.h"
#include "../include/terminal.h"
#include "../include/utils.h"
#include <time.h>
#include <signal.h>
#include <unistd.h>

extern volatile sig_atomic_t keep_running;

static double get_current_time() {
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

bool init_video(VideoState* video, const char* filename) {
    memset(video, 0, sizeof(VideoState));
    
    // Открываем файл
    if (avformat_open_input(&video->format_ctx, filename, NULL, NULL) != 0) {
        fprintf(stderr, "Не удалось открыть файл: %s\n", filename);
        return false;
    }
    
    // Получаем информацию о потоках
    if (avformat_find_stream_info(video->format_ctx, NULL) < 0) {
        fprintf(stderr, "Не удалось получить информацию о потоках\n");
        avformat_close_input(&video->format_ctx);
        return false;
    }
    
    // Находим видео поток
    video->video_stream_index = -1;
    for (unsigned int i = 0; i < video->format_ctx->nb_streams; i++) {
        if (video->format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video->video_stream_index = i;
            break;
        }
    }
    
    if (video->video_stream_index == -1) {
        fprintf(stderr, "Не найден видео поток\n");
        avformat_close_input(&video->format_ctx);
        return false;
    }
    
    // Настраиваем видео декодер
    AVCodecParameters* video_codec_params = video->format_ctx->streams[video->video_stream_index]->codecpar;
    AVCodec* video_codec = avcodec_find_decoder(video_codec_params->codec_id);
    if (!video_codec) {
        fprintf(stderr, "Не найден видео кодек\n");
        avformat_close_input(&video->format_ctx);
        return false;
    }
    
    video->video_codec_ctx = avcodec_alloc_context3(video_codec);
    if (!video->video_codec_ctx) {
        fprintf(stderr, "Не удалось выделить контекст кодека\n");
        avformat_close_input(&video->format_ctx);
        return false;
    }
    
    if (avcodec_parameters_to_context(video->video_codec_ctx, video_codec_params) < 0) {
        fprintf(stderr, "Не удалось скопировать параметры кодека\n");
        avcodec_free_context(&video->video_codec_ctx);
        avformat_close_input(&video->format_ctx);
        return false;
    }
    
    if (avcodec_open2(video->video_codec_ctx, video_codec, NULL) < 0) {
        fprintf(stderr, "Не удалось открыть кодек\n");
        avcodec_free_context(&video->video_codec_ctx);
        avformat_close_input(&video->format_ctx);
        return false;
    }
    
    // Выделяем кадры
    video->video_frame = av_frame_alloc();
    video->rgb_frame = av_frame_alloc();
    
    if (!video->video_frame || !video->rgb_frame) {
        fprintf(stderr, "Не удалось выделить кадры\n");
        avcodec_free_context(&video->video_codec_ctx);
        avformat_close_input(&video->format_ctx);
        return false;
    }
    
    // Создаем контекст для конвертации цвета
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
        fprintf(stderr, "Не удалось создать контекст масштабирования\n");
        av_frame_free(&video->video_frame);
        av_frame_free(&video->rgb_frame);
        avcodec_free_context(&video->video_codec_ctx);
        avformat_close_input(&video->format_ctx);
        return false;
    }
    
    // Выделяем буфер для RGB изображения
    int buffer_size = av_image_get_buffer_size(AV_PIX_FMT_RGB24, 
                                             video->video_codec_ctx->width, 
                                             video->video_codec_ctx->height, 
                                             1);
    video->video_buffer = (uint8_t*)av_malloc(buffer_size);
    if (!video->video_buffer) {
        fprintf(stderr, "Не удалось выделить буфер\n");
        sws_freeContext(video->sws_ctx);
        av_frame_free(&video->video_frame);
        av_frame_free(&video->rgb_frame);
        avcodec_free_context(&video->video_codec_ctx);
        avformat_close_input(&video->format_ctx);
        return false;
    }
    
    // Заполняем структуру RGB кадра
    if (av_image_fill_arrays(video->rgb_frame->data, 
                           video->rgb_frame->linesize, 
                           video->video_buffer,
                           AV_PIX_FMT_RGB24, 
                           video->video_codec_ctx->width, 
                           video->video_codec_ctx->height, 
                           1) < 0) {
        fprintf(stderr, "Не удалось заполнить RGB кадр\n");
        av_free(video->video_buffer);
        sws_freeContext(video->sws_ctx);
        av_frame_free(&video->video_frame);
        av_frame_free(&video->rgb_frame);
        avcodec_free_context(&video->video_codec_ctx);
        avformat_close_input(&video->format_ctx);
        return false;
    }
    
    // Получаем FPS видео
    AVStream* video_stream = video->format_ctx->streams[video->video_stream_index];
    video->video_time_base = av_q2d(video_stream->time_base);
    
    if (video_stream->avg_frame_rate.num > 0 && video_stream->avg_frame_rate.den > 0) {
        video->fps = av_q2d(video_stream->avg_frame_rate);
    } else if (video_stream->r_frame_rate.num > 0 && video_stream->r_frame_rate.den > 0) {
        video->fps = av_q2d(video_stream->r_frame_rate);
    } else {
        video->fps = 30.0;
    }
    
    // Инициализируем аудио если есть
    if (has_audio_stream(filename)) {
        printf("Инициализация аудио...\n");
        if (!init_audio(&video->audio_state, filename)) {
            fprintf(stderr, "Не удалось инициализировать аудио\n");
        } else {
            printf("Аудио инициализировано успешно\n");
        }
    }
    
    printf("Видео информация:\n");
    printf("  Размер: %dx%d\n", video->video_codec_ctx->width, video->video_codec_ctx->height);
    printf("  FPS: %.2f\n", video->fps);
    printf("  Задержка между кадрами: %.3f мс\n", get_frame_delay(video) * 1000);
    printf("  Аудио: %s\n", video->audio_state.initialized ? "доступно" : "недоступно");
    
    return true;
}

void close_video(VideoState* video) {
    // Очищаем аудио
    if (video->audio_state.initialized) {
        cleanup_audio(&video->audio_state);
    }
    
    // Очищаем видео
    if (video->sws_ctx) {
        sws_freeContext(video->sws_ctx);
    }
    
    if (video->video_buffer) {
        av_free(video->video_buffer);
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

bool read_video_frame(VideoState* video, AppState* state) {
    AVPacket packet;
    packet.data = NULL;
    packet.size = 0;
    
    while (av_read_frame(video->format_ctx, &packet) >= 0) {
        if (packet.stream_index == video->video_stream_index) {
            int ret = avcodec_send_packet(video->video_codec_ctx, &packet);
            av_packet_unref(&packet);
            
            if (ret < 0) continue;
            
            ret = avcodec_receive_frame(video->video_codec_ctx, video->video_frame);
            if (ret == 0) {
                // Конвертируем в RGB
                sws_scale(video->sws_ctx, 
                         (uint8_t const* const*)video->video_frame->data,
                         video->video_frame->linesize, 
                         0, 
                         video->video_codec_ctx->height,
                         video->rgb_frame->data, 
                         video->rgb_frame->linesize);
                
                // Отображаем кадр
                display_image_incremental(state, video->video_buffer, 
                                       video->video_codec_ctx->width, 
                                       video->video_codec_ctx->height, 
                                       3);
                
                return true;
            }
        } else {
            av_packet_unref(&packet);
        }
    }
    
    return false;
}

void process_video(const char* filename, AppState* state) {
    VideoState video;
    if (!init_video(&video, filename)) {
        fprintf(stderr, "Ошибка инициализации видео\n");
        return;
    }
    
    state->is_video = true;
    state->pause_video = false;
    state->playback_speed = 1.0f;
    video.video_clock = 0;
    
    double frame_delay = get_frame_delay(&video);
    printf("Начинаем воспроизведение: %.1f FPS\n", 1.0 / frame_delay);
    
    // Запускаем аудио если оно есть
    if (video.audio_state.initialized && state->audio_enabled) {
        start_audio(&video.audio_state);
        state->audio_playing = true;
        printf("Аудио запущено\n");
    }
    
    double last_frame_time = get_current_time();
    double video_time = 0;
    int frames_displayed = 0;
    
    printf("Управление:\n");
    printf("  SPACE - пауза/продолжить\n");
    printf("  M - вкл/выкл аудио\n");
    printf("  ESC - выход\n");
    printf("\nНачинаем воспроизведение...\n");
    
    while (keep_running) {
        if (state->pause_video) {
            precise_usleep(100000);
            continue;
        }
        
        double current_time = get_current_time();
        double time_since_last_frame = current_time - last_frame_time;
        double time_to_next_frame = frame_delay / state->playback_speed;
        
        // Если пора показывать следующий кадр
        if (time_since_last_frame >= time_to_next_frame) {
            double frame_start = get_current_time();
            
            if (read_video_frame(&video, state)) {
                frames_displayed++;
                video_time += frame_delay;
                video.video_clock = video_time;
                last_frame_time = frame_start;
                
                // Выводим статистику
                if (frames_displayed % 30 == 0) {
                    double audio_time = get_audio_time(&video.audio_state);
                    printf("\rКадр: %4d | Видео: %5.1fс | Аудио: %5.1fс | Разница: %+.3fс",
                           frames_displayed, video_time, audio_time, video_time - audio_time);
                    fflush(stdout);
                }
                
                // Синхронизация видео с аудио
                if (state->audio_playing && video.audio_state.initialized) {
                    double audio_time = video.audio_state.current_time;
                    if (video.audio_state.playing) {
                        audio_time = get_audio_time(&video.audio_state);
                    }
                    double diff = video_time - audio_time;
                    
                    // Если видео отстает или опережает больше чем на 0.1 секунды
                    if (fabs(diff) > 0.1) {
                        // Корректируем скорость воспроизведения
                        if (diff > 0) video_time -= 0.01; // Видео впереди - замедляем
                        else video_time += 0.01; // Видео отстает - ускоряем
                    }
                }
            } else {
                // Конец видео
                printf("\nКонец видео, перемотка в начало...\n");
                av_seek_frame(video.format_ctx, -1, 0, AVSEEK_FLAG_BACKWARD);
                avcodec_flush_buffers(video.video_codec_ctx);
                
                if (state->audio_playing && video.audio_state.initialized) {
                    stop_audio(&video.audio_state);
                    start_audio(&video.audio_state);
                }
                
                video_time = 0;
                last_frame_time = get_current_time();
                frames_displayed = 0;
            }
        } else {
            // Ждем до следующего кадра
            double wait_time = time_to_next_frame - time_since_last_frame;
            if (wait_time > 0) {
                precise_usleep((long)(wait_time * 1000000));
            }
        }
        
        // Обработка пользовательского ввода
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
                            start_audio(&video.audio_state);
                        }
                    }
                    break;
                case 'm': case 'M':
                    if (video.audio_state.initialized) {
                        toggle_audio(&video.audio_state);
                        state->audio_playing = video.audio_state.playing;
                    }
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
    }
    
    printf("\nОстанавливаем воспроизведение...\n");
    
    if (video.audio_state.initialized) {
        cleanup_audio(&video.audio_state);
    }
    
    close_video(&video);
}