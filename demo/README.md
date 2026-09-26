# Relic Core · WASM Demo

Shamir's Secret Sharing (SSS) over GF(256) — compiled to WebAssembly and running entirely in your browser.

Secret data **never leaves your machine**. All computations happen client-side via the WASM module compiled from `src/sss/sss.c`.

All web assets (HTML, CSS, JS) come from `../src/access-point/assets/` —
the exact same files deployed on the embedded device.  The scripts detect
whether WASM is available and fall back to `fetch()` calls on the device.

## Usage

- **Split** — enter any text and get 5 shares (threshold: 3).
- **Reconstruct** — paste 3 shares in `x:hex` format to recover the original secret.

## How it works

- `../src/sss/sss.c` / `../src/sss/sss.h` — pure C Shamir's Secret Sharing over GF(256)
- `../src/access-point/assets/` — HTML, CSS & dual-mode JS (WASM + fetch fallback)
- `src/main.c` — Emscripten glue exposing `sss_split_wasm` / `sss_combine_wasm`
- Built by a GitHub Action on every push and deployed to **GitHub Pages**

## Camera access

QR scanning with the live camera requires a **secure context**: serve over
HTTPS, or open via `http://localhost`. On a plain `http://<LAN-IP>` page the
browser blocks the camera and the scanner falls back to the file picker.
