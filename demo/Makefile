# Makefile — Build SSS + QR WASM demo
#
# Requires Emscripten SDK:
#   docker run --rm -v $(PWD):/src emscripten/emsdk make -C /src
#
# References the parent project sss.c and qr_encode.c directly (no copy).
# Outputs sss.js / sss.wasm and qr.js / qr.wasm to scripts/ so the WASM-aware
# JS (split.js, unsplit.js) can load the modules from the same directory.
# HTML pages are served from ../src/access-point/assets/.

# Locate the Emscripten compiler. Prefer `emcc` already on PATH, otherwise fall
# back to a locally-installed emsdk (e.g. ~/emsdk or /opt/emsdk) so the demo
# builds even in shells that have not sourced emsdk_env.sh.
EMCC ?= $(shell command -v emcc 2>/dev/null || \
    for d in $$HOME/emsdk/upstream/emscripten/emcc \
             /opt/emsdk/upstream/emscripten/emcc \
             /emsdk/upstream/emscripten/emcc; do \
        [ -x "$$d" ] && { echo "$$d"; break; }; \
    done)

# If EMCC is still empty, fall back to a bare `emcc` so the error message is
# the familiar "emcc: No such file or directory".
EMCC := $(if $(EMCC),$(EMCC),emcc)

CFLAGS = -O3 -flto
WASM_FLAGS = \
	-s MODULARIZE=1 \
	-s EXPORT_NAME=SSS \
	-s EXPORTED_FUNCTIONS=_wasm_set_seed,_sss_split_wasm,_sss_combine_wasm,_malloc,_free \
	-s EXPORTED_RUNTIME_METHODS=ccall,cwrap,getValue,setValue \
	-s NO_EXIT_RUNTIME=1 \
	-s FILESYSTEM=0 \
	--no-entry

RELIC_SSS_DIR = ../src/sss
RELIC_SSS = $(RELIC_SSS_DIR)/sss.c
SRC = src/main.c src/sys_rand_stub.c $(RELIC_SSS)
INC = -Isrc -Isrc/stubs -I$(RELIC_SSS_DIR)
OUT_DIR = scripts
OUT_WASM = $(OUT_DIR)/sss.wasm
OUT_JS = $(OUT_DIR)/sss.js

# ── QR Code WASM ──

QR_WASM_FLAGS = \
	-s MODULARIZE=1 \
	-s EXPORT_NAME=QRWasm \
	-s EXPORTED_FUNCTIONS=_wasm_qr_generate,_wasm_qr_free,_wasm_share_to_base32,_malloc,_free \
	-s EXPORTED_RUNTIME_METHODS=ccall,cwrap,getValue,setValue,UTF8ToString,stringToUTF8,lengthBytesUTF8 \
	-s NO_EXIT_RUNTIME=1 \
	-s FILESYSTEM=0 \
	--no-entry

RELIC_QR_DIR = ../src/qrcode
RELIC_SVG_DIR = ../src/svg
RELIC_COMPRESSION_DIR = ../src/compression
QR_SRC = src/qr_wasm.c $(RELIC_QR_DIR)/qr_encode.c $(RELIC_SVG_DIR)/qrcode_to_svg.c \
	$(RELIC_COMPRESSION_DIR)/share_base32.c $(RELIC_COMPRESSION_DIR)/base32.c
QR_INC = -I$(RELIC_QR_DIR) -I$(RELIC_SVG_DIR) -I$(RELIC_COMPRESSION_DIR) -I$(RELIC_SSS_DIR)
QR_OUT_WASM = $(OUT_DIR)/qr.wasm
QR_OUT_JS = $(OUT_DIR)/qr.js

# ── QR Code decode WASM (quirc) ──

QR_DECODE_WASM_FLAGS = \
	-s MODULARIZE=1 \
	-s EXPORT_NAME=QRDecodeWasm \
	-s EXPORTED_FUNCTIONS=_wasm_qr_decode,_wasm_share_from_base32,_malloc,_free \
	-s EXPORTED_RUNTIME_METHODS=ccall,cwrap,getValue,setValue,UTF8ToString,HEAPU8 \
	-s NO_EXIT_RUNTIME=1 \
	-s FILESYSTEM=0 \
	--no-entry

QUIRC_DIR = ../external/quirc/lib
QUIRC_SRC = $(QUIRC_DIR)/quirc.c $(QUIRC_DIR)/decode.c $(QUIRC_DIR)/identify.c $(QUIRC_DIR)/version_db.c
# The wrapper (demo) plus the portable decoder logic moved into src/qrcode/.
RELIC_QR_DECODE_DIR = ../src/qrcode
QR_DECODE_SRC = src/qr_decode_wasm.c $(RELIC_QR_DECODE_DIR)/qr_decode.c $(QUIRC_SRC) \
	$(RELIC_COMPRESSION_DIR)/share_base32.c $(RELIC_COMPRESSION_DIR)/base32.c
QR_DECODE_INC = -I$(RELIC_QR_DECODE_DIR) -I$(QUIRC_DIR) -I$(RELIC_COMPRESSION_DIR) -I$(RELIC_SSS_DIR)
QR_DECODE_OUT_WASM = $(OUT_DIR)/qr_decode.wasm
QR_DECODE_OUT_JS = $(OUT_DIR)/qr_decode.js

.PHONY: all clean

all: $(OUT_JS) $(OUT_WASM) $(QR_OUT_JS) $(QR_OUT_WASM) $(QR_DECODE_OUT_JS) $(QR_DECODE_OUT_WASM)

$(OUT_DIR):
	mkdir -p $(OUT_DIR)

$(OUT_JS) $(OUT_WASM): $(SRC) | $(OUT_DIR)
	$(EMCC) $(CFLAGS) $(INC) $(WASM_FLAGS) -o $(OUT_JS) $(SRC)

$(QR_OUT_JS) $(QR_OUT_WASM): $(QR_SRC) | $(OUT_DIR)
	$(EMCC) $(CFLAGS) $(QR_INC) $(QR_WASM_FLAGS) -o $(QR_OUT_JS) $(QR_SRC)

$(QR_DECODE_OUT_JS) $(QR_DECODE_OUT_WASM): $(QR_DECODE_SRC) | $(OUT_DIR)
	$(EMCC) $(CFLAGS) $(QR_DECODE_INC) $(QR_DECODE_WASM_FLAGS) -o $(QR_DECODE_OUT_JS) $(QR_DECODE_SRC)

clean:
	rm -f $(OUT_JS) $(OUT_WASM) $(QR_OUT_JS) $(QR_OUT_WASM) $(QR_DECODE_OUT_JS) $(QR_DECODE_OUT_WASM)
