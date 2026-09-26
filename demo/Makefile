# Makefile — Build SSS + QR WASM demo
#
# Requires Emscripten SDK:
#   docker run --rm -v $(PWD):/src emscripten/emsdk make -C /src
#
# References the parent project sss.c and qr_encode.c directly (no copy).
# Outputs sss.js/sss.wasm, qr.js/qr.wasm, compression.js/compression.wasm and
# qr_decode.js/qr_decode.wasm to scripts/ so the WASM-aware JS (split.js,
# unsplit.js) can load the modules from the same directory.
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

# C++ linker (same toolchain as EMCC). Needed to link the ZXing-cpp decode
# module against libc++; emcc alone would link libc only.
EMXX := $(subst emcc,em++,$(EMCC))

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
	-s EXPORTED_FUNCTIONS=_wasm_qr_generate,_wasm_qr_free,_malloc,_free \
	-s EXPORTED_RUNTIME_METHODS=ccall,cwrap,getValue,setValue,UTF8ToString,stringToUTF8,lengthBytesUTF8 \
	-s NO_EXIT_RUNTIME=1 \
	-s FILESYSTEM=0 \
	--no-entry

RELIC_QR_DIR = ../src/qrcode
RELIC_SVG_DIR = ../src/svg
QR_SRC = src/qr_wasm.c $(RELIC_QR_DIR)/qr_encode.c $(RELIC_SVG_DIR)/qrcode_to_svg.c
QR_INC = -I$(RELIC_QR_DIR) -I$(RELIC_SVG_DIR)
QR_OUT_WASM = $(OUT_DIR)/qr.wasm
QR_OUT_JS = $(OUT_DIR)/qr.js

# ── Compression codec WASM (share base32 + BIP-39) ──

COMPRESSION_WASM_FLAGS = \
	-s MODULARIZE=1 \
	-s EXPORT_NAME=CompressionWasm \
	-s EXPORTED_FUNCTIONS=_wasm_share_to_base32,_wasm_share_from_base32,_wasm_bip39_compress,_wasm_bip39_decompress,_wasm_bip39_compress_passphrase,_wasm_bip39_decompress_passphrase,_wasm_slip39_compress,_wasm_slip39_decompress,_wasm_slip39_compress_passphrase,_wasm_slip39_decompress_passphrase,_malloc,_free \
	-s EXPORTED_RUNTIME_METHODS=ccall,cwrap,getValue,setValue,UTF8ToString,stringToUTF8,lengthBytesUTF8 \
	-s NO_EXIT_RUNTIME=1 \
	-s FILESYSTEM=0 \
	--no-entry

RELIC_COMPRESSION_DIR = ../src/compression
COMPRESSION_SRC = src/compression_wasm.c \
	$(RELIC_COMPRESSION_DIR)/encoding/share_base32.c $(RELIC_COMPRESSION_DIR)/encoding/base32.c \
	$(RELIC_COMPRESSION_DIR)/word_list/wl_codec.c $(RELIC_COMPRESSION_DIR)/word_list/bip39_words.c \
	$(RELIC_COMPRESSION_DIR)/word_list/slip39_words.c
COMPRESSION_INC = -I$(RELIC_COMPRESSION_DIR)/encoding -I$(RELIC_COMPRESSION_DIR)/word_list -I$(RELIC_SSS_DIR)
COMPRESSION_OUT_WASM = $(OUT_DIR)/compression.wasm
COMPRESSION_OUT_JS = $(OUT_DIR)/compression.js

# ── QR Code decode WASM (ZXing-cpp, QR-only) ──

QR_DECODE_WASM_FLAGS = \
	-s MODULARIZE=1 \
	-s EXPORT_NAME=QRDecodeWasm \
	-s EXPORTED_FUNCTIONS=_wasm_qr_decode,_malloc,_free \
	-s EXPORTED_RUNTIME_METHODS=ccall,cwrap,getValue,setValue,UTF8ToString,HEAPU8 \
	-s NO_EXIT_RUNTIME=1 \
	-s FILESYSTEM=0 \
	-fexceptions \
	--no-entry

# ZXing-cpp QR-only reader subset (C++20), mirroring the firmware's
# CMakeLists.txt. The wrapper (demo) implements the same qr_decode_* interface
# as the quirc backend via src/qrcode/qr_decode_zxing.cpp.
RELIC_QR_DECODE_DIR = ../src/qrcode
ZXING_DIR = ../external/zxing-cpp/core/src
QR_DECODE_SRC = src/qr_decode_wasm.c \
	$(RELIC_QR_DECODE_DIR)/qr_decode_zxing.cpp \
	$(ZXING_DIR)/Barcode.cpp \
	$(ZXING_DIR)/BarcodeFormat.cpp \
	$(ZXING_DIR)/BitMatrix.cpp \
	$(ZXING_DIR)/CharacterSet.cpp \
	$(ZXING_DIR)/CreateBarcode.cpp \
	$(ZXING_DIR)/Content.cpp \
	$(ZXING_DIR)/ECI.cpp \
	$(ZXING_DIR)/Error.cpp \
	$(ZXING_DIR)/GTIN.cpp \
	$(ZXING_DIR)/JSON.cpp \
	$(ZXING_DIR)/ReadBarcode.cpp \
	$(ZXING_DIR)/ReedSolomon.cpp \
	$(ZXING_DIR)/Utf.cpp \
	$(ZXING_DIR)/WriteBarcode.cpp \
	$(ZXING_DIR)/ZXingCpp.cpp \
	$(ZXING_DIR)/BitArray.cpp \
	$(ZXING_DIR)/HRI.cpp \
	$(ZXING_DIR)/BinaryBitmap.cpp \
	$(ZXING_DIR)/BitSource.cpp \
	$(ZXING_DIR)/ConcentricFinder.cpp \
	$(ZXING_DIR)/GlobalHistogramBinarizer.cpp \
	$(ZXING_DIR)/GridSampler.cpp \
	$(ZXING_DIR)/HybridBinarizer.cpp \
	$(ZXING_DIR)/LocalGrid.cpp \
	$(ZXING_DIR)/MultiFormatReader.cpp \
	$(ZXING_DIR)/PerspectiveTransform.cpp \
	$(ZXING_DIR)/ResultPoint.cpp \
	$(ZXING_DIR)/TextDecoder.cpp \
	$(ZXING_DIR)/WhiteRectDetector.cpp \
	$(ZXING_DIR)/qrcode/QRCodecMode.cpp \
	$(ZXING_DIR)/qrcode/QRErrorCorrectionLevel.cpp \
	$(ZXING_DIR)/qrcode/QRVersion.cpp \
	$(ZXING_DIR)/qrcode/QRBitMatrixParser.cpp \
	$(ZXING_DIR)/qrcode/QRDataBlock.cpp \
	$(ZXING_DIR)/qrcode/QRDecoder.cpp \
	$(ZXING_DIR)/qrcode/QRDetector.cpp \
	$(ZXING_DIR)/qrcode/QRFormatInformation.cpp \
	$(ZXING_DIR)/qrcode/QRReader.cpp \
	$(ZXING_DIR)/libzueci/zueci.c
QR_DECODE_INC = -I$(RELIC_QR_DECODE_DIR) -I$(RELIC_QR_DECODE_DIR)/zxing -I$(ZXING_DIR)
QR_DECODE_CPPFLAGS = -std=c++20 -fexceptions -DZXING_INTERNAL
QR_DECODE_OUT_WASM = $(OUT_DIR)/qr_decode.wasm
QR_DECODE_OUT_JS = $(OUT_DIR)/qr_decode.js

# ZXing-cpp is C++ (needs -std=c++20) while the wrapper and libzueci are C, so
# compile each source to an object file with the right language flags and link
# them together afterwards (a single emcc invocation would apply -std=c++20 to
# the C files too).
QR_DECODE_OBJ_DIR = $(OUT_DIR)/obj/qr_decode
QR_DECODE_OBJS = \
	$(patsubst src/%.c,$(QR_DECODE_OBJ_DIR)/%.o,$(filter src/%,$(QR_DECODE_SRC))) \
	$(patsubst $(RELIC_QR_DECODE_DIR)/%.cpp,$(QR_DECODE_OBJ_DIR)/relic/%.o,$(filter $(RELIC_QR_DECODE_DIR)/%,$(QR_DECODE_SRC))) \
	$(patsubst $(ZXING_DIR)/%.cpp,$(QR_DECODE_OBJ_DIR)/zxing/%.o,$(filter $(ZXING_DIR)/%.cpp,$(QR_DECODE_SRC))) \
	$(patsubst $(ZXING_DIR)/libzueci/%.c,$(QR_DECODE_OBJ_DIR)/zxing/libzueci/%.o,$(filter $(ZXING_DIR)/libzueci/%.c,$(QR_DECODE_SRC)))

$(QR_DECODE_OBJ_DIR)/%.o: src/%.c | $(QR_DECODE_OBJ_DIR)
	$(EMCC) $(CFLAGS) $(QR_DECODE_INC) -c $< -o $@

$(QR_DECODE_OBJ_DIR)/relic/%.o: $(RELIC_QR_DECODE_DIR)/%.cpp | $(QR_DECODE_OBJ_DIR)
	$(EMCC) $(CFLAGS) $(QR_DECODE_INC) $(QR_DECODE_CPPFLAGS) -c $< -o $@

$(QR_DECODE_OBJ_DIR)/zxing/%.o: $(ZXING_DIR)/%.cpp | $(QR_DECODE_OBJ_DIR)
	$(EMCC) $(CFLAGS) $(QR_DECODE_INC) $(QR_DECODE_CPPFLAGS) -c $< -o $@

$(QR_DECODE_OBJ_DIR)/zxing/libzueci/%.o: $(ZXING_DIR)/libzueci/%.c | $(QR_DECODE_OBJ_DIR)
	$(EMCC) $(CFLAGS) $(QR_DECODE_INC) -c $< -o $@

.PHONY: all clean

all: $(OUT_JS) $(OUT_WASM) $(QR_OUT_JS) $(QR_OUT_WASM) $(COMPRESSION_OUT_JS) $(COMPRESSION_OUT_WASM) $(QR_DECODE_OUT_JS) $(QR_DECODE_OUT_WASM)

$(OUT_DIR):
	mkdir -p $(OUT_DIR)

$(OUT_JS) $(OUT_WASM): $(SRC) | $(OUT_DIR)
	$(EMCC) $(CFLAGS) $(INC) $(WASM_FLAGS) -o $(OUT_JS) $(SRC)

$(QR_OUT_JS) $(QR_OUT_WASM): $(QR_SRC) | $(OUT_DIR)
	$(EMCC) $(CFLAGS) $(QR_INC) $(QR_WASM_FLAGS) -o $(QR_OUT_JS) $(QR_SRC)

$(COMPRESSION_OUT_JS) $(COMPRESSION_OUT_WASM): $(COMPRESSION_SRC) | $(OUT_DIR)
	$(EMCC) $(CFLAGS) $(COMPRESSION_INC) $(COMPRESSION_WASM_FLAGS) -o $(COMPRESSION_OUT_JS) $(COMPRESSION_SRC)

$(QR_DECODE_OBJ_DIR):
	mkdir -p $(QR_DECODE_OBJ_DIR)/relic $(QR_DECODE_OBJ_DIR)/zxing/qrcode $(QR_DECODE_OBJ_DIR)/zxing/libzueci

$(QR_DECODE_OUT_JS) $(QR_DECODE_OUT_WASM): $(QR_DECODE_OBJS)
	$(EMXX) $(CFLAGS) $(QR_DECODE_WASM_FLAGS) -o $(QR_DECODE_OUT_JS) $(QR_DECODE_OBJS)

clean:
	rm -f $(OUT_JS) $(OUT_WASM) $(QR_OUT_JS) $(QR_OUT_WASM) $(COMPRESSION_OUT_JS) $(COMPRESSION_OUT_WASM) $(QR_DECODE_OUT_JS) $(QR_DECODE_OUT_WASM)
	rm -rf $(QR_DECODE_OBJ_DIR)
