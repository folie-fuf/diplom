#!/bin/bash
# configure.sh - Кросс-платформенный скрипт настройки ASCII Video Player

set -e  # Выход при ошибке

# Цвета для вывода
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Определение операционной системы
detect_os() {
    OS="unknown"
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        if [[ -f /etc/os-release ]]; then
            . /etc/os-release
            OS="${ID}"
        else
            OS="linux"
        fi
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        OS="macos"
    elif [[ "$OSTYPE" == "cygwin" ]] || [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "win32" ]]; then
        OS="windows"
    elif [[ "$OSTYPE" == "wsl"* ]]; then
        OS="wsl"
    fi
    
    echo -e "${GREEN}Обнаружена система: $OS${NC}"
}

# Проверка зависимостей
check_dependencies() {
    echo -e "\n${BLUE}=== Проверка зависимостей ===${NC}"
    
    local missing_deps=()
    
    # Проверка компилятора
    if ! command -v gcc &> /dev/null && ! command -v clang &> /dev/null; then
        missing_deps+=("gcc/clang")
    fi
    
    # Проверка make
    if ! command -v make &> /dev/null; then
        missing_deps+=("make")
    fi
    
    # Проверка FFmpeg библиотек
    if [[ "$OS" == "linux" ]] || [[ "$OS" == "wsl" ]]; then
        if ! pkg-config --exists libavcodec; then
            missing_deps+=("libavcodec-dev")
        fi
        if ! pkg-config --exists libavformat; then
            missing_deps+=("libavformat-dev")
        fi
        if ! pkg-config --exists libswscale; then
            missing_deps+=("libswscale-dev")
        fi
    fi
    
    # Проверка libjpeg
    if [[ "$OS" == "linux" ]] || [[ "$OS" == "wsl" ]]; then
        if ! pkg-config --exists libjpeg; then
            missing_deps+=("libjpeg-dev")
        fi
    fi
    
    # Проверка аудио систем
    check_audio_dependencies
    
    if [ ${#missing_deps[@]} -eq 0 ]; then
        echo -e "${GREEN}✓ Все зависимости установлены${NC}"
        return 0
    else
        echo -e "${YELLOW}⚠ Отсутствующие зависимости:${NC}"
        printf '  %s\n' "${missing_deps[@]}"
        return 1
    fi
}

# Проверка аудио зависимостей
check_audio_dependencies() {
    echo -e "\n${BLUE}Проверка аудио систем...${NC}"
    
    case "$OS" in
        "linux" | "ubuntu" | "debian" | "fedora" | "wsl")
            # Проверка различных аудио бэкендов
            if command -v ffplay &> /dev/null; then
                echo -e "${GREEN}✓ ffplay найден${NC}"
                AUDIO_BACKEND="ffplay"
            elif command -v mpv &> /dev/null; then
                echo -e "${GREEN}✓ mpv найден${NC}"
                AUDIO_BACKEND="mpv"
            elif command -v mplayer &> /dev/null; then
                echo -e "${GREEN}✓ mplayer найден${NC}"
                AUDIO_BACKEND="mplayer"
            elif command -v cvlc &> /dev/null; then
                echo -e "${GREEN}✓ VLC найден${NC}"
                AUDIO_BACKEND="vlc"
            else
                echo -e "${YELLOW}⚠ Аудио плеер не найден${NC}"
                missing_deps+=("ffplay/mpv/mplayer/vlc")
            fi
            ;;
        "macos")
            if command -v ffplay &> /dev/null; then
                echo -e "${GREEN}✓ ffplay найден${NC}"
                AUDIO_BACKEND="ffplay"
            elif brew list --formula | grep -q mpv; then
                echo -e "${GREEN}✓ mpv найден${NC}"
                AUDIO_BACKEND="mpv"
            else
                echo -e "${YELLOW}⚠ Аудио плеер не найден${NC}"
                missing_deps+=("ffplay/mpv")
            fi
            ;;
        "windows")
            # Проверка Windows
            if command -v ffplay.exe &> /dev/null; then
                echo -e "${GREEN}✓ ffplay найден${NC}"
                AUDIO_BACKEND="ffplay"
            elif where mpv &> /dev/null; then
                echo -e "${GREEN}✓ mpv найден${NC}"
                AUDIO_BACKEND="mpv"
            else
                echo -e "${YELLOW}⚠ Аудио плеер не найден${NC}"
                missing_deps+=("ffplay/mpv")
            fi
            ;;
    esac
}

# Установка зависимостей
install_dependencies() {
    echo -e "\n${BLUE}=== Установка зависимостей ===${NC}"
    
    case "$OS" in
        "ubuntu" | "debian" | "linuxmint" | "pop")
            echo -e "${YELLOW}Установка для Ubuntu/Debian...${NC}"
            sudo apt-get update
            sudo apt-get install -y \
                build-essential \
                libjpeg-dev \
                libavcodec-dev \
                libavformat-dev \
                libavutil-dev \
                libswscale-dev \
                libswresample-dev \
                ffmpeg \
                mpv
            
            if [[ "$OS" == "wsl" ]]; then
                echo -e "${YELLOW}Дополнительная настройка для WSL2...${NC}"
                # Установка PulseAudio для WSL2
                sudo apt-get install -y pulseaudio
                echo "export PULSE_SERVER=tcp:\$(grep nameserver /etc/resolv.conf | awk '{print \$2}')" >> ~/.bashrc
                echo -e "${YELLOW}⚠ Для WSL2 требуется настроить PulseAudio на Windows${NC}"
            fi
            ;;
        
        "fedora" | "centos" | "rhel")
            echo -e "${YELLOW}Установка для Fedora/RHEL...${NC}"
            sudo dnf install -y \
                gcc gcc-c++ make \
                libjpeg-turbo-devel \
                ffmpeg-devel \
                SDL2-devel \
                mpv
            ;;
        
        "arch" | "manjaro")
            echo -e "${YELLOW}Установка для Arch/Manjaro...${NC}"
            sudo pacman -S --needed \
                base-devel \
                jpeg \
                ffmpeg \
                mpv \
                sdl2
            ;;
        
        "macos")
            echo -e "${YELLOW}Установка для macOS...${NC}"
            if ! command -v brew &> /dev/null; then
                echo -e "${RED}Homebrew не установлен. Установите его с:${NC}"
                echo '/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"'
                exit 1
            fi
            brew install \
                jpeg \
                ffmpeg \
                mpv \
                sdl2
            ;;
        
        "windows")
            echo -e "${YELLOW}Установка для Windows...${NC}"
            echo "1. Установите MSYS2: https://www.msys2.org/"
            echo "2. В MSYS2 выполните:"
            echo "   pacman -S mingw-w64-x86_64-toolchain"
            echo "   pacman -S mingw-w64-x86_64-ffmpeg"
            echo "   pacman -S mingw-w64-x86_64-SDL2"
            echo "3. Используйте Makefile.win для сборки"
            exit 0
            ;;
        
        "wsl")
            echo -e "${YELLOW}Установка для WSL...${NC}"
            # Используем Ubuntu/Debian установку
            sudo apt-get update
            sudo apt-get install -y \
                build-essential \
                libjpeg-dev \
                libavcodec-dev \
                libavformat-dev \
                libavutil-dev \
                libswscale-dev \
                libswresample-dev \
                ffmpeg \
                mpv \
                pulseaudio
            
            echo -e "\n${YELLOW}=== Настройка аудио для WSL2 ===${NC}"
            echo "Для работы аудио в WSL2 необходимо:"
            echo "1. Установить PulseAudio на Windows:"
            echo "   https://www.freedesktop.org/wiki/Software/PulseAudio/Ports/Windows/Support/"
            echo "2. Добавить в ~/.bashrc:"
            echo "   export PULSE_SERVER=tcp:\$(grep nameserver /etc/resolv.conf | awk '{print \$2}')"
            echo "3. Запустить PulseAudio на Windows"
            ;;
        
        *)
            echo -e "${RED}Неподдерживаемая система: $OS${NC}"
            echo "Пожалуйста, установите зависимости вручную:"
            echo "  - GCC/Clang компилятор"
            echo "  - Make"
            echo "  - libjpeg"
            echo "  - FFmpeg (libavcodec, libavformat, libswscale)"
            echo "  - Аудио плеер (ffplay, mpv, mplayer или vlc)"
            exit 1
            ;;
    esac
    
    echo -e "${GREEN}✓ Зависимости установлены${NC}"
}

# Генерация конфигурационного файла
generate_config() {
    echo -e "\n${BLUE}=== Генерация конфигурации ===${NC}"
    
    # Создаем config.mk с настройками для текущей системы
    cat > config.mk << EOF
# Автоматически сгенерированный конфигурационный файл
# Система: $OS
# Аудио бэкенд: ${AUDIO_BACKEND:-none}

# Настройки компилятора
CC = gcc
CFLAGS = -Wall -Wextra -O2 -g -I./include
LDFLAGS = -lm -ljpeg -lavcodec -lavformat -lavutil -lswscale -lswresample

# Настройки для разных систем
ifeq (\$(OS),Windows_NT)
    # Windows настройки
    CFLAGS += -DWIN32 -D_WIN32
    LDFLAGS += -lmingw32 -lSDL2main -lSDL2
    TARGET = ascii_viewer.exe
else
    # Unix-like системы
    UNAME_S := \$(shell uname -s)
    ifeq (\$(UNAME_S),Linux)
        CFLAGS += -D_LINUX
        # Проверяем наличие SDL2
        ifneq (\$(shell pkg-config --exists sdl2 && echo 1),)
            CFLAGS += \$(shell pkg-config --cflags sdl2)
            LDFLAGS += \$(shell pkg-config --libs sdl2)
        endif
    endif
    ifeq (\$(UNAME_S),Darwin)
        CFLAGS += -D_MACOS \$(shell sdl2-config --cflags 2>/dev/null)
        LDFLAGS += \$(shell sdl2-config --libs 2>/dev/null)
    endif
    TARGET = ascii_viewer
endif

# Настройки аудио
AUDIO_BACKEND = ${AUDIO_BACKEND:-none}
CFLAGS += -DAUDIO_BACKEND_\"\$(AUDIO_BACKEND)\"

# Пути
SRC_DIR = src
BUILD_DIR = build
EOF
    
    echo -e "${GREEN}✓ Конфигурационный файл создан: config.mk${NC}"
    
    # Создаем заголовочный файл с настройками системы
    cat > include/platform.h << EOF
#ifndef PLATFORM_H
#define PLATFORM_H

// Автоматически определенные настройки платформы
// Система: $OS
// Аудио бэкенд: ${AUDIO_BACKEND:-none}

// Определение операционной системы
#if defined(_WIN32) || defined(_WIN64)
    #define OS_WINDOWS 1
    #define OS_LINUX 0
    #define OS_MACOS 0
    #define OS_WSL 0
#elif defined(__APPLE__) && defined(__MACH__)
    #define OS_WINDOWS 0
    #define OS_LINUX 0
    #define OS_MACOS 1
    #define OS_WSL 0
#elif defined(__linux__)
    #ifdef __WSL__
        #define OS_WINDOWS 0
        #define OS_LINUX 0
        #define OS_MACOS 0
        #define OS_WSL 1
    #else
        #define OS_WINDOWS 0
        #define OS_LINUX 1
        #define OS_MACOS 0
        #define OS_WSL 0
    #endif
#else
    #define OS_WINDOWS 0
    #define OS_LINUX 0
    #define OS_MACOS 0
    #define OS_WSL 0
#endif

// Аудио бэкенды
#define AUDIO_FFPLAY 1
#define AUDIO_MPV 2
#define AUDIO_MPLAYER 3
#define AUDIO_VLC 4
#define AUDIO_SDL2 5

#if defined(AUDIO_BACKEND_ffplay)
    #define AUDIO_BACKEND AUDIO_FFPLAY
#elif defined(AUDIO_BACKEND_mpv)
    #define AUDIO_BACKEND AUDIO_MPV
#elif defined(AUDIO_BACKEND_mplayer)
    #define AUDIO_BACKEND AUDIO_MPLAYER
#elif defined(AUDIO_BACKEND_vlc)
    #define AUDIO_BACKEND AUDIO_VLC
#elif defined(AUDIO_BACKEND_sdl2)
    #define AUDIO_BACKEND AUDIO_SDL2
#else
    #define AUDIO_BACKEND 0
#endif

// Функции для работы с платформой
const char* get_platform_name(void);
const char* get_audio_backend_name(void);
int check_audio_support(void);

#endif // PLATFORM_H
EOF
    
    echo -e "${GREEN}✓ Заголовочный файл платформы создан: include/platform.h${NC}"
}

# Основная функция
main() {
    echo -e "${BLUE}=== Настройка ASCII Video Player ===${NC}"
    echo -e "${YELLOW}Версия: 1.0.0${NC}"
    echo -e "${YELLOW}Автор: Ваш проект${NC}"
    echo ""
    
    # Определяем ОС
    detect_os
    
    # Проверяем зависимости
    if check_dependencies; then
        echo -e "\n${GREEN}✓ Все зависимости уже установлены${NC}"
        read -p "Перегенерировать конфигурацию? (y/N): " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            generate_config
        fi
    else
        echo -e "\n${YELLOW}Не все зависимости установлены.${NC}"
        read -p "Установить автоматически? (Y/n): " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Nn]$ ]]; then
            echo -e "${YELLOW}Пропускаем установку.${NC}"
            echo -e "${RED}Программа может не скомпилироваться или работать некорректно.${NC}"
        else
            install_dependencies
        fi
        generate_config
    fi
    
    echo -e "\n${GREEN}=== Настройка завершена! ===${NC}"
    echo "Для сборки выполните:"
    echo "  make"
    echo ""
    echo "Для очистки:"
    echo "  make clean"
    echo ""
    echo "Для полной пересборки:"
    echo "  make rebuild"
}

# Запуск основной функции
main "$@"