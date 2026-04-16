#include <emscripten/bind.h>
#include <emscripten/val.h>
#include "avif/avif.h"
#include <cstdlib>
#include <cmath>
#include <cstdint>

using namespace emscripten;

thread_local const val Uint8Array = val::global("Uint8Array");
thread_local const val Uint8ClampedArray = val::global("Uint8ClampedArray");
thread_local const val Uint16Array = val::global("Uint16Array");
thread_local const val ImageData = val::global("ImageData");
thread_local const val Object = val::global("Object");

// Output color space constants (matches OutputColorSpace enum in TS)
static constexpr uint32_t COLOR_SPACE_NONE = 0;
static constexpr uint32_t COLOR_SPACE_LINEAR = 1;

// --- Float16 conversion ---
// IEEE 754 half-precision: 1 sign, 5 exponent, 10 mantissa
static inline uint16_t floatToFloat16(float value) {
  union { float f; uint32_t u; } bits;
  bits.f = value;
  uint32_t f32 = bits.u;

  uint32_t sign = (f32 >> 16) & 0x8000;
  int32_t exponent = ((f32 >> 23) & 0xFF) - 127;
  uint32_t mantissa = f32 & 0x7FFFFF;

  if (exponent > 15) {
    return sign | 0x7C00; // infinity
  } else if (exponent > -15) {
    uint32_t exp16 = (uint32_t)(exponent + 15) << 10;
    uint32_t man16 = mantissa >> 13;
    if ((mantissa & 0x1FFF) > 0x1000 ||
        ((mantissa & 0x1FFF) == 0x1000 && (man16 & 1))) {
      man16++;
      if (man16 > 0x3FF) {
        man16 = 0;
        exp16 += 0x0400;
        if (exp16 > 0x7C00) exp16 = 0x7C00;
      }
    }
    return sign | exp16 | man16;
  } else if (exponent > -25) {
    mantissa |= 0x800000;
    uint32_t shift = (uint32_t)(-14 - exponent);
    uint32_t man16 = mantissa >> (shift + 13);
    return sign | man16;
  }
  return sign;
}

// --- PQ (SMPTE ST 2084) inverse EOTF ---
// Input: PQ signal in [0,1]
// Output: linear light normalized so 100 nits = 1.0
static inline float pqToLinear(float pq) {
  static constexpr float m1 = 0.1593017578125f;
  static constexpr float m2 = 78.84375f;
  static constexpr float c1 = 0.8359375f;
  static constexpr float c2 = 18.8515625f;
  static constexpr float c3 = 18.6875f;

  float p = powf(pq, 1.0f / m2);
  float num = fmaxf(p - c1, 0.0f);
  float den = c2 - c3 * p;
  return powf(num / den, 1.0f / m1) * 100.0f;
}

// --- LUT builder ---
// For N-bit input, build a lookup table mapping every possible integer value
// directly to its float16 output. This eliminates per-pixel powf() calls.
static void buildPQtoLinearF16LUT(uint16_t* lut, uint32_t bitDepth) {
  const uint32_t maxVal = (1u << bitDepth) - 1;
  const float norm = 1.0f / (float)maxVal;
  for (uint32_t i = 0; i <= maxVal; i++) {
    lut[i] = floatToFloat16(pqToLinear((float)i * norm));
  }
}

static void buildNormalizeF16LUT(uint16_t* lut, uint32_t bitDepth) {
  const uint32_t maxVal = (1u << bitDepth) - 1;
  const float norm = 1.0f / (float)maxVal;
  for (uint32_t i = 0; i <= maxVal; i++) {
    lut[i] = floatToFloat16((float)i * norm);
  }
}

val decode(val buffer, uint32_t bitDepth, bool outputFloat16,
           uint32_t outputColorSpace) {
  // Copy input data from JS into WASM memory
  val inputArray = Uint8Array.new_(buffer);
  const size_t inputLength = inputArray["length"].as<size_t>();
  uint8_t* inputData = (uint8_t*)malloc(inputLength);
  if (!inputData) {
    return val::null();
  }

  val inputView = val(typed_memory_view(inputLength, inputData));
  inputView.call<void>("set", inputArray);

  avifImage* image = avifImageCreateEmpty();
  avifDecoder* decoder = avifDecoderCreate();
  avifResult decodeResult =
      avifDecoderReadMemory(decoder, image, inputData, inputLength);

  avifDecoderDestroy(decoder);
  free(inputData);

  val result = val::null();
  if (decodeResult == AVIF_RESULT_OK) {
    avifRGBImage rgb;
    avifRGBImageSetDefaults(&rgb, image);

    // For float16 output, decode at the image's native depth to preserve HDR
    uint32_t decodeBitDepth = bitDepth;
    if (outputFloat16 && decodeBitDepth < 10) {
      decodeBitDepth = (image->depth > 8) ? image->depth : 12;
    }

    rgb.depth = decodeBitDepth;

    (void)avifRGBImageAllocatePixels(&rgb);
    (void)avifImageYUVToRGB(image, &rgb);

    const size_t pixelCount = rgb.width * rgb.height;
    const size_t totalElements = pixelCount * 4;

    if (outputFloat16) {
      // --- Float16 output path with LUT optimization ---
      uint16_t* f16Data = (uint16_t*)malloc(totalElements * sizeof(uint16_t));
      if (!f16Data) {
        avifRGBImageFreePixels(&rgb);
        avifImageDestroy(image);
        return val::null();
      }

      if (decodeBitDepth > 8) {
        const uint16_t* src = reinterpret_cast<uint16_t*>(rgb.pixels);
        const uint32_t lutSize = (1u << decodeBitDepth);

        // Build LUTs once — 4096 entries for 12-bit costs ~40ms vs ~600ms per-pixel
        uint16_t* rgbLUT = (uint16_t*)malloc(lutSize * sizeof(uint16_t));
        uint16_t* alphaLUT = (uint16_t*)malloc(lutSize * sizeof(uint16_t));

        bool isPQ = (outputColorSpace == COLOR_SPACE_LINEAR) &&
                    (image->transferCharacteristics ==
                     AVIF_TRANSFER_CHARACTERISTICS_SMPTE2084);

        if (isPQ) {
          buildPQtoLinearF16LUT(rgbLUT, decodeBitDepth);
        } else {
          buildNormalizeF16LUT(rgbLUT, decodeBitDepth);
        }
        buildNormalizeF16LUT(alphaLUT, decodeBitDepth);

        // Hot loop: pure table lookup, no floating-point math
        if (isPQ) {
          for (size_t i = 0; i < pixelCount; i++) {
            const size_t off = i * 4;
            f16Data[off + 0] = rgbLUT[src[off + 0]];
            f16Data[off + 1] = rgbLUT[src[off + 1]];
            f16Data[off + 2] = rgbLUT[src[off + 2]];
            f16Data[off + 3] = alphaLUT[src[off + 3]];
          }
        } else {
          // Non-PQ: same LUT for all channels
          for (size_t i = 0; i < totalElements; i++) {
            f16Data[i] = rgbLUT[src[i]];
          }
        }

        free(rgbLUT);
        free(alphaLUT);
      } else {
        // 8-bit source: 256-entry LUT
        const uint8_t* src = rgb.pixels;
        uint16_t lut256[256];
        buildNormalizeF16LUT(lut256, 8);
        for (size_t i = 0; i < totalElements; i++) {
          f16Data[i] = lut256[src[i]];
        }
      }

      auto pixelArray = Uint16Array.new_(val(totalElements));
      pixelArray.call<void>("set", typed_memory_view(totalElements, f16Data));

      result = Object.new_();
      result.set("data", pixelArray);
      result.set("width", rgb.width);
      result.set("height", rgb.height);
      result.set("colorSpace",
                 outputColorSpace == COLOR_SPACE_LINEAR ? val("linear") : val("none"));
      result.set("isFloat16", val(true));

      free(f16Data);
    } else if (decodeBitDepth != 8) {
      // --- Uint16 output (existing behavior) ---
      auto pixelArray = Uint16Array.new_(val(totalElements));
      pixelArray.call<void>("set", typed_memory_view(totalElements,
                                    reinterpret_cast<uint16_t*>(rgb.pixels)));

      result = Object.new_();
      result.set("data", pixelArray);
      result.set("width", rgb.width);
      result.set("height", rgb.height);
    } else {
      // --- 8-bit ImageData output (existing behavior) ---
      result = ImageData.new_(
          Uint8ClampedArray.new_(typed_memory_view(rgb.rowBytes * rgb.height, rgb.pixels)),
          rgb.width,
          rgb.height);
    }

    avifRGBImageFreePixels(&rgb);
  }

  avifImageDestroy(image);
  return result;
}

EMSCRIPTEN_BINDINGS(my_module) {
  function("decode", &decode);
}