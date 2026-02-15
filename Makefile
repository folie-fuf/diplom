# Компилятор
CC = gcc

# Флаги компиляции (добавляем SDL2)
CFLAGS = -Wall -Wextra -O2 -g -I./include $(shell sdl2-config --cflags 2>/dev/null || echo "")
LDFLAGS = -lm -ljpeg -lavcodec -lavformat -lavutil -lswscale -lswresample $(shell sdl2-config --libs 2>/dev/null || echo "-lSDL2") -lpthread

# Директории
SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build

# Исходные файлы
SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/terminal.c \
       $(SRC_DIR)/display.c \
       $(SRC_DIR)/image_processing.c \
       $(SRC_DIR)/video_processing.c \
       $(SRC_DIR)/audio.c \
       $(SRC_DIR)/utils.c

# Объектные файлы (помещаем в build/)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

# Имя исполняемого файла
TARGET = ascii_viewer

# Проверка наличия SDL2
SDL2_CHECK = $(shell pkg-config --exists sdl2 && echo "SDL2_OK")

# Правило по умолчанию
all: $(BUILD_DIR) check_sdl2 $(TARGET)

# Проверка SDL2
check_sdl2:
ifndef SDL2_CHECK
	@echo "=============================================="
	@echo "ВНИМАНИЕ: SDL2 не найден в системе!"
	@echo "Для качественного аудио необходим SDL2."
	@echo "Установите:"
	@echo "  Ubuntu/Debian: sudo apt-get install libsdl2-dev"
	@echo "  Fedora: sudo dnf install SDL2-devel"
	@echo "  macOS: brew install sdl2"
	@echo "=============================================="
	@echo "Продолжаем компиляцию без SDL2..."
	@sleep 2
endif

# Создание директории для объектных файлов
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Сборка исполняемого файла
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

# Компиляция исходных файлов в объектные
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Очистка
clean:
	rm -rf $(BUILD_DIR) $(TARGET)

# Пересборка
rebuild: clean all

# ==================== ЯВНЫЕ ЗАВИСИМОСТИ ====================

# main.c зависит от всех заголовочных файлов
$(BUILD_DIR)/main.o: $(SRC_DIR)/main.c \
                     $(INCLUDE_DIR)/app_state.h \
                     $(INCLUDE_DIR)/terminal.h \
                     $(INCLUDE_DIR)/display.h \
                     $(INCLUDE_DIR)/image_processing.h \
                     $(INCLUDE_DIR)/video_processing.h \
                     $(INCLUDE_DIR)/utils.h

# terminal.c
$(BUILD_DIR)/terminal.o: $(SRC_DIR)/terminal.c \
                         $(INCLUDE_DIR)/terminal.h \
                         $(INCLUDE_DIR)/app_state.h

# display.c
$(BUILD_DIR)/display.o: $(SRC_DIR)/display.c \
                        $(INCLUDE_DIR)/display.h \
                        $(INCLUDE_DIR)/app_state.h

# image_processing.c
$(BUILD_DIR)/image_processing.o: $(SRC_DIR)/image_processing.c \
                                 $(INCLUDE_DIR)/image_processing.h \
                                 $(INCLUDE_DIR)/app_state.h \
                                 $(INCLUDE_DIR)/display.h

# video_processing.c
$(BUILD_DIR)/video_processing.o: $(SRC_DIR)/video_processing.c \
                                 $(INCLUDE_DIR)/video_processing.h \
                                 $(INCLUDE_DIR)/app_state.h \
                                 $(INCLUDE_DIR)/display.h \
                                 $(INCLUDE_DIR)/audio.h \
                                 $(INCLUDE_DIR)/terminal.h \
                                 $(INCLUDE_DIR)/utils.h

# audio.c (теперь зависит от platform.h)
$(BUILD_DIR)/audio.o: $(SRC_DIR)/audio.c \
                      $(INCLUDE_DIR)/audio.h \
                      $(INCLUDE_DIR)/app_state.h \
                      $(INCLUDE_DIR)/platform.h

# Добавьте platform.o
$(BUILD_DIR)/platform.o: $(SRC_DIR)/platform.c \
                         $(INCLUDE_DIR)/platform.h

# utils.c
$(BUILD_DIR)/utils.o: $(SRC_DIR)/utils.c \
                      $(INCLUDE_DIR)/utils.h

# Зависимости заголовочных файлов (для целостности)
$(INCLUDE_DIR)/app_state.h:
$(INCLUDE_DIR)/terminal.h: $(INCLUDE_DIR)/app_state.h
$(INCLUDE_DIR)/display.h: $(INCLUDE_DIR)/app_state.h
$(INCLUDE_DIR)/image_processing.h: $(INCLUDE_DIR)/app_state.h
$(INCLUDE_DIR)/video_processing.h: $(INCLUDE_DIR)/app_state.h
$(INCLUDE_DIR)/audio.h: $(INCLUDE_DIR)/app_state.h $(INCLUDE_DIR)/video_processing.h
$(INCLUDE_DIR)/utils.h:

# ==================== ВСПОМОГАТЕЛЬНЫЕ ЦЕЛИ ====================

# Тестирование
test: all
	@echo "Запуск тестов..."
	@if [ -f "test.jpg" ]; then \
		echo "Тестирование с изображением..."; \
		./$(TARGET) test.jpg; \
	elif [ -f "test.mp4" ]; then \
		echo "Тестирование с видео..."; \
		./$(TARGET) test.mp4; \
	else \
		echo "Тестовые файлы не найдены. Используйте:"; \
		echo "  make test-image  # для тестирования с изображением"; \
		echo "  make test-video  # для тестирования с видео"; \
	fi

test-image: all
	@if [ -f "test.jpg" ]; then \
		./$(TARGET) test.jpg; \
	else \
		echo "Файл test.jpg не найден. Создайте тестовое изображение."; \
	fi

test-video: all
	@if [ -f "test.mp4" ]; then \
		./$(TARGET) test.mp4; \
	else \
		echo "Файл test.mp4 не найден. Создайте тестовое видео."; \
	fi

# Показать информацию о проекте
info:
	@echo "=== ASCII Video/Image Viewer ==="
	@echo "Цель: $(TARGET)"
	@echo "Исходники: $(SRCS)"
	@echo "Объектные файлы: $(OBJS)"
	@echo "Флаги компиляции: $(CFLAGS)"
	@echo "Флаги линковки: $(LDFLAGS)"
	@echo "SDL2 доступен: $(if $(SDL2_CHECK),Да,Нет)"
	@echo ""
	@echo "Использование:"
	@echo "  make          - сборка проекта"
	@echo "  make clean    - очистка"
	@echo "  make rebuild  - полная пересборка"
	@echo "  make test     - запуск тестов"
	@echo "  make info     - информация о проекте"

# Установка зависимостей (для разных дистрибутивов)
install-deps:
	@echo "Выберите вашу систему:"
	@echo "  1) Ubuntu/Debian"
	@echo "  2) Fedora/RHEL"
	@echo "  3) macOS (Homebrew)"
	@echo "  4) Выход"
	@read -p "Выбор [1-4]: " choice; \
	case $$choice in \
		1) sudo apt-get update && sudo apt-get install -y libjpeg-dev libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libswresample-dev ffmpeg libsdl2-dev ;; \
		2) sudo dnf install -y libjpeg-turbo-devel ffmpeg-devel SDL2-devel ;; \
		3) brew update && brew install jpeg ffmpeg sdl2 ;; \
		4) echo "Выход"; exit 0 ;; \
		*) echo "Неверный выбор"; exit 1 ;; \
	esac

install-deps-ubuntu:
	sudo apt-get update
	sudo apt-get install -y \
		libjpeg-dev \
		libavcodec-dev \
		libavformat-dev \
		libavutil-dev \
		libswscale-dev \
		libswresample-dev \
		ffmpeg \
		libsdl2-dev

install-deps-fedora:
	sudo dnf install -y \
		libjpeg-turbo-devel \
		ffmpeg-devel \
		SDL2-devel

install-deps-macos:
	brew update
	brew install \
		jpeg \
		ffmpeg \
		sdl2

# Отладка
debug: CFLAGS += -DDEBUG -ggdb3
debug: clean all

# Профилирование
profile: CFLAGS += -pg
profile: LDFLAGS += -pg
profile: clean all

# Выпускная сборка
release: CFLAGS = -Wall -Wextra -O3 -I./include -DNDEBUG $(shell sdl2-config --cflags 2>/dev/null || echo "")
release: clean $(BUILD_DIR) $(TARGET)

# Статическая линковка (если нужно)
static: LDFLAGS += -static
static: all

# Создание тестовых файлов
create-test-image:
	@echo "Создание тестового изображения (test.jpg)..."
	@convert -size 640x480 gradient:red-blue test.jpg 2>/dev/null || \
	(echo "Установите ImageMagick: sudo apt-get install imagemagick" && exit 1)
	@echo "Тестовое изображение создано: test.jpg"

create-test-video:
	@echo "Создание тестового видео (test.mp4)..."
	@ffmpeg -f lavfi -i testsrc=duration=10:size=640x480:rate=30 -c:v libx264 -pix_fmt yuv420p test.mp4 2>/dev/null || \
	(echo "Установите ffmpeg: sudo apt-get install ffmpeg" && exit 1)
	@echo "Тестовое видео создано: test.mp4"

# Проверка всех зависимостей
check-deps:
	@echo "Проверка зависимостей..."
	@echo -n "libjpeg: "; if pkg-config --exists libjpeg; then echo "OK"; else echo "НЕ НАЙДЕН"; fi
	@echo -n "FFmpeg: "; if pkg-config --exists libavcodec; then echo "OK"; else echo "НЕ НАЙДЕН"; fi
	@echo -n "SDL2: "; if pkg-config --exists sdl2; then echo "OK"; else echo "НЕ НАЙДЕН"; fi
	@echo ""
	@echo "Для установки всех зависимостей выполните: make install-deps"

# Помощь
help:
	@echo "Доступные цели:"
	@echo "  all               - сборка проекта (по умолчанию)"
	@echo "  clean             - очистка сборочных файлов"
	@echo "  rebuild           - полная пересборка"
	@echo "  release           - сборка с оптимизацией"
	@echo "  debug             - сборка с отладочной информацией"
	@echo "  profile           - сборка для профилирования"
	@echo "  static            - статическая линковка"
	@echo ""
	@echo "  test              - запуск тестов"
	@echo "  test-image        - тест с изображением"
	@echo "  test-video        - тест с видео"
	@echo "  create-test-image - создать тестовое изображение"
	@echo "  create-test-video - создать тестовое видео"
	@echo ""
	@echo "  info              - информация о проекте"
	@echo "  check-deps        - проверить зависимости"
	@echo "  install-deps      - установить зависимости (интерактивно)"
	@echo "  install-deps-*    - установить для конкретной ОС"
	@echo "  help              - эта справка"

.PHONY: all clean rebuild test test-image test-video info check_sdl2 \
        install-deps install-deps-ubuntu install-deps-fedora install-deps-macos \
        debug profile release static help create-test-image create-test-video check-deps