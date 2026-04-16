#include <emscripten/bind.h>
#include <emscripten/val.h>
#include "avif/avif.h"
#include <cstdlib>

using namespace emscripten;

thread_local const val Uint8Array = val::global("Uint8Array");
thread_local const val Uint8ClampedArray = val::global("Uint8ClampedArray");
thread_local const val Uint16Array = val::global("Uint16Array");
thread_local const val ImageData = val::global("ImageData");
thread_local const val Object = val::global("Object");

val decode(val buffer, uint32_t bitDepth = 8) {
  // Copy input data from JS into WASM memory in one step (avoids std::string marshaling overhead)
  val inputArray = Uint8Array.new_(buffer);
  const size_t inputLength = inputArray["length"].as<size_t>();
  uint8_t* inputData = (uint8_t*)malloc(inputLength);
  if (!inputData) {
    return val::null();
  }

  // Single copy from JS heap to WASM linear memory
  val inputView = val(typed_memory_view(inputLength, inputData));
  inputView.call<void>("set", inputArray);

  avifImage* image = avifImageCreateEmpty();
  avifDecoder* decoder = avifDecoderCreate();
  avifResult decodeResult =
      avifDecoderReadMemory(decoder, image, inputData, inputLength);

  // image is an independent copy of decoded data, decoder may be destroyed here
  avifDecoderDestroy(decoder);
  free(inputData);

  val result = val::null();
  if (decodeResult == AVIF_RESULT_OK) {
    avifRGBImage rgb;
    avifRGBImageSetDefaults(&rgb, image);

    rgb.depth = bitDepth;

    avifRGBImageAllocatePixels(&rgb);
    avifImageYUVToRGB(image, &rgb);

    if (bitDepth != 8) {
      const size_t pixelCount = rgb.width * rgb.height;
      const size_t channelCount = 4;
      const size_t totalElements = pixelCount * channelCount;

      // Allocate JS-side Uint16Array and copy directly via set() —
      // avoids the intermediate view + slice() detachment overhead
      auto pixelArray = Uint16Array.new_(val(totalElements));
      pixelArray.call<void>("set", typed_memory_view(totalElements,
                                    reinterpret_cast<uint16_t*>(rgb.pixels)));

      result = Object.new_();
      result.set("data", pixelArray);
      result.set("width", rgb.width);
      result.set("height", rgb.height);
    } else {
      result = ImageData.new_(
          Uint8ClampedArray.new_(typed_memory_view(rgb.rowBytes * rgb.height, rgb.pixels)),
          rgb.width,
          rgb.height);
    }

    // Now we can safely free the RGB pixels:
    avifRGBImageFreePixels(&rgb);
  }

  avifImageDestroy(image);
  return result;
}

EMSCRIPTEN_BINDINGS(my_module) {
  function("decode", &decode);
}