# SPDX-License-Identifier: GPL-3.0-or-later
#
# Relic Core — WASM Docker image.
#
# Multi-stage build:
#   1. Builds the SSS + QR WASM modules with Emscripten.
#   2. Assembles the static assets (HTML/CSS/JS/WASM) and serves them with nginx.
#
# The resulting image is architecture-independent (pure static files) and
# runs on any platform: amd64 (Linux, Windows, macOS), arm64 (Raspberry Pi
# 4/5, Apple Silicon), arm/v7 (Raspberry Pi 3).
#
# Build arguments describing the source version (passed by the CI workflow):
#   VERSION      — release tag (ex: v1.2.0) or "dev" for untagged builds
#   COMMIT_SHA   — full git commit SHA
#   BRANCH       — source branch (ex: main, dev/jfi)
#   BUILD_DATE   — RFC 3339 timestamp of the build

# ── Build arguments (applied in the final stage) ──────────────────────────
ARG VERSION=dev
ARG COMMIT_SHA=unknown
ARG BRANCH=unknown
ARG BUILD_DATE=unknown

# ── Stage 1: Build WASM modules ───────────────────────────────────────────
#
# emscripten/emsdk only publishes linux/amd64 and linux/arm64 manifests
# (no linux/arm/v7). We pin the build stage to amd64 so it always builds
# regardless of the target platform. The resulting .wasm modules are
# architecture-independent (pure static files), so this is safe.
FROM --platform=linux/amd64 emscripten/emsdk:latest AS wasm-builder

WORKDIR /workspace

# Copy the source files needed for the WASM build.
COPY demo/                  demo/
COPY src/sss/               src/sss/
COPY src/qrcode/            src/qrcode/
COPY src/svg/               src/svg/
COPY src/compression/        src/compression/
COPY external/quirc/        external/quirc/

# Build the WASM modules (sss.*, qr.* and qr_decode.*) into demo/scripts/.
RUN make -C demo clean all

# ── Stage 2: Assemble static assets ───────────────────────────────────────
FROM nginx:alpine AS assets

# Copy the web assets from the ESP32 captive portal (the same ones used for
# the GitHub Pages demo).
COPY src/access-point/assets/index.html     /usr/share/nginx/html/index.html
COPY src/access-point/assets/split.html     /usr/share/nginx/html/split.html
COPY src/access-point/assets/unsplit.html   /usr/share/nginx/html/unsplit.html
COPY src/access-point/assets/style.css      /usr/share/nginx/html/style.css

# Copy the favicon.
COPY doc/img/relic-logo_icone.svg         /usr/share/nginx/html/favicon.svg

# Copy the client-side JS (split / unsplit / QR scanner).
COPY src/access-point/assets/scripts/       /usr/share/nginx/html/scripts/

# Copy the WASM modules built in stage 1.
COPY --from=wasm-builder /workspace/demo/scripts/sss.js   /usr/share/nginx/html/scripts/sss.js
COPY --from=wasm-builder /workspace/demo/scripts/sss.wasm /usr/share/nginx/html/scripts/sss.wasm
COPY --from=wasm-builder /workspace/demo/scripts/qr.js    /usr/share/nginx/html/scripts/qr.js
COPY --from=wasm-builder /workspace/demo/scripts/qr.wasm  /usr/share/nginx/html/scripts/qr.wasm
COPY --from=wasm-builder /workspace/demo/scripts/qr_decode.js   /usr/share/nginx/html/scripts/qr_decode.js
COPY --from=wasm-builder /workspace/demo/scripts/qr_decode.wasm /usr/share/nginx/html/scripts/qr_decode.wasm

# ── Stage 3: Final runtime image ──────────────────────────────────────────
FROM nginx:alpine

# Re-declare the build args so they are available in this stage.
ARG VERSION=dev
ARG COMMIT_SHA=unknown
ARG BRANCH=unknown
ARG BUILD_DATE=unknown

# Copy the nginx configuration (adds application/wasm MIME type).
COPY docker/nginx.conf /etc/nginx/conf.d/default.conf

# Copy the assembled static assets.
COPY --from=assets /usr/share/nginx/html/ /usr/share/nginx/html/

# Write a version manifest so users can identify which build of Relic Core
# this image corresponds to (visible at http://<host>/version.json).
RUN printf '{\n  "version": "%s",\n  "commit": "%s",\n  "branch": "%s",\n  "build_date": "%s"\n}\n' \
    "${VERSION}" "${COMMIT_SHA}" "${BRANCH}" "${BUILD_DATE}" \
    > /usr/share/nginx/html/version.json

# Also expose the version as OCI labels so it is visible via docker inspect:
#   docker inspect relic-core --format '{{index .Config.Labels "org.opencontainers.image.version"}}'
LABEL org.opencontainers.image.title="Relic Core" \
      org.opencontainers.image.description="Shamir's Secret Sharing web UI" \
      org.opencontainers.image.version="${VERSION}" \
      org.opencontainers.image.revision="${COMMIT_SHA}" \
      org.opencontainers.image.source="https://github.com/ficaud/relic-core" \
      org.opencontainers.image.created="${BUILD_DATE}"

EXPOSE 80

HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
    CMD wget -q --spider http://localhost/ || exit 1

