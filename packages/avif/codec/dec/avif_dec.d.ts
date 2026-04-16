export interface DecodeResult16 {
  data: Uint16Array;
  height: number;
  width: number;
  colorSpace?: string;
  isFloat16?: boolean;
}

export interface AVIFModule extends EmscriptenWasm.Module {
  decode(data: BufferSource, bitDepth: 8, outputFloat16: false, outputColorSpace: number): ImageData | null;
  decode(data: BufferSource, bitDepth: 10 | 12 | 16, outputFloat16: false, outputColorSpace: number): DecodeResult16 | null;
  decode(data: BufferSource, bitDepth: number, outputFloat16: true, outputColorSpace: number): DecodeResult16 | null;
  decode(data: BufferSource, bitDepth: number, outputFloat16: boolean, outputColorSpace: number): DecodeResult16 | ImageData | null;
}

declare var moduleFactory: EmscriptenWasm.ModuleFactory<AVIFModule>;

export default moduleFactory;
