#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Entrypoint — Adjusts the container user UID/GID to match the workspace
# volume owner, so that 'west init', 'west build' etc. work regardless
# of the host user's UID.
#
# This pattern is used by many devcontainer base images (e.g.
# mcr.microsoft.com/devcontainers/base).
#
# Personal developer tooling (nvim, zsh, gitconfig, ...) is intentionally NOT
# handled here; it lives in a private overlay image built on top of this one.

set -eu

USERNAME="${USERNAME:-devuser}"
USER_UID="${USER_UID:-1000}"
USER_GID="${USER_GID:-1000}"
WORKSPACE="${WORKSPACE:-/workspaces/relic-core}"

# ---- Detect the target UID/GID for the runtime user -------------------
# On macOS Docker Desktop the workspace mount reports root:root, so the real
# host UID/GID must be supplied explicitly via RELIC_HOST_UID/RELIC_HOST_GID
# (set by the personal overlay). On Linux we fall back to the workspace
# owner, which already matches the host user there.
if [ -n "${RELIC_HOST_UID:-}" ] && [ -n "${RELIC_HOST_GID:-}" ]; then
    MOUNT_UID="$RELIC_HOST_UID"
    MOUNT_GID="$RELIC_HOST_GID"
elif [ -d "$WORKSPACE" ]; then
    MOUNT_UID="$(stat -c '%u' "$WORKSPACE")"
    MOUNT_GID="$(stat -c '%g' "$WORKSPACE")"
else
    MOUNT_UID="$USER_UID"
    MOUNT_GID="$USER_GID"
fi

# ---- (Re)create the user with the detected IDs -------------------------
if [ "$MOUNT_GID" -ne "$(id -g "$USERNAME" 2>/dev/null || echo 0)" ]; then
    # Ensure the group exists first: the base image may have shipped a
    # conflicting GID (e.g. "ubuntu" with GID 1000) that made the Dockerfile's
    # groupadd fail silently, leaving "devuser" without a group.
    getent group "$USERNAME" >/dev/null 2>&1 || groupadd "$USERNAME" 2>/dev/null || true
    groupadd --gid "$MOUNT_GID" "$USERNAME" 2>/dev/null || \
        groupmod -o -g "$MOUNT_GID" "$USERNAME" 2>/dev/null || true
fi

if [ "$MOUNT_UID" -ne "$(id -u "$USERNAME" 2>/dev/null || echo 0)" ]; then
    usermod -o -u "$MOUNT_UID" -g "$MOUNT_GID" "$USERNAME" 2>/dev/null || true
fi

# Make sure home directory has correct ownership
chown -R "$MOUNT_UID:$MOUNT_GID" "/home/$USERNAME" 2>/dev/null || true

# ---- Ensure the effective user has passwordless sudo -------------------
# The container runtime may rename the user (e.g. "ubuntu" vs "devuser")
# depending on the environment.  We grant NOPASSWD to whichever user
# ends up owning the workspace UID, so sudo works regardless of the
# username.
EFFECTIVE_USER="$(getent passwd "$MOUNT_UID" | cut -d: -f1)"
if [ -n "$EFFECTIVE_USER" ] && [ "$EFFECTIVE_USER" != "$USERNAME" ]; then
    echo "$EFFECTIVE_USER ALL=(ALL) NOPASSWD:ALL" > "/etc/sudoers.d/$EFFECTIVE_USER"
    chmod 440 "/etc/sudoers.d/$EFFECTIVE_USER"
fi

# Always ensure $USERNAME has passwordless sudo, even if an earlier
# usermod/groupmod operation failed or the Dockerfile-created sudoers
# file was lost (e.g. container restart without full rebuild).
if ! grep -qs "^${USERNAME}" /etc/sudoers.d/"${USERNAME}" 2>/dev/null; then
    echo "$USERNAME ALL=(ALL) NOPASSWD:ALL" > "/etc/sudoers.d/$USERNAME"
    chmod 440 "/etc/sudoers.d/$USERNAME"
fi

# ---- Fix ownership of Zephyr SDK (installed as root) -------------------
if [ -d "$ZEPHYR_SDK_INSTALL_DIR" ]; then
    chown -R "$MOUNT_UID:$MOUNT_GID" "$ZEPHYR_SDK_INSTALL_DIR" 2>/dev/null || true
fi

# ---- Pre-fetch ESP32 blobs (idempotent) --------------------------------
# Runs before every command so CI docker run --rm also gets blobs without
# needing an explicit CI step.  If the west workspace isn't initialized
# yet the script is a no-op.
BLOB_SCRIPT="/workspaces/relic-core/tools/download-blobs.sh"
if [ -f "$BLOB_SCRIPT" ]; then
    bash "$BLOB_SCRIPT" 2>/dev/null || true
fi

# ---- Drop privileges and execute the requested command -----------------
# We run as root; drop to the adjusted user before exec.
# -E  preserves env vars (ZEPHYR_BASE, SDK path, toolchain variant…)
# -H  resets HOME to the target user's home (avoids west reading
#     /root/.westconfig as a non-root user).
# If no command is supplied by the runtime, keep the container alive.
if [ "$#" -eq 0 ]; then
    set -- sleep infinity
fi

exec sudo -E -H -u "$USERNAME" "$@"
