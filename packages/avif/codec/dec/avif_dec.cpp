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
  // Use bit manipulation for correct conversion including denorms
  union { float f; uint32_t u; } bits;
  bits.f = value;
  uint32_t f32 = bits.u;

  uint32_t sign = (f32 >> 16) & 0x8000;
  int32_t exponent = ((f32 >> 23) & 0xFF) - 127;
  uint32_t mantissa = f32 & 0x7FFFFF;

  if (exponent > 15) {
    // Overflow → infinity
    return sign | 0x7C00;
  } else if (exponent > -15) {
    // Normal range
    uint32_t exp16 = (uint32_t)(exponent + 15) << 10;
    uint32_t man16 = mantissa >> 13;
    // Round to nearest even
    if ((mantissa & 0x1FFF) > 0x1000 ||
        ((mantissa & 0x1FFF) == 0x1000 && (man16 & 1))) {
      man16++;
      if (man16 > 0x3FF) {
        man16 = 0;
        exp16 += 0x0400;
        if (exp16 > 0x7C00) exp16 = 0x7C00; // inf
      }
    }
    return sign | exp16 | man16;
  } else if (exponent > -25) {
    // Denormalized
    mantissa |= 0x800000; // implicit leading 1
    uint32_t shift = (uint32_t)(-14 - exponent);
    uint32_t man16 = mantissa >> (shift + 13);
    return sign | man16;
  }
  // Too small → zero
  return sign;
}

// --- PQ (SMPTE ST 2084) inverse EOTF ---
// Input: PQ signal in [0,1] (normalized from N-bit depth)
// Output: linear light in [0, 10000] nits, normalized to SDR 100 nits = 1.0
static inline float pqToLinear(float pq) {
  static constexpr float m1 = 0.1593017578125f;    // 2610/16384
  static constexpr float m2 = 78.84375f;            // 2523/32 * 128
  static constexpr float c1 = 0.8359375f;           // 3424/4096
  static constexpr float c2 = 18.8515625f;          // 2413/128
  static constexpr float c3 = 18.6875f;             // 2392/128

  float p = powf(pq, 1.0f / m2);
  float num = fmaxf(p - c1, 0.0f);
  float den = c2 - c3 * p;
  // 10000 nits / 100 = 100.0 normalization factor
  float linear = powf(num / den, 1.0f / m1) * 100.0f;
  return linear;
}

val decode(val buffer, uint32_t bitDepth = 8, bool outputFloat16 = false,
           uint32_t outputColorSpace = COLOR_SPACE_NONE) {
  // Copy input data from JS into WASM memory in one step
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

    // For float16 output we always decode at the image's native depth (or at
    // least ≥10-bit) to preserve HDR precision, then convert in-place below.
    uint32_t decodeBitDepth = bitDepth;
    if (outputFloat16 && decodeBitDepth < 10) {
      // Use the image's native depth if available, otherwise 12-bit
      decodeBitDepth = (image->depth > 8) ? image->depth : 12;
    }

    rgb.depth = decodeBitDepth;

    avifRGBImageAllocatePixels(&rgb);
    avifImageYUVToRGB(image, &rgb);

    const size_t pixelCount = rgb.width * rgb.height;
    const size_t channelCount = 4;
    const size_t totalElements = pixelCount * channelCount;

    if (outputFloat16) {
      // --- Float16 output path ---
      // Allocate float16 output buffer (uint16_t stores the float16 bit pattern)
      uint16_t* f16Data = (uint16_t*)malloc(totalElements * sizeof(uint16_t));
      if (!f16Data) {
        avifRGBImageFreePixels(&rgb);
        avifImageDestroy(image);
        return val::null();
      }

      const float normFactor = 1.0f / (float)((1 << decodeBitDepth) - 1);

      if (decodeBitDepth > 8) {
        const uint16_t* src = reinterpret_cast<uint16_t*>(rgb.pixels);

        if (outputColorSpace == COLOR_SPACE_LINEAR) {
          // Detect transfer function from image CICP metadata
          bool isPQ = (image->transferCharacteristics ==
                       AVIF_TRANSFER_CHARACTERISTICS_SMPTE2084);
          // TODO: add HLG support in the future

          if (isPQ) {
            // PQ → linear for RGB, simple normalize for alpha
            for (size_t i = 0; i < pixelCount; i++) {
              const size_t off = i * 4;
              for (int c = 0; c < 3; c++) {
                float pqNorm = (float)src[off + c] * normFactor;
                float linear = pqToLinear(pqNorm);
                f16Data[off + c] = floatToFloat16(linear);
              }
              float alpha = (float)src[off + 3] * normFactor;
              f16Data[off + 3] = floatToFloat16(alpha);
            }
          } else {
            // Non-PQ: just normalize to [0,1] float16
            for (size_t i = 0; i < totalElements; i++) {
              f16Data[i] = floatToFloat16((float)src[i] * normFactor);
            }
          }
        } else {
          // COLOR_SPACE_NONE: normalize to [0,1] float16 (no EOTF)
          const uint16_t* src = reinterpret_cast<uint16_t*>(rgb.pixels);
          for (size_t i = 0; i < totalElements; i++) {
            f16Data[i] = floatToFloat16((float)src[i] * normFactor);
          }
        }
      } else {
        // 8-bit source
        const uint8_t* src = rgb.pixels;
        for (size_t i = 0; i < totalElements; i++) {
          f16Data[i] = floatToFloat16((float)src[i] * normFactor);
        }
      }

      auto pixelArray = Uint16Array.new_(val(totalElements));
      pixelArray.call<void>("set", typed_memory_view(totalElements, f16Data));

      result = Object.new_();
      result.set("data", pixelArray);
      result.set("width", rgb.width);
      result.set("height", rgb.height);
      // Report what colorSpace conversion was applied
      result.set("colorSpace",
                 outputColorSpace == COLOR_SPACE_LINEAR ? val("linear") : val("none"));
      result.set("isFloat16", val(true));

      free(f16Data);
    } else if (decodeBitDepth != 8) {
      // --- Uint16 output path (existing behavior) ---
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