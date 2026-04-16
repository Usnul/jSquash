# using libavif from https://github.com/AOMediaCodec/libavif
LIBAVIF_URL = https://github.com/AOMediaCodec/libavif/archive/refs/tags/v1.2.1.tar.gz
LIBAVIF_PACKAGE = node_modules/libavif.tar.gz

# using libaom from https://aomedia.googlesource.com/aom (encoder only)
LIBAOM_URL = https://aomedia.googlesource.com/aom/+archive/v3.11.0.tar.gz
LIBAOM_PACKAGE = node_modules/libaom.tar.gz

# using dav1d from https://code.videolan.org/videolan/dav1d (decoder only)
DAV1D_URL = https://code.videolan.org/videolan/dav1d/-/archive/1.5.1/dav1d-1.5.1.tar.gz
DAV1D_PACKAGE = node_modules/dav1d.tar.gz

export CODEC_DIR = node_modules/libavif
export BUILD_DIR = node_modules/build

# libaom source placed at ext/aom for LocalAom.cmake to find
LIBAOM_SRC = $(CODEC_DIR)/ext/aom
LIBAOM_BUILD = $(LIBAOM_SRC)/build.libavif
LIBAOM_OUT = $(LIBAOM_BUILD)/libaom.a

# dav1d source placed at ext/dav1d for LocalDav1d.cmake to find
DAV1D_SRC = $(CODEC_DIR)/ext/dav1d
DAV1D_BUILD = $(DAV1D_SRC)/build
DAV1D_OUT = $(DAV1D_BUILD)/src/libdav1d.a
EMSCRIPTEN_CROSS_FILE = $(DAV1D_SRC)/emscripten-cross.txt

override CFLAGS += "-Wno-unused-macros"

# We must build libsharpyuv from a specific libwebp commit
# See libavif/ext/libsharpyuv.cmd for more detail
LIBWEBP_URL_WITH_SHARPYUV = https://chromium.googlesource.com/webm/libwebp/+archive/e2c85878f6a33f29948b43d3492d9cdaf801aa54.tar.gz
LIBWEBP_DIR := $(CODEC_DIR)/ext/libwebp
LIBSHARPYUV_ST := $(LIBWEBP_DIR)/build_st/libsharpyuv.a
LIBSHARPYUV_MT := $(LIBWEBP_DIR)/build_mt/libsharpyuv.a
export LIBSHARPYUV := $(LIBWEBP_DIR)/build/libsharpyuv.a

OUT_ENC_JS = enc/avif_enc.js
OUT_ENC_MT_JS = enc/avif_enc_mt.js
OUT_DEC_JS = dec/avif_dec.js

OUT_ENC_CPP = enc/avif_enc.cpp
OUT_DEC_CPP = dec/avif_dec.cpp
ENVIRONMENT = web,worker

HELPER_MAKEFLAGS := -f helper.Makefile

.PHONY: all clean

all: $(OUT_ENC_JS) $(OUT_DEC_JS) $(OUT_ENC_MT_JS)

# ST-Encoding (uses libaom for encoding, no dav1d needed)
$(OUT_ENC_JS): $(OUT_ENC_CPP) $(CODEC_DIR)/CMakeLists.txt $(LIBAOM_OUT) $(LIBSHARPYUV_ST)
	mkdir -p $(LIBWEBP_DIR)/build && cp $(LIBSHARPYUV_ST) $(LIBSHARPYUV)
	$(MAKE) \
		$(HELPER_MAKEFLAGS) \
		OUT_JS=$@ \
		OUT_CPP=$< \
		STACK_SIZE=5242880 \
		INITIAL_MEMORY_SIZE=268435456 \
		ENVIRONMENT=$(ENVIRONMENT) \
		LIBAVIF_FLAGS="-DAVIF_CODEC_AOM=LOCAL -DAVIF_CODEC_AOM_DECODE=OFF -DAVIF_LIBSHARPYUV=LOCAL -DAVIF_LIBYUV=OFF" \
		AV1_CODEC_LIBS="$(abspath $(LIBAOM_OUT))" \
		EXTRA_EM_FLAGS="-s ALLOW_MEMORY_GROWTH=1"

# MT-Encoding
# We need to run the ST and MT tasks sequentially to avoid conflicts with the copy of libsharpyuv in the build directory
$(OUT_ENC_MT_JS): $(OUT_ENC_CPP) $(CODEC_DIR)/CMakeLists.txt $(LIBAOM_OUT) $(LIBSHARPYUV_MT) | $(OUT_ENC_JS)
	mkdir -p $(LIBWEBP_DIR)/build && cp $(LIBSHARPYUV_MT) $(LIBSHARPYUV)
	$(MAKE) \
		$(HELPER_MAKEFLAGS) \
		OUT_JS=$@ \
		OUT_CPP=$< \
		STACK_SIZE=5242880 \
		INITIAL_MEMORY_SIZE=268435456 \
		ENVIRONMENT=$(ENVIRONMENT) \
		LIBAVIF_FLAGS="-DAVIF_CODEC_AOM=LOCAL -DAVIF_CODEC_AOM_DECODE=OFF -DAVIF_LIBSHARPYUV=LOCAL -DAVIF_LIBYUV=OFF" \
		AV1_CODEC_LIBS="$(abspath $(LIBAOM_OUT))" \
		OUT_FLAGS="-pthread" \
		EXTRA_EM_FLAGS=""

# Decoding (uses dav1d instead of libaom for speed)
$(OUT_DEC_JS): $(OUT_DEC_CPP) $(CODEC_DIR)/CMakeLists.txt $(DAV1D_OUT)
	$(MAKE) \
		$(HELPER_MAKEFLAGS) \
		OUT_JS=$@ \
		OUT_CPP=$< \
		STACK_SIZE=5242880 \
		INITIAL_MEMORY_SIZE=16777216 \
		ENVIRONMENT=$(ENVIRONMENT) \
		LIBAVIF_FLAGS="-DAVIF_CODEC_AOM=OFF -DAVIF_CODEC_DAV1D=LOCAL -DAVIF_LIBYUV=OFF -DAVIF_LIBSHARPYUV=OFF" \
		AV1_CODEC_LIBS="$(abspath $(DAV1D_OUT))" \
		EXTRA_EM_FLAGS="-s ALLOW_MEMORY_GROWTH=1"

# LIBAOM BUILD SECTION
# Build libaom and place at ext/aom/build.libavif/ where LocalAom.cmake expects it

# Download the libaom tarball
$(LIBAOM_PACKAGE):
	mkdir -p $(@D)
	curl -sL $(LIBAOM_URL) -o $@

# Extract libaom to libavif/ext/aom
$(LIBAOM_SRC)/CMakeLists.txt: $(LIBAOM_PACKAGE) $(CODEC_DIR)/CMakeLists.txt
	mkdir -p $(LIBAOM_SRC)
	tar xzm -C $(LIBAOM_SRC) -f $(LIBAOM_PACKAGE)

# Build libaom for encoder (placed where LocalAom.cmake expects it)
$(LIBAOM_OUT): $(LIBAOM_SRC)/CMakeLists.txt
	emcmake cmake \
		-DCMAKE_BUILD_TYPE=Release \
		-DENABLE_CCACHE=0 \
		-DAOM_TARGET_CPU=generic \
		-DENABLE_DOCS=0 \
		-DENABLE_TESTS=0 \
		-DENABLE_EXAMPLES=0 \
		-DENABLE_TOOLS=0 \
		-DCONFIG_ACCOUNTING=0 \
		-DCONFIG_INSPECTION=0 \
		-DCONFIG_RUNTIME_CPU_DETECT=0 \
		-DCONFIG_WEBM_IO=0 \
		-DCONFIG_AV1_DECODER=0 \
		-DCONFIG_AV1_HIGHBITDEPTH=1 \
		-DCMAKE_C_FLAGS="-msimd128" \
		-DCMAKE_CXX_FLAGS="-msimd128" \
		-B $(LIBAOM_BUILD) \
		$(LIBAOM_SRC) && \
	$(MAKE) -C $(LIBAOM_BUILD)

# DAV1D BUILD SECTION
# Build dav1d and place at ext/dav1d/build/ where LocalDav1d.cmake expects it

# Download the dav1d tarball
$(DAV1D_PACKAGE):
	mkdir -p $(@D)
	curl -sL $(DAV1D_URL) -o $@

# Extract dav1d to libavif/ext/dav1d
$(DAV1D_SRC)/meson.build: $(DAV1D_PACKAGE) $(CODEC_DIR)/CMakeLists.txt
	mkdir -p $(DAV1D_SRC)
	tar xzm --strip 1 -C $(DAV1D_SRC) -f $(DAV1D_PACKAGE)

# Build dav1d with meson + emscripten cross-compilation
$(DAV1D_OUT): $(DAV1D_SRC)/meson.build
	@echo "=== Building dav1d for WASM ==="
	printf '[binaries]\nc = '"'"'emcc'"'"'\ncpp = '"'"'em++'"'"'\nar = '"'"'emar'"'"'\nstrip = '"'"'emstrip'"'"'\n\n[host_machine]\nsystem = '"'"'emscripten'"'"'\ncpu_family = '"'"'wasm32'"'"'\ncpu = '"'"'wasm32'"'"'\nendian = '"'"'little'"'"'\n\n[built-in options]\nc_args = ['"'"'-msimd128'"'"', '"'"'-flto'"'"']\n' > $(EMSCRIPTEN_CROSS_FILE)
	meson setup $(DAV1D_BUILD) $(DAV1D_SRC) \
		--cross-file $(EMSCRIPTEN_CROSS_FILE) \
		--default-library=static \
		-Denable_tools=false \
		-Denable_tests=false \
		-Denable_asm=false \
		--buildtype=release
	ninja -C $(DAV1D_BUILD)

# LIBAVIF EXTRACTION SECTION

# Download the libavif tarball
$(LIBAVIF_PACKAGE):
	mkdir -p $(@D)
	curl -sL $(LIBAVIF_URL) -o $@

# Extract libavif from the tarball
$(CODEC_DIR)/CMakeLists.txt: $(LIBAVIF_PACKAGE)
	mkdir -p $(@D)
	tar xzm --strip 1 -C $(@D) -f $(LIBAVIF_PACKAGE)

# Create libavif/ext/libwebp
$(LIBWEBP_DIR)/CMakeLists.txt: $(CODEC_DIR)/CMakeLists.txt
	mkdir -p $(LIBWEBP_DIR)
	curl -sL $(LIBWEBP_URL_WITH_SHARPYUV) \
		| tar xzm -C $(LIBWEBP_DIR)

# Make libsharpyuv.a for ST-Encoding
$(LIBSHARPYUV_ST): $(LIBWEBP_DIR)/CMakeLists.txt
	mkdir -p $(@D)
	emcmake cmake \
		-DWEBP_USE_THREAD=OFF \
		-DWEBP_BUILD_ANIM_UTILS=OFF \
		-DWEBP_BUILD_CWEBP=OFF \
		-DWEBP_BUILD_DWEBP=OFF \
		-DWEBP_BUILD_GIF2WEBP=OFF \
		-DWEBP_BUILD_IMG2WEBP=OFF \
		-DWEBP_BUILD_VWEBP=OFF \
		-DWEBP_BUILD_WEBPINFO=OFF \
		-DWEBP_BUILD_LIBWEBPMUX=OFF \
		-DWEBP_BUILD_WEBPMUX=OFF \
		-DWEBP_BUILD_EXTRAS=OFF \
		-DBUILD_SHARED_LIBS=OFF \
		-DCMAKE_BUILD_TYPE=Release \
		-S $(LIBWEBP_DIR) \
		-B $(@D)
	$(MAKE) -C $(@D) sharpyuv

# Make libsharpyuv.a for MT-Encoding
$(LIBSHARPYUV_MT): $(LIBWEBP_DIR)/CMakeLists.txt
	mkdir -p $(@D)
	emcmake cmake \
		-DWEBP_BUILD_ANIM_UTILS=OFF \
		-DWEBP_BUILD_CWEBP=OFF \
		-DWEBP_BUILD_DWEBP=OFF \
		-DWEBP_BUILD_GIF2WEBP=OFF \
		-DWEBP_BUILD_IMG2WEBP=OFF \
		-DWEBP_BUILD_VWEBP=OFF \
		-DWEBP_BUILD_WEBPINFO=OFF \
		-DWEBP_BUILD_LIBWEBPMUX=OFF \
		-DWEBP_BUILD_WEBPMUX=OFF \
		-DWEBP_BUILD_EXTRAS=OFF \
		-DBUILD_SHARED_LIBS=OFF \
		-DCMAKE_BUILD_TYPE=Release \
		-S $(LIBWEBP_DIR) \
		-B $(@D)
	$(MAKE) -C $(@D) sharpyuv

clean:
	$(MAKE) $(HELPER_MAKEFLAGS) OUT_JS=$(OUT_DEC_JS) clean
	$(MAKE) $(HELPER_MAKEFLAGS) OUT_JS=$(OUT_ENC_JS) clean
	$(MAKE) $(HELPER_MAKEFLAGS) OUT_JS=$(OUT_ENC_MT_JS) clean
