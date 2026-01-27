#!/bin/bash

# ============================================
# DEPLOY SCRIPT FOR HFT SERVER APPLICATION
# Автоматическая сборка и деплой с обновлением "на лету"
# ============================================

set -e  # Выход при первой ошибке
set -u  # Ошибка при использовании необъявленных переменных

# ============================================
# КОНФИГУРАЦИЯ
# ============================================

# Имя приложения
APP_NAME="md_feeder"
APP_USER="hft-user"
APP_GROUP="hft-group"

# Пути
SRC_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SRC_DIR/.build"
CONFIG_DIR="$SRC_DIR/conf"
INSTALL_DIR="/opt/hft/bin"
LOGS_DIR="/var/log/hft"
TARGET_SYSTEMD_DIR="/etc/systemd/system"
TARGET_LOGROTATE_DIR="/etc/logrotate.d"

# Файлы конфигурации
SYSTEMD_CONFIG="$APP_NAME.service"
LOGROTATE_CONFIG="hft_logrotate"

# ============================================
# ФУНКЦИИ
# ============================================

# Логирование
log_info() {
    echo -e "\033[1;34m[INFO]\033[0m $(date '+%Y-%m-%d %H:%M:%S') - $1"
}

log_warn() {
    echo -e "\033[1;33m[WARN]\033[0m $(date '+%Y-%m-%d %H:%M:%S') - $1"
}

log_error() {
    echo -e "\033[1;31m[ERROR]\033[0m $(date '+%Y-%m-%d %H:%M:%S') - $1" >&2
}

log_success() {
    echo -e "\033[1;32m[SUCCESS]\033[0m $(date '+%Y-%m-%d %H:%M:%S') - $1"
}

# Создание пользователя/группы приложения
create_app_user() {
    if ! id "$APP_USER" &>/dev/null; then
        log_info "Создание пользователя $APP_USER..."
        useradd --system --shell /bin/false "$APP_USER"
    fi

    if ! getent group "$APP_GROUP" &>/dev/null; then
        log_info "Создание группы $APP_GROUP..."
        groupadd "$APP_GROUP"
        usermod -aG "$APP_GROUP" "$APP_USER"
    fi
}

# Создание необходимых директорий
create_directories() {
    log_info "Создание директорий..."

    mkdir -p "$INSTALL_DIR/$APP_NAME"
    mkdir -p "$LOGS_DIR"
    mkdir -p "$BUILD_DIR"

    # Установка прав
    chown -R "$APP_USER:$APP_GROUP" "$INSTALL_DIR/$APP_NAME"
    chown -R "$APP_USER:$APP_GROUP" "$LOGS_DIR"
    chmod 750 "$INSTALL_DIR/$APP_NAME"
    chmod 755 "$LOGS_DIR"
}

# Копирование конфигурационных файлов
copy_configs() {
    log_info "Копирование конфигурационных файлов..."

    # Systemd service
    if [[ -f "$CONFIG_DIR/$SYSTEMD_CONFIG" ]]; then
        cp "$CONFIG_DIR/$SYSTEMD_CONFIG" "$TARGET_SYSTEMD_DIR/"
        chmod 644 "$TARGET_SYSTEMD_DIR/$SYSTEMD_CONFIG"
        log_info "  → systemd сервис скопирован"
    else
        log_warn "Файл systemd сервиса не найден: $CONFIG_DIR/$SYSTEMD_CONFIG"
    fi

    # Logrotate config
    if [[ -f "$CONFIG_DIR/$LOGROTATE_CONFIG" ]]; then
        cp "$CONFIG_DIR/$LOGROTATE_CONFIG" "$TARGET_LOGROTATE_DIR/"
        chmod 644 "$TARGET_LOGROTATE_DIR/$LOGROTATE_CONFIG"
        log_info "  → конфиг logrotate скопирован"
    else
        log_warn "Конфиг logrotate не найден: $CONFIG_DIR/$LOGROTATE_CONFIG"
    fi
}

# Настройка cron для учащенного запуска logrotate
deploy_cron_config() {
    log_info "Настройка cron для учащенного запуска logrotate..."

    CRON_FILE="/etc/cron.d/hft-logrotate"
    STATE_DIR="/var/lib/logrotate"

    # Создаем директорию для состояния
    mkdir -p "$STATE_DIR"

    # Создаем cron файл
    cat > "$CRON_FILE" << EOF
# HFT Logrotate Configuration
* * * * * root /usr/sbin/logrotate $TARGET_LOGROTATE_DIR/$LOGROTATE_CONFIG --state $STATE_DIR/hft.status 2>&1 | logger -t hft-logrotate
EOF

    chmod 644 "$CRON_FILE"
}

# Сборка приложения
build_application() {
    log_info "Сборка приложения..."

    cd "$BUILD_DIR"

    # Очистка предыдущей сборки
    if [[ -f "CMakeCache.txt" ]]; then
        log_info "  → очистка предыдущей сборки..."
        make clean 2>/dev/null || true
    fi

    # CMake конфигурация
    log_info "  → запуск cmake..."
    cmake "$SRC_DIR"

    # Компиляция
    log_info "  → компиляция..."
    make

    # Проверка существования бинарника
    if [[ -f "$BUILD_DIR/$APP_NAME" ]]; then
        log_success "Сборка завершена"
    else
        log_error "Бинарник $APP_NAME не найден в $BUILD_DIR"
        exit 1
    fi
}

# Установка и запуск приложения
deploy() {
    log_info "Деплой приложения..."

    # 1. Останавливаем сервис
    log_info "  → Останавливаем сервис $APP_NAME..."
    systemctl stop "$APP_NAME" 2>/dev/null || true

    # 2. Ждем 3 секунды чтобы процесс точно завершился
    log_info "  → Ожидание завершения процессов..."
    sleep 3

    # 3. Убиваем все оставшиеся процессы
    log_info "  → Принудительное завершение оставшихся процессов..."
    pkill -9 -f "$INSTALL_DIR/$APP_NAME/$APP_NAME" 2>/dev/null || true
    sleep 1

    # 4. Удаляем старый бинарник
    log_info "  → Удаляем старый бинарник..."
    rm -f "$INSTALL_DIR/$APP_NAME/$APP_NAME" 2>/dev/null || true

    # 5. Копируем новый (убедитесь что сборка уже выполнена)
    log_info "  → Копируем новый бинарник..."
    cp "$BUILD_DIR/$APP_NAME" "$INSTALL_DIR/$APP_NAME/"
    chown "$APP_USER:$APP_GROUP" "$INSTALL_DIR/$APP_NAME/$APP_NAME"
    chmod 750 "$INSTALL_DIR/$APP_NAME/$APP_NAME"

    # 6. Запускаем сервис
    log_info "  → Запускаем сервис..."
    systemctl daemon-reload
    systemctl start "$APP_NAME"

    log_success "Деплой завершен"
}

# ============================================
# ОСНОВНОЙ СКРИПТ
# ============================================

main() {
    # Основной процесс деплоя
    create_app_user
    create_directories
    copy_configs
    deploy_cron_config
    build_application
    deploy

    # Даем время на запуск
    sleep 3

    # Проверка сервиса
    log_info "Проверка деплоя..."
    if systemctl is-active --quiet "$APP_NAME"; then
        log_success "Сервис $APP_NAME запущен и работает"
    else
        log_error "Сервис $APP_NAME не запущен"
        systemctl status "$APP_NAME" --no-pager
        exit 1
    fi
}

# Запуск с обработкой ошибок
trap 'log_error "Скрипт прерван!"; exit 1' INT TERM

main "$@"
