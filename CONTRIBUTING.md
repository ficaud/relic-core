# Contributing to Relic Core

Thanks for your interest in contributing! This guide explains how to set up
your environment, follow the project conventions, and submit changes.

## License

Relic Core is licensed under the **GNU General Public License v3.0**
(`GPL-3.0-or-later`). By contributing, you agree that your work is published
under this license. Every source file must start with the following SPDX
header:

```
// SPDX-License-Identifier: GPL-3.0-or-later
```

## Environment Setup

The project ships a dev container that provides a ready-to-use build
environment.

1. **Build and enter the dev container**

   ```bash
   devcontainer up --workspace-folder .
   devcontainer exec --workspace-folder . zsh
   ```

2. **Rebuild the dev container from scratch**

   ```bash
   devcontainer up --workspace-folder . --remove-existing-container
   ```

3. **Exit the dev container**

   ```bash
   exit
   ```

## Development

Build the firmware for a supported board with `west`:

```bash
west build -b esp32s3_devkitc/esp32s3/procpu
```

Supported boards:

| Board | Build target |
|---|---|
| ESP32-S3-DevKitC-1 | `esp32s3_devkitc/esp32s3/procpu` |
| ESP32-DevKit-V1 | `doit_esp32_devkit_v1/esp32/procpu` |
| Seeed XIAO ESP32S3 | `xiao_esp32s3/esp32s3/procpu` |

## Running the Tests

The project uses Google Test for native unit tests (they run on the host, not
the ESP32). Tests are built and run via CMake presets.

### 1. Configure

```bash
cmake --preset tests
```

### 2. Build

```bash
cmake --build --preset tests
```

### 3. Run all tests (summary output)

```bash
ctest --test-dir build/tests
```

### 4. Run all tests with verbose output

Shows each test's command line, working directory, and output in real time:

```bash
ctest --test-dir build/tests --verbose
```

### 5. Run a single test suite in isolation

Each suite is a standalone executable under `build/tests/tests/`:

```bash
# QR Code tests
./build/tests/tests/qrcode_test

# Shamir's Secret Sharing + QR tests
./build/tests/tests/sss_test

# QR → SVG conversion tests
./build/tests/tests/svg_test
```

### 6. Run a single test case with detailed output

```bash
# Run one test case with full details (per-test timing, assertions)
./build/tests/tests/qrcode_test --gtest_filter='QRCodeTest.ShareGoldenGridMatches'

# Run every test in a suite
./build/tests/tests/sss_test --gtest_filter='*'

# Run several cases matching multiple filters
./build/tests/tests/qrcode_test --gtest_filter='QRCodeTest.Encode*:QRCodeTest.Share*'
```

## Coding Style

- **Formatting**: `.clang-format` follows Zephyr/Linux kernel style (Allman
  braces, 4-space indent, 120-column limit).
- **Headers**: use `#ifndef` / `#define` / `#endif` guards (not `#pragma once`).
- **C++**: wrap C headers in `extern "C"` blocks when used from C++.
- **Comments**: Doxygen-style (`@file`, `@brief`, `@param`, `@return`) for
  public APIs.
- **SPDX**: every file starts with `// SPDX-License-Identifier: GPL-3.0-or-later`.

## Commit Convention

Commits follow [Conventional Commits](https://www.conventionalcommits.org/):

```
type(scope): description
```

Common types used in this project:

| Type | Purpose |
|---|---|
| `added` | New feature |
| `changed` | Modification of existing behaviour |
| `fixed` | Bug fix |
| `chore` | Housekeeping (deps, config, docs) |

Reference the related issue in the description when applicable:

```
added(share_base32): base32 encode/decode for shares compression (#46)
```

## Submitting a Pull Request

1. Fork the repository and create a branch from `main`.
2. Make your changes, following the conventions above.
3. Run the tests and ensure they pass.
4. Update `CHANGELOG.md` with an entry for your change.
5. Open a pull request against `main`.

The `main` branch is protected and a pull request is mandatory for all
changes. Changes require review before they can be merged.

## Local WASM Demo Server

Build the WASM demo using the `Build Demo WASM (Makefile)` task, then start a
local server. In the dev container, use port forwarding on port 8000:

```bash
cd /workspaces/relic-core/ && python3 demo/serve.py
```

Then open `http://localhost:8000`.
