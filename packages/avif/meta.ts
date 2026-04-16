import {
  EncodeOptions as RawEncodeOptions,
  AVIFTune,
} from './codec/enc/avif_enc.js';

export { AVIFTune };

/** ITU-T H.273 Color Primaries (subset of common values) */
export const enum AVIFColorPrimaries {
  UNSPECIFIED = 0,
  BT709 = 1,       // sRGB / Rec.709
  BT2020 = 9,      // HDR / Wide Color Gamut
  P3 = 12,         // Display P3
}

/** ITU-T H.273 Transfer Characteristics (subset of common values) */
export const enum AVIFTransferCharacteristics {
  UNSPECIFIED = 0,
  BT709 = 1,       // Rec.709
  SRGB = 13,       // sRGB EOTF
  PQ = 16,         // SMPTE ST 2084 (Perceptual Quantizer) - HDR10
  HLG = 18,        // Hybrid Log-Gamma - HLG HDR
}

/** ITU-T H.273 Matrix Coefficients (subset of common values) */
export const enum AVIFMatrixCoefficients {
  IDENTITY = 0,    // RGB / lossless
  BT709 = 1,       // Rec.709
  BT601 = 6,       // BT.601 (current default for lossy)
  BT2020_NCL = 9,  // BT.2020 non-constant luminance
}

export type EncodeOptions = RawEncodeOptions & {
  lossless: boolean;
};

export type ImageData16bit = {
  data: Uint16Array;
  width: number;
  height: number;
};

/** Result type when outputFloat16 is true — data contains IEEE 754 float16 bit patterns */
export type { DecodeResult16 } from './codec/dec/avif_dec.js';

export const label = 'AVIF';
export const mimeType = 'image/avif';
export const extension = 'avif';
export const defaultOptions: EncodeOptions = {
  quality: 50,
  qualityAlpha: -1,
  denoiseLevel: 0,
  tileColsLog2: 0,
  tileRowsLog2: 0,
  speed: 6,
  subsample: 1,
  chromaDeltaQ: false,
  sharpness: 0,
  tune: AVIFTune.auto,
  enableSharpYUV: false,
  bitDepth: 8,
  lossless: false,
  colorPrimaries: 0,          // 0 = use libavif default / unspecified
  transferCharacteristics: 0, // 0 = use libavif default / unspecified
  matrixCoefficients: 0,      // 0 = use legacy behavior (BT601 lossy / Identity lossless)
  clli_maxCLL: 0,             // 0 = unset
  clli_maxPALL: 0,            // 0 = unset
};

