# Основной Makefile - включает конфигурацию
include config.mk

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
       $(SRC_DIR)/utils.c \
       $(SRC_DIR)/platform.c

# Объектные файлы
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

# Правило по умолчанию
all: configure $(BUILD_DIR) $(TARGET)

# Настройка
configure:
	@if [ ! -f "config.mk" ]; then \
		echo "Конфигурационный файл не найден. Запускаю настройку..."; \
		chmod +x configure.sh && ./configure.sh; \
	fi

# Создание директорий
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Сборка исполняемого файла
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)
	@echo "✅ Сборка завершена: $(TARGET)"
	@echo "Аудио бэкенд: $(AUDIO_BACKEND)"

# Компиляция исходных файлов
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Очистка
clean:
	rm -rf $(BUILD_DIR) $(TARGET)
	@echo "✅ Очистка завершена"

# Пересборка
rebuild: clean all

# Тестирование
test: all
	@if [ -f "test.mp4" ]; then \
		echo "Запуск теста с видео..."; \
		./$(TARGET) test.mp4; \
	elif [ -f "test.jpg" ]; then \
		echo "Запуск теста с изображением..."; \
		./$(TARGET) test.jpg; \
	else \
		echo "Тестовые файлы не найдены."; \
	fi

# Установка
install: all
	@echo "Установка в /usr/local/bin..."
	@sudo cp $(TARGET) /usr/local/bin/
	@echo "✅ Установка завершена"

# Удаление
uninstall:
	@echo "Удаление из /usr/local/bin..."
	@sudo rm -f /usr/local/bin/$(TARGET)
	@echo "✅ Удаление завершено"

# Отладка
debug: CFLAGS += -DDEBUG -ggdb3
debug: rebuild

# Релиз
release: CFLAGS = -Wall -Wextra -O3 -I./include -DNDEBUG
release: rebuild

# Справка
help:
	@echo "Доступные команды:"
	@echo "  make          - сборка проекта"
	@echo "  make clean    - очистка"
	@echo "  make rebuild  - полная пересборка"
	@echo "  make test     - запуск тестов"
	@echo "  make install  - установка в систему"
	@echo "  make uninstall- удаление из системы"
	@echo "  make debug    - отладочная сборка"
	@echo "  make release  - релизная сборка"
	@echo "  make help     - эта справка"
	@echo ""
	@echo "Настройка:"
	@echo "  ./configure.sh - настройка для вашей системы"

.PHONY: all clean rebuild test install uninstall debug release help configure