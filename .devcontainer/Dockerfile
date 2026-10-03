# SPDX-License-Identifier: GPL-3.0-or-later

# Devcontainer for Zephyr RTOS development on ESP32-S3-DevKitC-1
# Includes only what is required to build and test the firmware:
#   Zephyr SDK (xtensa-esp32s3 toolchain), west, esptool,
#   Emscripten SDK (WASM demo), clang-format, clangd.
#
# Personal developer tools (nvim, lazygit, opencode, zsh, Oh My Zsh, ...)
# are intentionally NOT installed here; they live in a private overlay
# repository and are baked into a personal image built on top of this one.

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
    clang-format \
    clangd \
    libmagic1 \
    lz4 \
    lzop \
    zstd \
    libgtest-dev \
    libgmock-dev \
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
# 6. Environment setup
# ------------------------------------------------------------------
ENV ZEPHYR_TOOLCHAIN_VARIANT=zephyr
# Zephyr RTOS was pre-downloaded into the image at /opt/zephyr (v4.4.2).
ENV ZEPHYR_BASE=/opt/zephyr

# ------------------------------------------------------------------
# 7. Non-root user
# ------------------------------------------------------------------
ARG USERNAME=devuser
ARG USER_UID=1000
ARG USER_GID=1000

# Create group first, then user, then assign sudo.
# The base image (ubuntu:24.04) already ships a user/group named "ubuntu" with
# UID/GID 1000. Remove it first so "devuser" is the unique UID/GID 1000 owner:
# otherwise getpwuid(1000) resolves to "ubuntu" (it comes first in /etc/passwd)
# and the runtime user gets HOME=/home/ubuntu instead of /home/devuser, which
# breaks the personal overlay (zsh/oh-my-zsh, nvim, gitconfig live in /home/devuser).
RUN userdel ubuntu 2>/dev/null || true; \
    groupdel ubuntu 2>/dev/null || true; \
    groupadd --gid "${USER_GID}" "${USERNAME}" 2>/dev/null || true; \
    useradd --uid "${USER_UID}" --gid "${USER_GID}" -m "${USERNAME}"; \
    echo "${USERNAME} ALL=(root) NOPASSWD:ALL" > /etc/sudoers.d/"${USERNAME}" && \
    chmod 0440 /etc/sudoers.d/"${USERNAME}" && \
    # Safety net: ALL sudo-group members get passwordless sudo. This handles
    # cases where the devcontainer runtime creates a different user at runtime
    # with the same UID as USERNAME due to overrideCommand bypassing the
    # entrypoint.
    echo "%sudo ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/sudogroup-nopasswd && \
    chmod 0440 /etc/sudoers.d/sudogroup-nopasswd

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
