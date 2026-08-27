# AGENTS.md

This file is the canonical reference for AI assistants working on the Relic
Core codebase. It is versioned in the repository (unlike `CLAUDE.md`, which is
git-ignored and local-only). Keep it factual and up to date.

**Maintenance**: when you discover a discrepancy (new module, renamed file,
changed command, new gotcha, etc.), update this file proactively rather than
leaving it stale.

## Project Overview

Relic Core (R.E.L.I.C. — *Recovery and Encryption via Lagrange Interpolated
Components*) is a firmware for ESP32 boards (and more) that implements **Shamir's Secret
Sharing (SSS)** to split and recover secrets (passwords, private keys, bitcoin
seed phrases, etc.) in a secure, offline way. One specific particularity of Relic Core is that it enables user to split and recover secrets using Qr codes.

- **Framework**: Zephyr RTOS **v4.4.2**
- **Language**: C (firmware), C++ (unit tests)
- **Target boards**: ESP32-S3-DevKitC-1, ESP32-DevKit-V1 (WROOM-32), Seeed XIAO ESP32S3, Raspberry Pi (through docker)
- **License**: GPL-3.0-or-later
- **Threshold scheme**: 3-of-5 (N=5 shares, K=3 required), over GF(2⁸)
- **Operation**: the device creates its own Wi-Fi AP and serves a captive portal that can generate Qr codes and scan them back to reconstruct the secret;
  secrets never leave the device during split/recovery.

## Repository Layout

```
├── src/                    # Firmware source code (main focus)
│   ├── main.c              # Entry point: init Wi-Fi AP, DNS, HTTP server
│   ├── access-point/       # Captive portal subsystem
│   │   ├── http/           # HTTP server, router, handlers, types (inc/ + src/)
│   │   ├── net/            # DNS interceptor and DHCP server (inc/ + src/)
│   │   ├── wifi/           # Wi-Fi access point manager (inc/ + src/)
│   │   └── assets/         # HTML/CSS/JS assets → embedded as page_captive.h
│   ├── sss/                # Shamir's Secret Sharing over GF(256)
│   ├── compression/        # Share size reduction codecs
│   │   ├── base32.c/h      # RFC 4648 base32 (unpadded)
│   │   ├── share_base32.c/h# "x:hex" <-> "x:base32" share conversion
│   │   ├── bip39.c/h       # BIP-39 seed phrase (de)compression (+ passphrase)
│   │   └── bip39_words.c/h # BIP-39 English wordlist (generated)
│   ├── qrcode/             # QR encode (Nayuki) + decode (quirc, S3-only)
│   └── svg/                # QR code → SVG conversion
├── tests/                  # Google Test unit tests (native, not Zephyr)
├── boards/                 # Per-board Kconfig fragments (*.conf)
├── external/sss/           # Reference SSS impl (dsprenkels/sss) — tests only
├── external/quirc/         # QR decoder (relic-quirc fork) — submodule
├── demo/                   # Web demo (WASM)
├── doc/                    # Documentation
├── tools/                  # Build/flash helpers (embed-assets.py, flash scripts)
├── CMakeLists.txt          # Build dispatcher (tests vs firmware)
├── CMakePresets.json       # CMake presets
├── Kconfig                 # Zephyr configuration
├── prj.conf                # Zephyr project configuration
└── west.yml                # West manifest (Zephyr modules)
```

## Architecture & Modules

### `src/sss/` — Shamir's Secret Sharing
- Uses the AES irreducible polynomial `x^8 + x^4 + x^3 + x + 1` (0x11B).
- GF(256) addition is XOR; multiplication/division use precomputed log/exp tables.
- Randomness sourced from Zephyr's `sys_rand_get()`.
- `sss_split()` — split a secret into N shares (K required to reconstruct).
- `sss_combine()` — reconstruct a secret from K shares via Lagrange interpolation.
- Constants: `SSS_MAX_SECRET_LEN` (256), `SSS_N` (5), `SSS_K` (3).
- `struct sss_share { uint8_t x; uint8_t data[256]; size_t len; }`.

### `src/compression/` — share/secret compression
- `base32` — RFC 4648 codec, uppercase, **no '=' padding**; case-insensitive decode.
- `share_base32` — converts the canonical `x:hex...` share form to a compact
  `x:base32...` QR payload and back. Buffer sizing via `SHARE_B32_BUF_SIZE`.
- `bip39` — replaces seed-phrase words with their 2-byte little-endian indices;
  `bip39_compress_passphrase` handles an optional `;`-separated passphrase.

### `src/qrcode/`
- `qr_encode.c` — Nayuki QR-Code-generator (reduced build: alphanumeric, ECC LOW).
- `qr_decode.c` — quirc wrapper (`qr_decode_begin/commit/destroy/buffer`),
  compiled **only** when `CONFIG_RELIC_QR_DECODE_SERVER` is set (ESP32-S3 boards).

### `src/access-point/`
- `http/` — `http_server.c` (single-threaded socket loop), `http_router.c`
  (path → handler dispatch), `http_handlers.c` (business logic), `http_types.c`.
- `net/` — `dns.c` (captive-portal DNS interceptor), `dhcp.c` (DHCP server).
- `wifi/` — `wifi_mgr.c` (AP mode init).
- `assets/` — embedded via `tools/embed-assets.py` into `page_captive.h`
  (generated into the build dir, git-ignored).

## HTTP API (captive portal)

Routes defined in `http_router.c`:

| Route | Handler | Notes |
|---|---|---|
| `/` | `handler_root` | Captive portal home |
| `/split.html` | `handler_split` | Split page |
| `/unsplit.html` | `handler_unsplit` | Reconstruct page |
| `/divide` | `handler_divide` | `?msg=<secret>&bip=<classic|passphrase>` → JSON shares |
| `/reconstruct` | `handler_reconstruct` | `?d=<hex,hex,...>&x=<x,x,...>&bip=...` → JSON secret |
| `/qr.svg` | `handler_qr_svg` | `?text=<text>` → QR SVG |
| `/qr-share.svg` | `handler_qr_share_svg` | `?text=<x:hex...>` → base32-compressed QR SVG |
| `/qr_decode` | `handler_qr_decode_stream` | POST `?w=&h=` grayscale body (S3-only) |

Unknown paths fall back to `handler_captive_portal` (returns the captive page so
Android/Apple/Windows detection triggers the portal popup).

## Build System

The root `CMakeLists.txt` dispatches on `BUILD_TESTS`:

1. **Firmware** (default, Zephyr RTOS):
   ```bash
   west build -b esp32s3_devkitc/esp32s3/procpu
   west build -b doit_esp32_devkit_v1/esp32/procpu
   west build -b xiao_esp32s3/esp32s3/procpu
   ```

2. **Native unit tests** (Google Test):
   ```bash
   cmake --preset tests
   cmake --build --preset tests
   ctest --test-dir build/tests
   ```

Key details:
- Firmware sources are listed explicitly in `target_sources(app PRIVATE ...)`.
- Zephyr modules are listed explicitly via `ZEPHYR_MODULES` to avoid the module
  scanner picking up the self project.
- `page_captive.h` is generated by `tools/embed-assets.py` into the build dir
  (per-board, since QR decode flags differ).
- Include dirs set via `target_include_directories(app PRIVATE ...)`.

## Boards & Kconfig Fragments

Per-board fragments live in `boards/*.conf` and are merged after `prj.conf`:

| Board | Build target | On-device QR decode |
|---|---|---|
| ESP32-S3-DevKitC-1 | `esp32s3_devkitc/esp32s3/procpu` | yes (`CONFIG_RELIC_QR_DECODE_SERVER=y`) |
| ESP32-DevKit-V1 | `doit_esp32_devkit_v1/esp32/procpu` | no (jsQR fallback) |
| Seeed XIAO ESP32S3 | `xiao_esp32s3/esp32s3/procpu` | yes |

- `CONFIG_RELIC_QR_DECODE_MAX_DIM`: 224 (S3) / 192 (V1).
- S3/XIAO tune `CONFIG_ESP32_WIFI_*` buffer counts down and enable
  `CONFIG_NET_CONTEXT_RCVTIMEO`.

## Testing

Unit tests live in `tests/`, use **Google Test**, and run natively. The reference
implementation `external/sss` (dsprenkels/sss) cross-validates the SSS output.

Test executables (each registered with `ctest`):
`sss_test`, `base32_test`, `bip39_test`, `share_base32_test`, `qrcode_test`,
`svg_test`, `qr_decode_test`.

```bash
ctest --test-dir build/tests                # summary
ctest --test-dir build/tests --verbose      # verbose
./build/tests/tests/qrcode_test --gtest_filter='QRCodeTest.Encode*'
```

- `mockup/sys_rand_stub.c` stubs Zephyr's `sys_rand_get()`.
- `stubs/zephyr/random/random.h` provides the fake Zephyr header.
- Coverage: `cmake --preset tests -DENABLE_COVERAGE=ON` then
  `cmake --build build/tests --target coverage` (uses gcovr, GCC only).

## Coding Style

- **Formatting**: `.clang-format` follows Zephyr/Linux kernel style (Allman
  braces, 4-space indent, 120-column limit).
- **Headers**: `#ifndef` / `#define` / `#endif` guards (not `#pragma once`).
- **C++**: `extern "C"` blocks for C headers used from C++.
- **Comments**: Doxygen-style (`@file`, `@brief`, `@param`, `@return`) for public APIs.
- **SPDX**: every file starts with `// SPDX-License-Identifier: GPL-3.0-or-later`.

## Commit & PR

- **The agent must not run `git commit`, push, or open PRs.** It only prepares
  the changes (code, tests, docs) and lets the human review, commit and open the
  PR. Only commit when the user explicitly asks.
- For the human's reference, the project uses Conventional Commits
  (`type(scope): description`, types `added`/`changed`/`fixed`/`chore`) and
  requires a PR against the protected `main` branch; `CHANGELOG.md` is updated
  with each change.
- Never commit secrets; `.env` and `public.pem` are git-ignored.

## Dev Environment & Tools

- **Dev container**: `.devcontainer/` (Zephyr SDK at `/opt/zephyr-sdk`,
  `ZEPHYR_BASE=/workspaces/relic-core/zephyr`). `devcontainer up` /
  `devcontainer exec --workspace-folder . zsh`.
- **Environment awareness**: the agent must determine whether it is running
  inside the dev container (e.g. `ZEPHYR_BASE` points to
  `/workspaces/relic-core/zephyr`, `/opt/zephyr-sdk` exists, `west`/`cmake` are
  available). When it is **not** inside the dev container, it must warn the user
  that builds, tests and other tooling may fail because the Zephyr SDK and build
  tools are missing.
- **WASM demo**: `demo/` (build via Makefile, serve with `python3 demo/serve.py`,
  port 8000).
- **Asset embedding**: `tools/embed-assets.py` → `page_captive.h` (auto-run by build).
- **clangd**: `.clangd` points to the root `compile_commands.json` symlink, which
  should be refreshed to the latest board build.

## Gotchas

- **ESP32-S3 Wi-Fi OOM**: the ESP-IDF Wi-Fi driver allocates from the kernel heap;
  default buffer counts overflow it (`esp32_wifi_adapter: memory allocation
  failed`). S3/XIAO fragments reduce `CONFIG_ESP32_WIFI_*` buffers.
- **TCP window size**: `CONFIG_NET_TCP_MAX_SEND/RECV_WINDOW_SIZE=4096` (bytes, not
  segments). A 4-byte value stalls the ~37 KB QR upload (~9400 round-trips).
- **Single-threaded HTTP server**: handlers use `static` response buffers; the
  `/qr_decode` body read relies on `SO_RCVTIMEO` (`CONFIG_NET_CONTEXT_RCVTIMEO=y`)
  so a stalled upload cannot block the server forever.
- **quirc RAM**: `QR_DECODE_MAX_DIM` limits image size; quirc buffers are shrunk
  via `QUIRC_MAX_PAYLOAD/CAPSTONES/GRIDS` and `QUIRC_FLOAT_TYPE=float` (ESP32-S3
  FPU is single-precision).
- **Versioning**: README states Zephyr v4.4.2; do not trust older docs that say
  v4.4.1.
