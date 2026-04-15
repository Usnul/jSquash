export const enum AVIFTune {
  auto,
  psnr,
  ssim,
}

export interface EncodeOptions {
  quality: number;
  qualityAlpha: number;
  denoiseLevel: number;
  tileRowsLog2: number;
  tileColsLog2: number;
  speed: number;
  subsample: number;
  chromaDeltaQ: boolean;
  sharpness: number;
  enableSharpYUV: boolean;
  tune: AVIFTune;
  bitDepth: number;
  colorPrimaries: number;
  transferCharacteristics: number;
  matrixCoefficients: number;
  clli_maxCLL: number;
  clli_maxPALL: number;
}

export interface AVIFModule extends EmscriptenWasm.Module {
  encode(
    data: BufferSource,
    width: number,
    height: number,
    options: EncodeOptions,
  ): Uint8Array | null;
}

declare var moduleFactory: EmscriptenWasm.ModuleFactory<AVIFModule>;

export default moduleFactory;
