#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Entrypoint — Adjusts the container user UID/GID to match the workspace
# volume owner, so that 'west init', 'west build' etc. work regardless
# of the host user's UID.
#
# This pattern is used by many devcontainer base images (e.g.
# mcr.microsoft.com/devcontainers/base).

set -eu

USERNAME="${USERNAME:-vscode}"
USER_UID="${USER_UID:-1000}"
USER_GID="${USER_GID:-1000}"
WORKSPACE="${WORKSPACE:-/workspaces/relic-core}"

# ---- Detect the owner of the workspace mount ---------------------------
if [ -d "$WORKSPACE" ]; then
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
    # groupadd fail silently, leaving "vscode" without a group.
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
# The container runtime may rename the user (e.g. "ubuntu" vs "vscode")
# depending on the environment.  We grant NOPASSWD to whichever user
# ends up owning the workspace UID, so sudo works regardless of the
# username.
EFFECTIVE_USER="$(getent passwd "$MOUNT_UID" | cut -d: -f1)"
if [ -n "$EFFECTIVE_USER" ] && [ "$EFFECTIVE_USER" != "$USERNAME" ]; then
    echo "$EFFECTIVE_USER ALL=(ALL) NOPASSWD:ALL" > "/etc/sudoers.d/$EFFECTIVE_USER"
    chmod 440 "/etc/sudoers.d/$EFFECTIVE_USER"
fi

# ---- Bridge Neovim config for effective user ---------------------------
# devcontainer.json mounts host nvim config at /home/$USERNAME/.config/nvim.
# If runtime user differs (e.g. ubuntu), link it into that user's HOME so
# `nvim` still loads LazyVim.
if [ -n "$EFFECTIVE_USER" ]; then
    EFFECTIVE_HOME="$(getent passwd "$EFFECTIVE_USER" | cut -d: -f6)"
    SOURCE_NVIM="/home/$USERNAME/.config/nvim"
    TARGET_NVIM="$EFFECTIVE_HOME/.config/nvim"

    if [ -d "$SOURCE_NVIM" ] || [ -L "$SOURCE_NVIM" ]; then
        mkdir -p "$EFFECTIVE_HOME/.config"
        if [ ! -e "$TARGET_NVIM" ] && [ ! -L "$TARGET_NVIM" ]; then
            ln -s "$SOURCE_NVIM" "$TARGET_NVIM" 2>/dev/null || true
        fi
    fi

    # ---- Bridge .gitconfig for effective user --------------------------
    # devcontainer.json mounts host .gitconfig at /home/$USERNAME/.gitconfig.
    # If runtime user differs (e.g. ubuntu), link it into that user's HOME
    # so git/lazygit pick up the host user.name / user.email.
    SOURCE_GITCONFIG="/home/$USERNAME/.gitconfig"
    TARGET_GITCONFIG="$EFFECTIVE_HOME/.gitconfig"

    if [ -f "$SOURCE_GITCONFIG" ] || [ -L "$SOURCE_GITCONFIG" ]; then
        if [ ! -e "$TARGET_GITCONFIG" ] && [ ! -L "$TARGET_GITCONFIG" ]; then
            ln -s "$SOURCE_GITCONFIG" "$TARGET_GITCONFIG" 2>/dev/null || true
        fi
    fi

    # ---- Bridge .zshrc for effective user ------------------------------
    # devcontainer.json mounts the host .zshrc (readonly) at
    # /home/$USERNAME/.zshrc.  Wrap it (instead of symlinking it) so we can
    # correct host-specific paths that are wrong inside the container --
    # most importantly ZEPHYR_BASE, which the MacBook .zshrc points at
    # /Users/.../zephyrproject/zephyr and would otherwise break west build.
    SOURCE_ZSHRC="/home/$USERNAME/.zshrc"
    TARGET_ZSHRC="$EFFECTIVE_HOME/.zshrc"

    if [ -f "$SOURCE_ZSHRC" ] || [ -L "$SOURCE_ZSHRC" ]; then
        if [ ! -e "$TARGET_ZSHRC" ] && [ ! -L "$TARGET_ZSHRC" ]; then
            {
                echo "[ -f \"$SOURCE_ZSHRC\" ] && source \"$SOURCE_ZSHRC\""
                echo "# Container overrides (host paths may not exist here):"
                echo "export ZEPHYR_BASE=\"$WORKSPACE/zephyr\""
                echo "export SSH_AUTH_SOCK=\"/tmp/ssh-agent.socket\""
            } > "$TARGET_ZSHRC"
            chown "$MOUNT_UID:$MOUNT_GID" "$TARGET_ZSHRC" 2>/dev/null || true
        fi
    fi

    # ---- Bridge Oh My Zsh for effective user ---------------------------
    # The mounted .zshrc sets ZSH="$HOME/.oh-my-zsh".  The framework and
    # its plugins are installed in the image under /home/$USERNAME; link
    # them into the effective user's HOME so the shell loads correctly.
    SOURCE_OMZ="/home/$USERNAME/.oh-my-zsh"
    TARGET_OMZ="$EFFECTIVE_HOME/.oh-my-zsh"

    if [ -d "$SOURCE_OMZ" ] || [ -L "$SOURCE_OMZ" ]; then
        if [ ! -e "$TARGET_OMZ" ] && [ ! -L "$TARGET_OMZ" ]; then
            ln -s "$SOURCE_OMZ" "$TARGET_OMZ" 2>/dev/null || true
        fi
    fi

    # ---- Make zsh the default shell for the effective user ------------
    # If the effective user's login shell is not zsh, switch it so that
    # new shells (and `sudo -u` interactive sessions) start in zsh.
    if [ -x /usr/bin/zsh ] && [ "$(getent passwd "$EFFECTIVE_USER" | cut -d: -f7)" != "/usr/bin/zsh" ]; then
        chsh -s /usr/bin/zsh "$EFFECTIVE_USER" 2>/dev/null || true
    fi
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

# ---- Make the forwarded SSH agent socket accessible ---------------------
# devcontainer.json forwards the host SSH agent socket into the container at
# /tmp/ssh-agent.socket (via `-v`, since `--mount type=bind` hits a Docker
# Desktop socket-mount bug). Docker Desktop maps it as root:root (0660),
# which the unprivileged runtime user cannot open. Fix ownership so git/ssh
# can reach the agent.
SSH_AGENT_SOCKET="/tmp/ssh-agent.socket"
if [ -S "$SSH_AGENT_SOCKET" ]; then
    chown "$MOUNT_UID:$MOUNT_GID" "$SSH_AGENT_SOCKET" 2>/dev/null || \
        chmod 777 "$SSH_AGENT_SOCKET" 2>/dev/null || true
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
