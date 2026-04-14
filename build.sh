#!/bin/bash

set -e  # Exit on first error
set -u  # Error on use of unset variables

# ============================================
# CONFIGURATION
# ============================================

RUN_TESTS=false
DEPLOY=false
TUNE_HW=false

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
        -tune-hw)
            TUNE_HW=true
            shift
            ;;
        *)
            echo "Unknown flag: $1"
            exit 1
            ;;
    esac
done

if [[ "$TUNE_HW" == true && "$DEPLOY" != true ]]; then
    echo "-tune-hw requires -deploy"
    exit 1
fi

# Binaries under /opt/hft/bin/<name>/<name>
APP_NAMES=("feeder" "trader" "executor" "observer")
# systemd unit names (without .service)
SERVICE_NAMES=("feeder" "trader_btcusdt" "trader_ethusdt" "executor" "observer")
APP_USER="hft-user"
APP_GROUP="hft-group"

# Paths
SRC_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SRC_DIR/.build"
CONFIG_DIR="$SRC_DIR/conf"
INSTALL_DIR="/opt/hft/bin"
INSTALL_CONF_DIR="/opt/hft/conf"
LOGS_DIR="/var/log/hft"
TARGET_SYSTEMD_DIR="/etc/systemd/system"
TARGET_LOGROTATE_DIR="/etc/logrotate.d"

# Configuration files
SYSTEMD_CONFIGS=("feeder.service" "trader_btcusdt.service" "trader_ethusdt.service" "executor.service" "observer.service")
LOGROTATE_CONFIG="hft_logrotate"
APPLY_HW_TUNING_SCRIPT="$SRC_DIR/apply_hw_tuning.sh"

# ============================================
# FUNCTIONS
# ============================================

# Logging
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

# Create application user/group
create_app_user() {
    if ! id "$APP_USER" &>/dev/null; then
        log_info "Creating user $APP_USER..."
        useradd --system --shell /bin/false "$APP_USER"
    fi

    if ! getent group "$APP_GROUP" &>/dev/null; then
        log_info "Creating group $APP_GROUP..."
        groupadd "$APP_GROUP"
        usermod -aG "$APP_GROUP" "$APP_USER"
    fi
}

# Create required directories
create_directories() {
    log_info "Creating directories..."

    for app_name in "${APP_NAMES[@]}"; do
        mkdir -p "$INSTALL_DIR/$app_name"
    done
    mkdir -p "$LOGS_DIR"
    mkdir -p "$INSTALL_CONF_DIR"

    for app_name in "${APP_NAMES[@]}"; do
        chown -R "$APP_USER:$APP_GROUP" "$INSTALL_DIR/$app_name"
        chmod 750 "$INSTALL_DIR/$app_name"
    done
    chown -R "$APP_USER:$APP_GROUP" "$LOGS_DIR"
    chmod 755 "$LOGS_DIR"
    chown root:root "$INSTALL_CONF_DIR"
    chmod 755 "$INSTALL_CONF_DIR"
}

# Copy configuration files
copy_configs() {
    log_info "Copying configuration files..."

    for systemd_config in "${SYSTEMD_CONFIGS[@]}"; do
        if [[ -f "$CONFIG_DIR/$systemd_config" ]]; then
            cp "$CONFIG_DIR/$systemd_config" "$TARGET_SYSTEMD_DIR/"
            chmod 644 "$TARGET_SYSTEMD_DIR/$systemd_config"
            chown root:root "$TARGET_SYSTEMD_DIR/$systemd_config"
            log_info "  → systemd config for $systemd_config copied"
        else
            log_warn "Systemd config for $systemd_config not found: $CONFIG_DIR/$systemd_config"
            exit 1
        fi
    done

    if [[ -f "$CONFIG_DIR/$LOGROTATE_CONFIG" ]]; then
        cp "$CONFIG_DIR/$LOGROTATE_CONFIG" "$TARGET_LOGROTATE_DIR/"
        chmod 644 "$TARGET_LOGROTATE_DIR/$LOGROTATE_CONFIG"
        chown root:root "$TARGET_LOGROTATE_DIR/$LOGROTATE_CONFIG"
        log_info "  → logrotate config copied"
    else
        log_warn "Logrotate config not found: $CONFIG_DIR/$LOGROTATE_CONFIG"
    fi

    if [[ -f "$CONFIG_DIR/ipc.env" ]]; then
        cp "$CONFIG_DIR/ipc.env" "$INSTALL_CONF_DIR/ipc.env"
        chmod 644 "$INSTALL_CONF_DIR/ipc.env"
        chown root:root "$INSTALL_CONF_DIR/ipc.env"
        log_info "  → ipc.env copied to $INSTALL_CONF_DIR/ipc.env"
    else
        log_warn "ipc.env not found: $CONFIG_DIR/ipc.env"
        exit 1
    fi
}

# Configure cron for frequent logrotate runs
deploy_cron_config() {
    log_info "Configuring cron for frequent logrotate runs..."

    CRON_FILE="/etc/cron.d/hft-logrotate"
    STATE_DIR="/var/lib/logrotate"

    mkdir -p "$STATE_DIR"

    # Create cron file
    cat > "$CRON_FILE" << EOF
# HFT Logrotate Configuration
* * * * * root /usr/sbin/logrotate $TARGET_LOGROTATE_DIR/$LOGROTATE_CONFIG --state $STATE_DIR/hft.status 2>&1 | logger -t hft-logrotate
EOF

    chmod 644 "$CRON_FILE"
    chown root:root "$CRON_FILE"
}

# Build applications
build_applications() {
    log_info "Building applications..."

    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    log_info "  → running cmake..."
    cmake "$SRC_DIR" -DCMAKE_BUILD_TYPE=Release

    log_info "  → compiling..."
    make

    # Verify binaries exist
    for app_name in "${APP_NAMES[@]}"; do
        if [[ -f "$BUILD_DIR/$app_name" ]]; then
            log_success "Build of $app_name completed"
        else
            log_error "Binary $app_name not found in $BUILD_DIR"
            exit 1
        fi
    done
}

# Install and start applications
deploy() {
    log_info "Deploying applications..."

    # 1. Stop services
    for service_name in "${SERVICE_NAMES[@]}"; do
        log_info "  → Stopping service $service_name..."
        systemctl stop "$service_name" 2>/dev/null || true
    done

    # 2. Wait 3 seconds for processes to fully terminate
    log_info "  → Waiting for processes to finish..."
    sleep 3

    # 3. Kill any remaining processes
    log_info "  → Force terminating remaining processes..."
    for app_name in "${APP_NAMES[@]}"; do
        pkill -9 -f "$INSTALL_DIR/$app_name/$app_name" 2>/dev/null || true
    done
    sleep 1

    # 4. Copy binaries
    for app_name in "${APP_NAMES[@]}"; do
        log_info "  → Removing old binary $app_name..."
        rm -f "$INSTALL_DIR/$app_name/$app_name" 2>/dev/null || true
        log_info "  → Copying new binary $app_name..."
        cp "$BUILD_DIR/$app_name" "$INSTALL_DIR/$app_name/"
        chown "$APP_USER:$APP_GROUP" "$INSTALL_DIR/$app_name/$app_name"
        chmod 750 "$INSTALL_DIR/$app_name/$app_name"
    done

    # 5. HW tuning (optional)
    if [[ -f "$APPLY_HW_TUNING_SCRIPT" ]]; then
        if [[ "$TUNE_HW" == true ]]; then
            log_info "  → Applying hardware tuning..."
            bash "$APPLY_HW_TUNING_SCRIPT"
        else
            log_info "  → Rolling back hardware tuning..."
            bash "$APPLY_HW_TUNING_SCRIPT" --rollback
        fi
    else
        log_warn "  → $APPLY_HW_TUNING_SCRIPT not found"
    fi

    # 6. Start services
    systemctl daemon-reload

    for service_name in "${SERVICE_NAMES[@]}"; do
        log_info "  → Starting service $service_name..."
        systemctl enable "$service_name"
        systemctl start "$service_name"
    done

    log_success "Deploy completed"
}

do_deploy() {
    log_info "Starting deploy..."
    create_app_user
    create_directories
    copy_configs
    deploy_cron_config
    deploy

    # Allow time for startup
    sleep 3

    # Verify services
    log_info "Verifying deploy..."
    for service_name in "${SERVICE_NAMES[@]}"; do
        if systemctl is-active --quiet "$service_name"; then
            log_success "Service $service_name is up and running"
        else
            log_error "Service $service_name is not running"
            systemctl status "$service_name" --no-pager
        fi
    done
}

run_tests() {
    log_info "Running tests..."
    $BUILD_DIR/run_tests
}

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

trap 'log_error "Script interrupted!"; exit 1' INT TERM

main "$@"
