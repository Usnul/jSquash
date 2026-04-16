# This is a helper Makefile for building LibAVIF with given params and linking
# the final WASM module.
#
# Params that must be supplied by the caller:
#   $(CODEC_DIR)       - path to libavif source
#   $(BUILD_DIR)       - build output root
#   $(OUT_JS)          - output JS file (e.g. enc/avif_enc.js or dec/avif_dec.js)
#   $(OUT_CPP)         - C++ wrapper source
#   $(LIBAVIF_FLAGS)   - CMake flags for libavif (codec selection, etc.)
#   $(ENVIRONMENT)     - emscripten environment targets
#   $(AV1_CODEC_LIBS)  - pre-built codec .a file(s) to link with

# $(OUT_JS) is something like "enc/avif_enc.js" or "dec/avif_dec.js"
# so $(OUT_BUILD_DIR) will be "node_modules/build/enc/avif_enc" etc.
OUT_BUILD_DIR := $(BUILD_DIR)/$(basename $(OUT_JS))

CODEC_BUILD_DIR := $(OUT_BUILD_DIR)/libavif
CODEC_OUT := $(CODEC_BUILD_DIR)/libavif.a

OUT_WASM = $(OUT_JS:.js=.wasm)
OUT_WORKER=$(OUT_JS:.js=.worker.js)

PRE_JS = pre.js

.PHONY: all clean

all: $(OUT_JS)

# Only add libsharpyuv as a dependency for encoders.
ifneq (,$(findstring enc/, $(OUT_JS)))
$(OUT_JS): $(LIBSHARPYUV)
$(CODEC_OUT): $(LIBSHARPYUV)
endif

# Link the final WASM module
$(OUT_JS): $(OUT_CPP) $(AV1_CODEC_LIBS) $(CODEC_OUT)
	$(CXX) \
		-I $(CODEC_DIR)/include \
		$(CXXFLAGS) \
		$(LDFLAGS) \
		$(OUT_FLAGS) \
		-msimd128 \
		--pre-js $(PRE_JS) \
		--bind \
		-s ERROR_ON_UNDEFINED_SYMBOLS=0 \
		-s ENVIRONMENT=$(ENVIRONMENT) \
		-s EXPORT_ES6=1 \
		-s DYNAMIC_EXECUTION=0 \
		-s MODULARIZE=1 \
		-s TOTAL_STACK=$(STACK_SIZE) \
		-s INITIAL_MEMORY=$(INITIAL_MEMORY_SIZE) \
		$(EXTRA_EM_FLAGS) \
		-o $@ \
		$+

# Build libavif.
# The caller controls which codecs are enabled via LIBAVIF_FLAGS:
#   Encoder: -DAVIF_CODEC_AOM=LOCAL (finds pre-built libaom at ext/aom/build.libavif/)
#   Decoder: -DAVIF_CODEC_DAV1D=LOCAL (finds pre-built dav1d at ext/dav1d/build/)
$(CODEC_OUT): $(CODEC_DIR)/CMakeLists.txt $(AV1_CODEC_LIBS)
	emcmake cmake \
		-DCMAKE_BUILD_TYPE=Release \
		-DBUILD_SHARED_LIBS=0 \
		-DCMAKE_C_FLAGS="$(OUT_FLAGS) -msimd128" \
		-DCMAKE_CXX_FLAGS="$(OUT_FLAGS) -msimd128" \
		$(LIBAVIF_FLAGS) \
		-B $(CODEC_BUILD_DIR) \
		$(CODEC_DIR) && \
	$(MAKE) -C $(CODEC_BUILD_DIR)

clean:
	$(RM) $(OUT_JS) $(OUT_WASM) $(OUT_WORKER)
	-$(MAKE) -C $(CODEC_BUILD_DIR) clean