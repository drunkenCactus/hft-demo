#!/bin/bash
# Installed to /opt/hft/conf/irq-affinity.sh (root, executable).
# Keep ALLOWED_IRQ_CPUS in sync with conf/non-hft-cpus.conf (AllowedCPUs=).

set -u

ALLOWED_IRQ_CPUS="0-3,8-19,24-31"

Log() {
    echo "[irq-affinity] $*" >&2
}

# Disable irqbalance
if systemctl is-enabled irqbalance.service &>/dev/null; then
    systemctl stop irqbalance.service 2>/dev/null || true
    systemctl disable irqbalance.service 2>/dev/null || true
    Log "irqbalance stopped and disabled"
elif systemctl is-active --quiet irqbalance.service 2>/dev/null; then
    systemctl stop irqbalance.service 2>/dev/null || true
    Log "irqbalance stopped"
fi

# Set target CPU set for smp_affinity_list for numeric IRQs
for irq_dir in /proc/irq/*; do
    [[ -d "$irq_dir" ]] || continue
    irq=$(basename "$irq_dir")
    [[ "$irq" =~ ^[0-9]+$ ]] || continue
    list_file="$irq_dir/smp_affinity_list"
    [[ -e "$list_file" ]] || continue
    if ! echo "$ALLOWED_IRQ_CPUS" > "$list_file" 2>/dev/null; then
        Log "warning: could not set smp_affinity_list for IRQ $irq (ignored)"
    fi
done

# Disable RPS and XPS
shopt -s nullglob
for rps in /sys/class/net/*/queues/rx-*/rps_cpus; do
    if ! echo 0 > "$rps" 2>/dev/null; then
        Log "warning: could not disable RPS for $rps (ignored)"
    fi
done
for xps in /sys/class/net/*/queues/tx-*/xps_cpus; do
    if ! echo 0 > "$xps" 2>/dev/null; then
        Log "warning: could not disable XPS for $xps (ignored)"
    fi
done
shopt -u nullglob

Log "done"
exit 0
