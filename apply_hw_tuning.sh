#!/bin/bash
# Reads profile from conf/hw_tuning (next to repo root). INSTALL_CONF_DIR must match build.sh.
#   sudo bash apply_hw_tuning.sh
#   sudo bash apply_hw_tuning.sh --rollback
set -euo pipefail

if [[ "${EUID:-0}" -ne 0 ]]; then
    echo "apply_hw_tuning.sh: must run as root" >&2
    exit 1
fi

# Source dirs
SRC_DIR="$(cd "$(dirname "$0")" && pwd)"
CONFIG_DIR="$SRC_DIR/conf"
HW_TUNING_SRC_DIR="$CONFIG_DIR/hw_tuning"

# Target dirs
INSTALL_CONF_DIR="/opt/hft/conf"
TARGET_SYSTEMD_DIR="/etc/systemd/system"

# Files
NON_HFT_CONF="non-hft-cpus.conf"
HFT_SLICE_UNIT="hft.slice"
IRQ_AFFINITY_SCRIPT="irq-affinity.sh"
IRQ_AFFINITY_SERVICE="irq-affinity.service"

Log() {
    echo "[apply_hw_tuning] $*" >&2
}

Die() {
    echo "apply_hw_tuning.sh: $*" >&2
    exit 1
}

RequireTuningSrcDir() {
    [[ -d "$HW_TUNING_SRC_DIR" ]] || Die "expected directory $HW_TUNING_SRC_DIR"
}

RequireFullProfile() {
    RequireTuningSrcDir
    for name in \
        "$HFT_SLICE_UNIT" \
        "$IRQ_AFFINITY_SCRIPT" \
        "$IRQ_AFFINITY_SERVICE" \
        "$NON_HFT_CONF"
    do
        [[ -f "$HW_TUNING_SRC_DIR/$name" ]] || Die "missing $HW_TUNING_SRC_DIR/$name"
    done
}

InstallOrRemoveServiceOverrides() {
    # $1 = install | remove
    [[ -d "$HW_TUNING_SRC_DIR/overrides" ]] || return 0
    shopt -s nullglob
    for f in "$HW_TUNING_SRC_DIR/overrides"/*.conf; do
        unit=$(basename "$f" .conf)
        drop="$TARGET_SYSTEMD_DIR/${unit}.service.d/$(basename "$f")"
        if [[ "$1" == install ]]; then
            mkdir -p "$TARGET_SYSTEMD_DIR/${unit}.service.d"
            cp "$f" "$drop"
            chmod 644 "$drop"
            chown root:root "$drop"
        else
            rm -f "$drop"
        fi
    done
    shopt -u nullglob
}

InstallSliceAndIrqUnits() {
    Log "installing $HFT_SLICE_UNIT and $IRQ_AFFINITY_SERVICE..."
    cp "$HW_TUNING_SRC_DIR/$HFT_SLICE_UNIT" "$TARGET_SYSTEMD_DIR/$HFT_SLICE_UNIT"
    chmod 644 "$TARGET_SYSTEMD_DIR/$HFT_SLICE_UNIT"
    chown root:root "$TARGET_SYSTEMD_DIR/$HFT_SLICE_UNIT"

    cp "$HW_TUNING_SRC_DIR/$IRQ_AFFINITY_SCRIPT" "$INSTALL_CONF_DIR/$IRQ_AFFINITY_SCRIPT"
    chmod 750 "$INSTALL_CONF_DIR/$IRQ_AFFINITY_SCRIPT"
    chown root:root "$INSTALL_CONF_DIR/$IRQ_AFFINITY_SCRIPT"

    cp "$HW_TUNING_SRC_DIR/$IRQ_AFFINITY_SERVICE" "$TARGET_SYSTEMD_DIR/$IRQ_AFFINITY_SERVICE"
    chmod 644 "$TARGET_SYSTEMD_DIR/$IRQ_AFFINITY_SERVICE"
    chown root:root "$TARGET_SYSTEMD_DIR/$IRQ_AFFINITY_SERVICE"
}

InstallNonHftSliceLimits() {
    mkdir -p "$TARGET_SYSTEMD_DIR/system.slice.d" "$TARGET_SYSTEMD_DIR/user.slice.d"
    cp "$HW_TUNING_SRC_DIR/$NON_HFT_CONF" "$TARGET_SYSTEMD_DIR/system.slice.d/$NON_HFT_CONF"
    cp "$HW_TUNING_SRC_DIR/$NON_HFT_CONF" "$TARGET_SYSTEMD_DIR/user.slice.d/$NON_HFT_CONF"
    chmod 644 \
        "$TARGET_SYSTEMD_DIR/system.slice.d/$NON_HFT_CONF" \
        "$TARGET_SYSTEMD_DIR/user.slice.d/$NON_HFT_CONF"
    chown root:root \
        "$TARGET_SYSTEMD_DIR/system.slice.d/$NON_HFT_CONF" \
        "$TARGET_SYSTEMD_DIR/user.slice.d/$NON_HFT_CONF"
}

StartIrqAffinity() {
    systemctl daemon-reload
    Log "enabling irq-affinity.service..."
    systemctl enable irq-affinity.service
    if ! systemctl start irq-affinity.service; then
        Log "warning: irq-affinity.service start failed (journalctl -u irq-affinity.service)"
    fi
    if systemctl is-active --quiet irq-affinity.service; then
        Log "irq-affinity.service is active (oneshot, RemainAfterExit=yes)"
    else
        Log "warning: irq-affinity.service is not active"
    fi
}

ApplyHwTuning() {
    RequireFullProfile

    InstallSliceAndIrqUnits

    Log "profile: $HW_TUNING_SRC_DIR"

    InstallNonHftSliceLimits
    InstallOrRemoveServiceOverrides install

    StartIrqAffinity
    Log "done"
}

RollbackHwTuning() {
    RequireTuningSrcDir

    Log "rollback: removing HW tuning (install root: $INSTALL_CONF_DIR)..."

    rm -f "$TARGET_SYSTEMD_DIR/system.slice.d/$NON_HFT_CONF"
    rm -f "$TARGET_SYSTEMD_DIR/user.slice.d/$NON_HFT_CONF"

    InstallOrRemoveServiceOverrides remove

    systemctl disable irq-affinity.service 2>/dev/null || true
    systemctl stop irq-affinity.service 2>/dev/null || true

    rm -f "$TARGET_SYSTEMD_DIR/$IRQ_AFFINITY_SERVICE"
    rm -f "$INSTALL_CONF_DIR/$IRQ_AFFINITY_SCRIPT"
    rm -f "$TARGET_SYSTEMD_DIR/$HFT_SLICE_UNIT"

    systemctl daemon-reload
    Log "rollback done"
}

case "${1:-}" in
    --rollback)
        [[ $# -le 1 ]] || Die "usage: $0 [--rollback]"
        RollbackHwTuning
        ;;
    "")
        ApplyHwTuning
        ;;
    *)
        Die "usage: $0 [--rollback]"
        ;;
esac
