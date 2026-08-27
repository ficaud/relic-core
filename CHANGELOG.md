# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.6.1] - 2026-08-27

### added
- `AGENTS.md` : ai agents references files to know more about the project ([#47](https://github.com/ficaud/relic-core/issues/47))
- `test_qr_decode.cpp` : qr decode unitary tests
- `gcovr` : unitary tests coverage report that is published on github page ([#55](https://github.com/ficaud/relic-core/issues/55))

### changed
- `README` : added badge that exhibit the current coverage of the unitary tests + table that contains all the external libs dependencies

## [1.6.0] - 2026-08-27

### added
- `bip-39` : pass phrase in BIP-39 compression which means you can add a pass phrases at the end of the seed phrase in BIP-39 compression mode ([#52](https://github.com/ficaud/relic-core/issues/52)).
- `CODE_OF_CONDUCT.md` `SECURITY.md` : code of conduct and security policy files
- `.gitignore` : .private/ folder to get notes and others infos that are not supposed to be pushed.

### changed
- `license` : add the GPLv3 license to all files that were not already licensed
- `CONTRIBUTING.md` : update the contribution guidelines by adding commit convention, pull request rules and coding style.
- `Dockerfile` : added `fd-find` to use nvim explorer to find files faster

## [1.5.0] - 2026-08-26

### added
- `bip-39` : bip-39 seed phrase compression and decompression converting word list in hex number index ([#51](https://github.com/ficaud/relic-core/issues/51))

## [1.4.4] - 2026-08-24

### added
- `xiao32 (esp32s3)` : support for the xiao32 (esp32s3) board
- `base32`: base32 encoding/decoding and its unit tests for qrcode shares conversation only ([#46](https://github.com/ficaud/relic-core/issues/46)).

### changed
- `devcontainer` : update devcontainer to forward password manager ssh keys agents using open-ssh
- `readme` : update project's name to reveal the acronym of "Relic"

## [1.4.3] - 2026-08-18

### changed
- `project name`: changed project name from horcrux-core to relic-core and rename all the references to the old name.

## [1.4.2] - 2026-08-16

### added
- `quirc`: added quirc to decode qrcode for ESP32S3
- `dockerfile`: opencode to devcontainer

### changed
- `docker.md`: typo fixes + general improvements
- `contribution.md`: commands ready to be copied in the terminal 

## [1.4.1] - 2026-08-15

### added
- `qr_decode`: added quirc to decode qrcode in pure c for wasm demo pages ([#39](https://github.com/ficaud/horcrux-core/issues/39)).
- `workflows`: automatic pages and docker triggering when the release has been triggered
- `tasks.json`: build and serve demo wasm page in the devcontainer /w port forwarding to debug the demo page of see modification without flashing embedded device

### fixed 
- `index.html`: easier to understand main page note about the horcrux.co address
- `workflows`: removed most of the automatic master/main branches triggering (which make no sense sometimes).

### changed
- `c files header`: changed or add GPLv3 license to all c files

## [1.4.0] - 2026-08-14

### added
- `docker/`: Dockerfile and docker-compose.yml to build a WASM image of horcrux core ([#28](https://github.com/ficaud/horcrux-core/issues/28)).
- `readme`: features list section

### changed
- `index.html`: add a message telling the horcrux.co address is only available in the embedded version of horcrux core.
- `workflow`: update github actions usages to use the latest version

## [1.3.0] - 2026-08-13

### added
- `tasks.json`: unitary tests task
- `test_qrcode.cpp, test_svg.cpp`: golden tests for qrcode geenration and parsing into svg ([#37](https://github.com/ficaud/horcrux-core/issues/37)).
- `contribution.md`: unit tests related helpers

### fixed
- `github_workflows`: update nodejs > 20 fo fix action's warning about it

### changed
- `readme.md`: minor fixed + improvements
- `sss.c`: now produce upper cases hex strings (to optimize qrencode)
- `qr_encode.c`: only support alphanumeric mode (opitmized) + minor code formatting ([#35](https://github.com/ficaud/horcrux-core/issues/35))

## [1.2.3] - 2026-08-08

### Added
- `flash.html`: Added qrcode generation to make it easier to join the horcrux network after flashing the esp32 device ([#33](https://github.com/ficaud/horcrux-core/issues/33))

### Fixed
- `index.html`: Replace the index page link with the domain name one (Horcrux.co)
- `build.yml/release.yml`: Refresh cache before building the firmware to avoid errors when updating zephyr.

### Changed
- `Zephyr`: update to version 4.4.2 + fix for future updates

## [1.2.2] - 2026-08-02

### Fixed
 - `dns.c`: allow a list of domain names to trigger the captive portal serving (this is to have an automated portal generating on connection)

## [1.2.1] - 2026-08-02

### Added
- `.clangd`: Added a clangd configuration file to use clangd as the C/C++ language server instead of the VSCode IntelliSense engine, providing more accurate parsing and indexing of the project.
- `logo`: Added a new logo to the project in the readme and the flash page.
- `test_qrcode.cpp`: Added new unit tests for the QR code generation and parsing ([#17](https://github.com/ficaud/horcrux-core/issues/17)).
- `test_svg.cpp`: Added new unit tests for the SVG generation from QR codes 
([#17](https://github.com/ficaud/horcrux-core/issues/17)).

### Changed
- `devcontainer`: Update the devcontainer to install nvim, clangd, lazyvim, lazygit and ohmyzsh to ease the development experience.
- `dns.c`: Only accept url "horcrux.co" to serve the captive portal ([#29](https://github.com/ficaud/horcrux-core/issues/29)).

### Fixed
- `release.yml`: Removed the `.elf` and `.map` files from the release artifacts and updated the release README with the new instructions to flash the ESP32.
- `qrcode_to_svg`, `qr_encode`: Minor fixes to pass the unit tests.

## [1.2.0] - 2026-08-01

### Added
- `split.html`: QR code creation and SVG rendering have been added to the split page to allow users to easily share secrets with others by scanning the QR code ([#19](https://github.com/ficaud/horcrux-core/issues/19)).
- GPLv3 license has been added to the project to ensure that it remains free and open-source for all users ([#24](https://github.com/ficaud/horcrux-core/issues/24)).

### Fixed
- `flash.html`: Replaced the back button with a back-to-demo page button (for the WASM demo).
- `devcontainer`: Removed `clangd`, which was causing conflicts with IntelliSense in VSCode and was not needed for the project.
- `c_cpp_properties.json`: Fixed the compiler path and the overall configuration to ensure that the project is properly parsed and indexed by the C/C++ extension in VSCode.
- `http_handler`: Moved some local buffers to static to reduce stack usage and avoid stack overflow when sending large secrets to split or reconstruct.

### Removed
- `contribution`: Left the contribution documentation empty for now, as it is not yet ready to be published. It will be added in a future release.
- `page_captive.h`: Removed the auto-generated captive portal header for ESP32 firmware, as it does not need to be versioned by Git ([#26](https://github.com/ficaud/horcrux-core/issues/26)).

## [1.1.1] - 2026-07-27

### Fixed
- `unplsit.html`: Qr code scanning was not working properly on either WASM demo or on the ESP32 captive portal (#23, #19).
- `index.html`: typo

## [1.1.0] - 2026-07-26

### Added
- `demo/flash.html`: A new web page has been added to the demo that allows users to flash the Horcrux Core firmware to their ESP32 boards directly from the browser using the Web Serial API.
- `unsplit.html`: A camera button has been added to allow users to scan a QR code to retrieve the secret from the Horcrux Core device instead of typing it manually (#18).
- `badges`: Zephyr version badge, build status badge, and demo link badge have been added to the README.md file to provide quick access to relevant information about the project.

### Changed
- `wifi_mgr`: The Wi-Fi MAC address is now used to customize the access point name (SSID) to avoid conflicts when multiple devices are in the same area (#16).
- `release`: Artifacts are now renamed to clearly indicate which platform they are built for (ESP32-S3-DevKitC-1 or ESP32-DevKit-V1).

### Fixed
- `http_server`: Increased the maximum size of the HTTP request body to avoid errors when sending large secrets to split or reconstruct, such as Bitcoin seed phrases.

### Removed
- `workflow`: Removed the automatic build and pages generation workflow when pushing to the `dev/jfi` branch.

## [1.0.0] - 2026-07-22

First version of the Horcrux Core project that provides the basics of what it is intended to do: split and reconstruct secrets using Shamir's Secret Sharing (SSS) over GF(256) on an embedded device, with a captive portal to manage the operations.

### Added
- `demo/wasm`: A new demo page that runs Shamir's Secret Sharing (SSS) entirely in the browser using WebAssembly (WASM) has been built and deployed to GitHub Pages. The demo page allows users to get an overview of the captive portal that will be displayed on the embedded device (#10).

### Changed
- `horcrux-core`: The old project was renamed to `horcrux-core` and now includes only the embedded firmware with the demo WASM page.

### Removed
- `readme`: ESPWebTool is no longer used as a means to flash devices because it is unstable, and it's difficult to know if it is still maintained or even working at any given time.

## [0.0.4] - 2026-07-18

### Added
- `sss`: Shamir's Secret Sharing algorithm implementation for splitting and reconstructing secrets using GF(2^8) finite fields.
- `unit_tests/sss`: Unit tests (using Google Test) with deterministic test vectors, as well as cross-validation with the [dsprenkels/sss](https://github.com/dsprenkels/sss) project to ensure correctness of the implementation.
- Version number display in the captive portal pages to indicate the current version of the Horcrux Core project (#4).
- Split / unsplit pages: Management of Shamir's Secret Sharing split and reconstruct operations from the captive portal pages using plain text and copy helpers.

### Changed
- `embed-assets`: The script now takes separate JavaScript files, minifies them, and embeds them into the captive portal pages (to ease web page maintenance and readability).

### Fixed
- Devcontainer: User permissions are now set correctly to avoid getting stuck when requiring root permissions to run commands in the devcontainer.
- `http_server`: Increased stack and query size limits to avoid errors on large secrets to split or reconstruct, such as Bitcoin seed phrases (#12).

## [0.0.3] - 2026-07-18

### Added
- Toolchain for ESP32 WROOM-32 boards, including a corresponding Dockerfile, CI build, and scripts to simplify building and flashing firmware on ESP32 WROOM-32 boards.
- Contribution guidelines and README updates explaining how to flash the embedded firmware to ESP32S3 and ESP32 WROOM-32 boards.
- Split and unsplit pages in the captive portal to prepare for Shamir's Secret Sharing code integration (in later releases).
- OpenOCD remote flash and monitor support for ESP32S3 boards (connected via a Raspberry Pi on the local network) to simplify flashing firmware on ESP32S3 boards.

### Removed
- Useless files remaining in the repository after the initial release.

## [0.0.2] - 2026-07-14

### Added
- Initial release of the Horcrux Core project with basic features and documentation (not an official working release).
