/**
 * TypeScript definitions for enjin2 WebAssembly module
 *
 * These definitions allow TypeScript to understand the API exposed
 * by the enjin2 WebAssembly module's Emscripten bindings.
 */

export interface LuaResult {
  success: boolean;
  error: string;
}

export interface LuaEngine {
  initialize(): boolean;
  shutdown(): void;
  isInitialized(): boolean;
  executeString(code: string): LuaResult;
  executeFile(filename: string): LuaResult;
  getMemoryUsage(): number;
  clearScripts(): void;
}

export interface LuaCanvas {
  getWidth(): number;
  getHeight(): number;
  is4BitCanvas(): boolean;
  clear(color: number): void;
  setPixel(x: number, y: number, color: number): void;
  getPixel(x: number, y: number): number;
  drawLine(x1: number, y1: number, x2: number, y2: number, color: number): void;
  drawRect(x: number, y: number, width: number, height: number, color: number): void;
  fillRect(x: number, y: number, width: number, height: number, color: number): void;
  drawCircle(x: number, y: number, radius: number, color: number): void;
  fillCircle(x: number, y: number, radius: number, color: number): void;
  drawTriangle(x1: number, y1: number, x2: number, y2: number, x3: number, y3: number, color: number): void;
  fillTriangle(x1: number, y1: number, x2: number, y2: number, x3: number, y3: number, color: number): void;
}

export interface LuaBindings {
  registerAll(): void;
  setCanvas(canvas: LuaCanvas): void;
  getCanvas(): LuaCanvas | null;
}

export interface LuaScriptSystem {
  initialize(): boolean;
  shutdown(): void;
  setCanvas(canvas: LuaCanvas): void;
  executeScript(code: string): LuaResult;
  loadScript(filename: string): LuaResult;
  getMemoryUsage(): number;
}

export interface Canvas4 {
  clear(color: number): void;
  setPixel(x: number, y: number, color: number): void;
  getPixel(x: number, y: number): number;
}

export interface Canvas4_64x32 {
  clear(color: number): void;
  setPixel(x: number, y: number, color: number): void;
  getPixel(x: number, y: number): number;
}

export interface Enjin2Module extends EmscriptenModule {
  // Constructors
  LuaEngine: new() => LuaEngine;
  LuaBindings: new(engine: LuaEngine) => LuaBindings;
  LuaScriptSystem: new() => LuaScriptSystem;
  Canvas4: new() => Canvas4;
  Canvas4_64x32: new() => Canvas4_64x32;

  // Factory functions
  createLuaCanvas(): LuaCanvas;
  createLuaCanvas64x32(): LuaCanvas;

  // Canvas dimension queries
  getCanvasWidth(): number;
  getCanvasHeight(): number;

  // Data access helpers
  getCanvasData(canvas: Canvas4): Uint8Array;
  getCanvasData64x32(canvas: Canvas4_64x32): Uint8Array;
  setCanvasData(canvas: Canvas4, data: Uint8Array): void;
  setCanvasData64x32(canvas: Canvas4_64x32, data: Uint8Array): void;

  // Fast drawing operations
  fastFillRect(canvas: Canvas4, x: number, y: number, w: number, h: number, color: number): void;
  fastDrawLine(canvas: Canvas4, x1: number, y1: number, x2: number, y2: number, color: number): void;
  drawPixelsBatch(canvas: Canvas4, pixels: number[]): void;
  drawLinesBatch(canvas: Canvas4, lines: number[]): void;
  fillRectsBatch(canvas: Canvas4, rects: number[]): void;

  // Palette
  getPaletteRGB(): Uint8Array;
  setPaletteColor(index: number, r: number, g: number, b: number): void;
  loadPalette(name: string): boolean;
  getPaletteSize(): number;
}

export interface EmscriptenModule {
  onRuntimeInitialized?: () => void;
  INITIAL_MEMORY?: number;
  ALLOW_MEMORY_GROWTH?: boolean;
  MAXIMUM_MEMORY?: number;
  noExitRuntime?: boolean;
  noInitialRun?: boolean;
  print?: (text: string) => void;
  printErr?: (text: string) => void;
  onAbort?: (what: any) => void;
}

declare const Enjin2ModuleFactory: (module?: Partial<EmscriptenModule>) => Promise<Enjin2Module>;

export default Enjin2ModuleFactory;
