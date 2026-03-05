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

RUN_TESTS=false
DEPLOY=false

while [[ $# -gt 0 ]]; do
    case $1 in
        -tests)
            RUN_TESTS=true
            shift
            ;;
        -deploy)
            DEPLOY=true
            shift
            ;;
        *)
            echo "Неизвестный флаг: $1"
            exit 1
            ;;
    esac
done

# Имя приложения
APP_NAMES=("md_feeder" "trading_engine" "observer")
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
SYSTEMD_CONFIGS=("md_feeder.service" "trading_engine.service" "observer.service")
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

    for app_name in "${APP_NAMES[@]}"; do
        mkdir -p "$INSTALL_DIR/$app_name"
    done
    mkdir -p "$LOGS_DIR"

    # Установка прав
    for app_name in "${APP_NAMES[@]}"; do
        chown -R "$APP_USER:$APP_GROUP" "$INSTALL_DIR/$app_name"
        chmod 750 "$INSTALL_DIR/$app_name"
    done
    chown -R "$APP_USER:$APP_GROUP" "$LOGS_DIR"
    chmod 755 "$LOGS_DIR"
}

# Копирование конфигурационных файлов
copy_configs() {
    log_info "Копирование конфигурационных файлов..."

    # Systemd service
    for systemd_config in "${SYSTEMD_CONFIGS[@]}"; do
        if [[ -f "$CONFIG_DIR/$systemd_config" ]]; then
            cp "$CONFIG_DIR/$systemd_config" "$TARGET_SYSTEMD_DIR/"
            chmod 644 "$TARGET_SYSTEMD_DIR/$systemd_config"
            log_info "  → конфиг systemd для $systemd_config скопирован"
        else
            log_warn "Конфиг systemd для $systemd_config не найден: $CONFIG_DIR/$systemd_config"
            exit 1
        fi
    done

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

# Сборка приложений
build_applications() {
    log_info "Сборка приложений..."

    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    # CMake конфигурация
    log_info "  → запуск cmake..."
    cmake "$SRC_DIR"

    # Компиляция
    log_info "  → компиляция..."
    make

    # Проверка существования бинарников
    for app_name in "${APP_NAMES[@]}"; do
        if [[ -f "$BUILD_DIR/$app_name" ]]; then
            log_success "Сборка $app_name завершена"
        else
            log_error "Бинарник $app_name не найден в $BUILD_DIR"
            exit 1
        fi
    done
}

# Установка и запуск приложений
deploy() {
    log_info "Деплой приложений..."

    # 1. Останавливаем сервисы
    for app_name in "${APP_NAMES[@]}"; do
        log_info "  → Останавливаем сервис $app_name..."
        systemctl stop "$app_name" 2>/dev/null || true
    done

    # 2. Ждем 3 секунды чтобы процессы точно завершились
    log_info "  → Ожидание завершения процессов..."
    sleep 3

    # 3. Убиваем все оставшиеся процессы
    log_info "  → Принудительное завершение оставшихся процессов..."
    for app_name in "${APP_NAMES[@]}"; do
        pkill -9 -f "$INSTALL_DIR/$app_name/$app_name" 2>/dev/null || true
    done
    sleep 1

    # 4. Копируем бинарники
    for app_name in "${APP_NAMES[@]}"; do
        log_info "  → Удаляем старый бинарник $app_name..."
        rm -f "$INSTALL_DIR/$app_name/$app_name" 2>/dev/null || true
        log_info "  → Копируем новый бинарник $app_name..."
        cp "$BUILD_DIR/$app_name" "$INSTALL_DIR/$app_name/"
        chown "$APP_USER:$APP_GROUP" "$INSTALL_DIR/$app_name/$app_name"
        chmod 750 "$INSTALL_DIR/$app_name/$app_name"
    done

    # 5. Запускаем сервисы
    systemctl daemon-reload

    log_info "  → Запускаем md_feeder..."
    systemctl enable md_feeder
    systemctl start md_feeder

    sleep 1

    log_info "  → Запускаем trading_engine..."
    systemctl enable trading_engine
    systemctl start trading_engine

    sleep 1

    log_info "  → Запускаем observer..."
    systemctl enable observer
    systemctl start observer

    log_success "Деплой завершен"
}

do_deploy() {
    log_info "Запуск деплоя..."
    create_app_user
    create_directories
    copy_configs
    deploy_cron_config
    deploy

    # Даем время на запуск
    sleep 3

    # Проверка сервиса
    log_info "Проверка деплоя..."
    for app_name in "${APP_NAMES[@]}"; do
        if systemctl is-active --quiet "$app_name"; then
            log_success "Сервис $app_name запущен и работает"
        else
            log_error "Сервис $app_name не запущен"
            systemctl status "$app_name" --no-pager
        fi
    done
}

run_tests() {
    log_info "Запуск тестов..."
    $BUILD_DIR/run_tests
}

# ============================================
# ОСНОВНОЙ СКРИПТ
# ============================================

main() {
    build_applications

    if [ "$RUN_TESTS" = true ]; then
        run_tests
        exit 0
    fi

    if [ "$DEPLOY" = true ]; then
        do_deploy
        exit 0
    fi
}

# Запуск с обработкой ошибок
trap 'log_error "Скрипт прерван!"; exit 1' INT TERM

main "$@"
