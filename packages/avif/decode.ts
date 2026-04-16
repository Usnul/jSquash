/**
 * Copyright 2020 Google Inc. All Rights Reserved.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * Notice: I (Jamie Sinclair) have copied this code from the JPEG encode module
 * and modified it to decode JPEG images.
 */

import type { AVIFModule, DecodeResult16 } from './codec/dec/avif_dec.js';
import { initEmscriptenModule } from './utils.js';

import avif_dec from './codec/dec/avif_dec.js';
import { ImageData16bit } from 'meta.js';

let emscriptenModule: Promise<AVIFModule>;

export async function init(
  moduleOptionOverrides?: Partial<EmscriptenWasm.ModuleOpts>,
): Promise<void>;
export async function init(
  module?: WebAssembly.Module,
  moduleOptionOverrides?: Partial<EmscriptenWasm.ModuleOpts>,
): Promise<void> {
  let actualModule: WebAssembly.Module | undefined = module;
  let actualOptions: Partial<EmscriptenWasm.ModuleOpts> | undefined =
    moduleOptionOverrides;

  // If only one argument is provided and it's not a WebAssembly.Module
  if (arguments.length === 1 && !(module instanceof WebAssembly.Module)) {
    actualModule = undefined;
    actualOptions = module as unknown as Partial<EmscriptenWasm.ModuleOpts>;
  }

  emscriptenModule = initEmscriptenModule(
    avif_dec,
    actualModule,
    actualOptions,
  );
}

/**
 * Output color space for the decoder.
 * Controls what transfer function (EOTF) conversion is applied.
 */
export const enum OutputColorSpace {
  /** No conversion — output raw decoded values, normalized. */
  NONE = 0,
  /**
   * Convert to linear light.
   * For PQ-encoded images: applies inverse PQ EOTF (ST.2084), output in
   * units where 100 nits = 1.0. HDR highlights will exceed 1.0.
   * For non-PQ images: simple normalization to [0,1].
   */
  LINEAR = 1,
}

type DecodeOptions = {
  /** Decode bit depth (per-channel). Default: 8 */
  bitDepth?: 8 | 10 | 12 | 16;
  /**
   * When true, output pixel data as IEEE 754 float16 stored in a Uint16Array.
   * This is useful for HDR pipelines that need linear-light float data
   * (e.g. uploading directly to a WebGPU rgba16float texture).
   *
   * When combined with `outputColorSpace: OutputColorSpace.LINEAR`, PQ images
   * will be decoded to linear light with values where 100 nits = 1.0.
   *
   * Default: false
   */
  outputFloat16?: boolean;
  /**
   * Controls the output color space / transfer function conversion.
   * Only meaningful when `outputFloat16` is true.
   * Default: OutputColorSpace.NONE
   */
  outputColorSpace?: OutputColorSpace;
};

// Overload: default / 8-bit → ImageData
export default async function decode(
  buffer: ArrayBuffer,
): Promise<ImageData | null>;
export default async function decode(
  buffer: ArrayBuffer,
  options: { bitDepth?: 8; outputFloat16?: false },
): Promise<ImageData | null>;

// Overload: >8-bit without float16 → ImageData16bit
export default async function decode(
  buffer: ArrayBuffer,
  options: { bitDepth: 10 | 12 | 16; outputFloat16?: false },
): Promise<ImageData16bit | null>;

// Overload: float16 → DecodeResult16 (always — regardless of bitDepth)
export default async function decode(
  buffer: ArrayBuffer,
  options: { outputFloat16: true; bitDepth?: 8 | 10 | 12 | 16; outputColorSpace?: OutputColorSpace },
): Promise<DecodeResult16 | null>;

// Implementation
export default async function decode(
  buffer: ArrayBuffer,
  options?: DecodeOptions,
): Promise<ImageData | ImageData16bit | DecodeResult16 | null> {
  if (!emscriptenModule) {
    init();
  }

  const module = await emscriptenModule;
  const bitDepth = options?.bitDepth ?? 8;
  const outputFloat16 = options?.outputFloat16 ?? false;
  const outputColorSpace = options?.outputColorSpace ?? OutputColorSpace.NONE;

  const result = module.decode(buffer, bitDepth, outputFloat16, outputColorSpace);
  if (!result) throw new Error('Decoding error');
  return result;
}
