#!/usr/bin/env bash
# Author: Ryan Hartzell
# Date  : OCT 24 2026
#
# Configure chrony so this device syncs its clock to the Slade, the MLSS LAN
# master NTP clock. Automates the steps in MLSS_TIME_SYNCHRONIZATION_README.
#
# Usage:
#   sudo ./mlss_time_sync_setup.sh [NTP_SERVER_IP]
#
# NTP_SERVER_IP defaults to 192.168.2.10 (the Slade on LAN subnet 2).
# Safe to re-run: existing settings are updated in place, not duplicated.

set -euo pipefail

NTP_SERVER="${1:-192.168.2.10}"
CHRONY_DIR="/etc/chrony"
CHRONY_CONF="${CHRONY_DIR}/chrony.conf"
SOURCES_DIR="${CHRONY_DIR}/sources.d"
SOURCES_FILE="${SOURCES_DIR}/mlss_ntp.sources"

log()  { printf '\n==> %s\n' "$*"; }
warn() { printf '\nWARNING: %s\n' "$*" >&2; }
die()  { printf '\nERROR: %s\n' "$*" >&2; exit 1; }

if [[ $EUID -ne 0 ]]; then
    die "This script must be run as root. Try: sudo $0 $*"
fi

if ! [[ $NTP_SERVER =~ ^[0-9]{1,3}(\.[0-9]{1,3}){3}$ ]]; then
    die "'${NTP_SERVER}' does not look like an IPv4 address."
fi

# 0) Install chrony (needs internet; skipped if already installed so the
#    script still works on airgapped machines that were prepared beforehand)
if command -v chronyd >/dev/null 2>&1; then
    log "chrony is already installed, skipping apt install"
else
    log "Installing chrony"
    apt-get update || warn "apt-get update failed, trying install anyway"
    apt-get install -y chrony \
        || die "Could not install chrony. It must be installed before airgapped use in the MLSS."
fi

[[ -f $CHRONY_CONF ]] || die "${CHRONY_CONF} not found."

# 1-2) Back up the existing configuration before editing it
BACKUP="${CHRONY_CONF}.bak.$(date +%Y%m%d-%H%M%S)"
cp -p "$CHRONY_CONF" "$BACKUP"
log "Backed up ${CHRONY_CONF} to ${BACKUP}"

# 3) Comment out all 'pool' statements (default internet NTP servers)
log "Commenting out 'pool' statements"
sed -i -E 's/^([[:space:]]*)(pool[[:space:]])/\1#\2/' "$CHRONY_CONF"

# Comment out the DHCP sourcedir so DHCP-provided NTP servers are ignored
log "Commenting out 'sourcedir /run/chrony-dhcp'"
sed -i -E 's#^([[:space:]]*)(sourcedir[[:space:]]+/run/chrony-dhcp)#\1\#\2#' "$CHRONY_CONF"

# 4) Ensure chrony reads sources from /etc/chrony/sources.d
if grep -qE "^[[:space:]]*sourcedir[[:space:]]+${SOURCES_DIR}/?[[:space:]]*$" "$CHRONY_CONF"; then
    log "sourcedir ${SOURCES_DIR} already present"
else
    log "Adding 'sourcedir ${SOURCES_DIR}'"
    printf '\n# MLSS: use NTP sources found in %s\nsourcedir %s\n' \
        "$SOURCES_DIR" "$SOURCES_DIR" >> "$CHRONY_CONF"
fi

# 5) Always step the clock if it is off by more than 1 second
log "Setting 'makestep 1 -1'"
if grep -qE '^[[:space:]]*makestep[[:space:]]' "$CHRONY_CONF"; then
    sed -i -E 's/^[[:space:]]*makestep[[:space:]].*/makestep 1 -1/' "$CHRONY_CONF"
else
    printf '\nmakestep 1 -1\n' >> "$CHRONY_CONF"
fi

# 6) Accept sources with a root distance of up to 16 seconds
log "Setting 'maxdistance 16.0'"
if grep -qE '^[[:space:]]*maxdistance[[:space:]]' "$CHRONY_CONF"; then
    sed -i -E 's/^[[:space:]]*maxdistance[[:space:]].*/maxdistance 16.0/' "$CHRONY_CONF"
else
    printf '\nmaxdistance 16.0\n' >> "$CHRONY_CONF"
fi

# 8-9) Point chrony at the Slade
log "Writing ${SOURCES_FILE} (server ${NTP_SERVER})"
mkdir -p "$SOURCES_DIR"
printf 'server %s iburst prefer\n' "$NTP_SERVER" > "$SOURCES_FILE"
chmod 644 "$SOURCES_FILE"

# Catch config mistakes before restarting the daemon
if ! chronyd -p -f "$CHRONY_CONF" >/dev/null 2>&1; then
    cp -p "$BACKUP" "$CHRONY_CONF"
    die "New chrony.conf failed validation; restored ${BACKUP}. Check the file manually."
fi

# 10) Restart chrony, wait for it to reach the Slade, then step the clock
log "Restarting chrony"
systemctl enable chrony >/dev/null 2>&1 || true
systemctl restart chrony 2>/dev/null || systemctl restart chronyd

log "Waiting up to 30s for ${NTP_SERVER} to respond"
if chronyc waitsync 30 0 0 1 >/dev/null 2>&1; then
    echo "Synchronized."
else
    warn "Not yet synchronized with ${NTP_SERVER}. Check that the Slade is reachable on UDP port 123."
fi

chronyc makestep

log "Current NTP sources (only ${NTP_SERVER} should be listed and selected with '*')"
chronyc sources -v

log "Tracking status"
chronyc tracking

log "Done. Compare the time below with the Slade:"
date
