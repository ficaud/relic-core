# SPDX-License-Identifier: GPL-3.0-or-later

# Devcontainer for Zephyr RTOS development on ESP32-S3-DevKitC-1
# Includes: Zephyr SDK (xtensa-esp32s3 toolchain), west, esptool,
#           Emscripten SDK (WASM demo), clang-format, clangd, opencode

FROM ubuntu:24.04

LABEL description="Zephyr RTOS dev environment for ESP32-S3-DevKitC-1"

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

# ------------------------------------------------------------------
# 1. System dependencies
# ------------------------------------------------------------------
RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake \
    ninja-build \
    gperf \
    ccache \
    dfu-util \
    device-tree-compiler \
    make \
    file \
    xz-utils \
    python3 \
    python3-dev \
    python3-pip \
    python3-setuptools \
    python3-tk \
    python3-wheel \
    python3-venv \
    gcc \
    g++ \
    gcovr \
    git \
    wget \
    curl \
    lsb-release \
    sudo \
    unzip \
    doxygen \
    clang-format \
    clangd \
    libsdl2-dev \
    libmagic1 \
    lz4 \
    lzop \
    zstd \
    net-tools \
    openssh-client \
    screen \
    socat \
    libgtest-dev \
    libgmock-dev \
    ripgrep \
    zsh \
    lsof \
    fd-find \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# ------------------------------------------------------------------
# 2. Python packages
# ------------------------------------------------------------------
RUN pip3 install --no-cache-dir --break-system-packages \
    west \
    pyelftools \
    esptool \
    anytree \
    pykwalify \
    python-magic \
    psutil \
    pyyaml \
    requests \
    jsonschema

# ------------------------------------------------------------------
# 3. Zephyr SDK (xtensa-esp32s3 toolchain + cmake scaffolding)
# ------------------------------------------------------------------
# The SDK 1.0.1 _gnu bundle (~2 GB) is unreliable to download in CI.
# We use a two-step approach:
#   1. The full _gnu bundle for the small cmake scaffolding (< 20 MB)
#      (cmake/ comes first in the tar, so extraction is fast)
#   2. The individual ESP32 and ESP32-S3 toolchain tarballs extracted
#      on top, ensuring the toolchains are always complete.
ENV ZEPHYR_SDK_VERSION=1.0.1
ENV ZEPHYR_SDK_INSTALL_DIR=/opt/zephyr-sdk

RUN ARCH="$(uname -m)" \
    && case "${ARCH}" in \
        x86_64) SDK_ARCH="x86_64" ;; \
        aarch64|arm64) SDK_ARCH="aarch64" ;; \
        *) echo "Unsupported architecture: ${ARCH}" && exit 1 ;; \
    esac \
    && TOOLCHAIN_ESP32S3="xtensa-espressif_esp32s3_zephyr-elf" \
    && TOOLCHAIN_ESP32="xtensa-espressif_esp32_zephyr-elf" \
    && FULL_SDK_URL="https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v${ZEPHYR_SDK_VERSION}/zephyr-sdk-${ZEPHYR_SDK_VERSION}_linux-${SDK_ARCH}_gnu.tar.xz" \
    && TOOLCHAIN_ESP32S3_URL="https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v${ZEPHYR_SDK_VERSION}/toolchain_gnu_linux-${SDK_ARCH}_${TOOLCHAIN_ESP32S3}.tar.xz" \
    && TOOLCHAIN_ESP32_URL="https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v${ZEPHYR_SDK_VERSION}/toolchain_gnu_linux-${SDK_ARCH}_${TOOLCHAIN_ESP32}.tar.xz" \
    # 3a. Full SDK bundle (cmake scaffolding, small files first in tar)
    && echo "Downloading SDK cmake scaffolding..." \
    && wget -q --show-progress --tries=5 --timeout=120 -O /tmp/sdk-full.tar.xz "${FULL_SDK_URL}" \
    && tar -xf /tmp/sdk-full.tar.xz -C /opt/ \
    && [ -d "/opt/zephyr-sdk-${ZEPHYR_SDK_VERSION}" ] \
    && mv "/opt/zephyr-sdk-${ZEPHYR_SDK_VERSION}" "${ZEPHYR_SDK_INSTALL_DIR}" \
    && rm /tmp/sdk-full.tar.xz \
    # 3b. Overlay the ESP32-S3 toolchain from its individual tarball (guarantees it's complete)
    && echo "Downloading ESP32-S3 toolchain..." \
    && wget -q --show-progress --tries=5 --timeout=120 -O /tmp/toolchain-s3.tar.xz "${TOOLCHAIN_ESP32S3_URL}" \
    && mkdir -p "${ZEPHYR_SDK_INSTALL_DIR}/gnu" \
    && rm -rf "${ZEPHYR_SDK_INSTALL_DIR}/gnu/${TOOLCHAIN_ESP32S3}" \
    && tar -xf /tmp/toolchain-s3.tar.xz -C "${ZEPHYR_SDK_INSTALL_DIR}/gnu/" \
    && rm /tmp/toolchain-s3.tar.xz \
    # 3c. Overlay the ESP32 toolchain from its individual tarball
    && echo "Downloading ESP32 toolchain..." \
    && wget -q --show-progress --tries=5 --timeout=120 -O /tmp/toolchain-esp32.tar.xz "${TOOLCHAIN_ESP32_URL}" \
    && rm -rf "${ZEPHYR_SDK_INSTALL_DIR}/gnu/${TOOLCHAIN_ESP32}" \
    && tar -xf /tmp/toolchain-esp32.tar.xz -C "${ZEPHYR_SDK_INSTALL_DIR}/gnu/" \
    && rm /tmp/toolchain-esp32.tar.xz \
    # 3d. Strip all other toolchains (save ~2 GB)
    && if [ -d "${ZEPHYR_SDK_INSTALL_DIR}/gnu" ]; then \
         for dir in "${ZEPHYR_SDK_INSTALL_DIR}"/gnu/*/; do \
           [ -d "$dir" ] || continue; \
           tc="$(basename "$dir")"; \
           [ "$tc" = "${TOOLCHAIN_ESP32S3}" ] || [ "$tc" = "${TOOLCHAIN_ESP32}" ] && continue; \
           echo "Removing unused toolchain: $tc"; \
           rm -rf "$dir"; \
         done; \
       fi \
    # 3e. Update sdk_gnu_toolchains to list only the ones we keep
    && printf "%s\n%s\n" "${TOOLCHAIN_ESP32S3}" "${TOOLCHAIN_ESP32}" > "${ZEPHYR_SDK_INSTALL_DIR}/sdk_gnu_toolchains" \
    # 3f. Verify the toolchains are usable before proceeding
    && TOOLCHAIN_ESP32S3_BIN="${ZEPHYR_SDK_INSTALL_DIR}/gnu/${TOOLCHAIN_ESP32S3}/bin/${TOOLCHAIN_ESP32S3}-gcc" \
    && TOOLCHAIN_ESP32_BIN="${ZEPHYR_SDK_INSTALL_DIR}/gnu/${TOOLCHAIN_ESP32}/bin/${TOOLCHAIN_ESP32}-gcc" \
    && if [ ! -x "${TOOLCHAIN_ESP32S3_BIN}" ] || [ ! -x "${TOOLCHAIN_ESP32_BIN}" ]; then \
         echo "ERROR: Toolchain not found or not executable"; \
         ls -la "${ZEPHYR_SDK_INSTALL_DIR}/gnu/" 2>/dev/null || echo "gnu/ empty or missing"; \
         exit 1; \
       fi \
    && echo "SDK installed successfully (${TOOLCHAIN_ESP32S3} and ${TOOLCHAIN_ESP32} verified)"

# ------------------------------------------------------------------
# 4. Emscripten SDK (WASM demo toolchain)
# ------------------------------------------------------------------
# Installs the Emscripten SDK so the WebAssembly demo (demo/) can be built
# with `make -C demo`. Uses the official emsdk installer and activates the
# latest release. The EM_CONFIG is written to the SDK root so emcc works
# regardless of the active user. The PATH entries mirror what
# `emsdk_env.sh` would export.
ENV EMSDK=/opt/emsdk
ENV EM_CONFIG=/opt/emsdk/.emscripten
ENV PATH="/opt/emsdk:/opt/emsdk/upstream/emscripten:/opt/emsdk/upstream/bin:${PATH}"

RUN echo "Installing Emscripten SDK..." \
    && git clone --depth 1 https://github.com/emscripten-core/emsdk.git "${EMSDK}" \
    && cd "${EMSDK}" \
    && ./emsdk install latest \
    && ./emsdk activate latest \
    # Sanity check: compile a trivial file to pre-populate the system cache
    && echo 'int main(void) { return 0; }' > /tmp/hello.c \
    && emcc -c /tmp/hello.c -o /tmp/hello.o \
    && rm -f /tmp/hello.c /tmp/hello.o \
    && emcc --version | head -1 \
    && echo "Emscripten installed"

# ------------------------------------------------------------------
# 5. Zephyr RTOS (v4.4.2)
# ------------------------------------------------------------------
# Pre-downloads the Zephyr RTOS source at version v4.4.2 into the image so
# the framework is available at build time.  The west manifest (west.yml)
# pins the same revision, keeping the workspace consistent.
ENV ZEPHYR_VERSION=v4.4.2
ENV ZEPHYR_BASE=/opt/zephyr

RUN echo "Cloning Zephyr RTOS ${ZEPHYR_VERSION}..." \
    && git clone --depth 1 --branch "${ZEPHYR_VERSION}" \
       https://github.com/zephyrproject-rtos/zephyr /opt/zephyr \
    && rm -rf /opt/zephyr/.git \
    && test -f /opt/zephyr/CMakeLists.txt \
    && echo "Zephyr RTOS ${ZEPHYR_VERSION} installed"

# ------------------------------------------------------------------
# 6. Neovim, Lazygit and OpenCode (optional developer tools)
# ------------------------------------------------------------------
# These downloads are developer conveniences, not required to build or
# test the firmware. A transient GitHub download failure must NOT abort
# the CI docker build, so each tool is installed in an isolated step that
# tolerates failure (logs a warning and continues).
ENV NVIM_VERSION=0.11.2
ENV LAZYGIT_VERSION=0.45.2

# Map the build architecture to the GitHub release asset suffix.
RUN ARCH="$(uname -m)" \
    && case "${ARCH}" in \
        x86_64) N_ARCH="x86_64" ;; \
        aarch64|arm64) N_ARCH="arm64" ;; \
        *) echo "Unsupported architecture: ${ARCH}" && exit 1 ;; \
    esac \
    && echo "${N_ARCH}" > /tmp/n_arch.txt

# 6a. Neovim (best-effort; failure is non-fatal for CI)
RUN N_ARCH="$(cat /tmp/n_arch.txt)" \
    && NVIM_URL="https://github.com/neovim/neovim/releases/download/v${NVIM_VERSION}/nvim-linux-${N_ARCH}.tar.gz" \
    && echo "Downloading Neovim ${NVIM_VERSION} for ${N_ARCH}..." \
    && if wget -q --show-progress --tries=3 --timeout=60 -O /tmp/nvim.tar.gz "${NVIM_URL}"; then \
         tar -xzf /tmp/nvim.tar.gz -C /opt/ \
         && ln -sf /opt/nvim-linux-${N_ARCH}/bin/nvim /usr/local/bin/nvim \
         && rm /tmp/nvim.tar.gz \
         && nvim --version | head -1 \
         && echo "Neovim installed" \
         ; \
       else \
         echo "WARNING: Neovim download failed; continuing without it" \
         ; \
       fi

# 6b. Lazygit (best-effort; failure is non-fatal for CI)
RUN N_ARCH="$(cat /tmp/n_arch.txt)" \
    && LAZYGIT_URL="https://github.com/jesseduffield/lazygit/releases/download/v${LAZYGIT_VERSION}/lazygit_${LAZYGIT_VERSION}_Linux_${N_ARCH}.tar.gz" \
    && echo "Downloading Lazygit ${LAZYGIT_VERSION} for ${N_ARCH}..." \
    && if wget -q --show-progress --tries=3 --timeout=60 -O /tmp/lazygit.tar.gz "${LAZYGIT_URL}"; then \
         tar -xzf /tmp/lazygit.tar.gz -C /usr/local/bin/ lazygit \
         && rm /tmp/lazygit.tar.gz \
         && lazygit --version \
         && echo "Lazygit installed" \
         ; \
       else \
         echo "WARNING: Lazygit download failed; continuing without it" \
         ; \
       fi \
    && rm -f /tmp/n_arch.txt

# 6c. OpenCode (best-effort; failure is non-fatal for CI)
# AI coding agent for the terminal. The official installer only writes to
# $HOME/.opencode/bin, so we run it inside a temporary home and copy the
# binary to /usr/local/bin to put it on PATH for every user.
RUN OPENCODE_HOME="$(mktemp -d)" \
    && echo "Installing OpenCode..." \
    && if HOME="${OPENCODE_HOME}" bash -c "curl -fsSL https://opencode.ai/install | bash"; then \
         install -m 0755 "${OPENCODE_HOME}/.opencode/bin/opencode" /usr/local/bin/opencode \
         && chown ubuntu:ubuntu /usr/local/bin/opencode \
         && opencode --version \
         && echo "OpenCode installed"; \
       else \
         echo "WARNING: OpenCode install failed; continuing without it"; \
       fi \
    && rm -rf "${OPENCODE_HOME}"

# ------------------------------------------------------------------
# 7. Environment setup
# ------------------------------------------------------------------
ENV ZEPHYR_TOOLCHAIN_VARIANT=zephyr
# Zephyr RTOS was pre-downloaded into the image at /opt/zephyr (v4.4.2).
ENV ZEPHYR_BASE=/opt/zephyr

# ------------------------------------------------------------------
# 8. Non-root user
# ------------------------------------------------------------------
ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=1000

# Create group first, then user, then assign sudo.
# The base image (ubuntu:24.04) already ships a user/group named "ubuntu" with
# UID/GID 1000, so groupadd fails silently there. Fall back to `-o` (allow a
# duplicate GID) so the "vscode" group always exists for the entrypoint to
# adjust at runtime.
RUN ( groupadd --gid "${USER_GID}" "${USERNAME}" 2>/dev/null || \
      groupadd -o --gid "${USER_GID}" "${USERNAME}" 2>/dev/null || true ); \
    if ! id -u "${USERNAME}" &>/dev/null; then \
        if getent passwd "${USER_UID}" &>/dev/null; then \
            useradd --uid "${USER_UID}" --gid "${USER_GID}" -m "${USERNAME}" -o 2>/dev/null || true; \
        else \
            useradd --uid "${USER_UID}" --gid "${USER_GID}" -m "${USERNAME}"; \
        fi; \
    fi; \
    echo "${USERNAME} ALL=(root) NOPASSWD:ALL" > /etc/sudoers.d/"${USERNAME}" && \
    chmod 0440 /etc/sudoers.d/"${USERNAME}" && \
    # Safety net: ALL sudo-group members get passwordless sudo. This handles
    # cases where the devcontainer runtime creates a different user at runtime
    # (e.g. "ubuntu") with the same UID as USERNAME due to overrideCommand
    # bypassing the entrypoint.
    echo "%sudo ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/sudogroup-nopasswd && \
    chmod 0440 /etc/sudoers.d/sudogroup-nopasswd

# ------------------------------------------------------------------
# 9. LazyVim (Neovim configuration) — optional developer tooling
# ------------------------------------------------------------------
# Best-effort clone; a transient GitHub failure must not abort the CI build.
RUN mkdir -p /home/${USERNAME}/.config \
    && if git clone --depth 1 https://github.com/LazyVim/starter /home/${USERNAME}/.config/nvim; then \
         rm -rf /home/${USERNAME}/.config/nvim/.git \
         && echo "LazyVim installed"; \
       else \
         echo "WARNING: LazyVim clone failed; continuing without it"; \
       fi \
    && chown -R ${USERNAME}:${USER_GID} /home/${USERNAME}/.config

# ------------------------------------------------------------------
# 10. Oh My Zsh + plugins (used by the host .zshrc mounted from the MacBook)
# ------------------------------------------------------------------
# The mounted .zshrc points ZSH="$HOME/.oh-my-zsh" and loads the plugins
# git, zsh-autosuggestions and zsh-syntax-highlighting.  We install those
# here (into /home/${USERNAME}) so the container is self-contained; the
# entrypoint bridges them to the effective runtime user.
# Oh My Zsh itself is CRITICAL: without oh-my-zsh.sh the mounted .zshrc
# fails to source and zsh is unusable.  Clone it with retries and abort the
# build if it never succeeds (a transient GitHub failure must not silently
# produce a broken shell).
# Clone into a temporary dir and move into place so the target never
# pre-exists (git refuses to clone into a non-empty directory).
RUN clone_ohmyzsh() { \
         for i in 1 2 3 4 5; do \
           rm -rf /tmp/oh-my-zsh; \
           if git clone --depth 1 https://github.com/ohmyzsh/ohmyzsh.git /tmp/oh-my-zsh; then \
             return 0; \
           fi; \
           echo "Oh My Zsh clone attempt ${i} failed; retrying in 5s..."; \
           sleep 5; \
         done; \
         return 1; \
       }; \
       if clone_ohmyzsh; then \
         rm -rf /home/${USERNAME}/.oh-my-zsh \
         && mv /tmp/oh-my-zsh /home/${USERNAME}/.oh-my-zsh \
         && rm -rf /home/${USERNAME}/.oh-my-zsh/.git \
         && echo "Oh My Zsh installed"; \
       else \
         echo "ERROR: Oh My Zsh clone failed after retries" >&2; \
         exit 1; \
       fi \
    && test -f /home/${USERNAME}/.oh-my-zsh/oh-my-zsh.sh \
    && mkdir -p /home/${USERNAME}/.oh-my-zsh/custom/plugins \
    && if git clone --depth 1 https://github.com/zsh-users/zsh-autosuggestions /home/${USERNAME}/.oh-my-zsh/custom/plugins/zsh-autosuggestions; then \
         echo "zsh-autosuggestions installed"; \
       else \
         echo "WARNING: zsh-autosuggestions clone failed; continuing without it"; \
       fi \
    && if git clone --depth 1 https://github.com/zsh-users/zsh-syntax-highlighting /home/${USERNAME}/.oh-my-zsh/custom/plugins/zsh-syntax-highlighting; then \
         echo "zsh-syntax-highlighting installed"; \
       else \
         echo "WARNING: zsh-syntax-highlighting clone failed; continuing without it"; \
       fi \
    && chown -R ${USERNAME}:${USER_GID} /home/${USERNAME}/.oh-my-zsh

# ------------------------------------------------------------------
# 11. Make zsh the default shell for the container user
# ------------------------------------------------------------------
RUN chsh -s /usr/bin/zsh ${USERNAME}

# Entrypoint — adjusts UID/GID at runtime for volume-mounted workspaces.
# Must run as root so it can call usermod/groupmod/chown, then
# it drops privileges to USERNAME via sudo -E before exec.
# Path is relative to build context (workspace root), set in devcontainer.json
COPY .devcontainer/entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod +x /usr/local/bin/entrypoint.sh

# Switch back to root for entrypoint; the script will drop privileges
USER root
ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
WORKDIR /workspaces/relic-core
