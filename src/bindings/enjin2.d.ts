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

  // Scene-editor preview surface (editor_preview.cpp, Lua-free).
  // Call order per frame: injectInput() -> tick() -> getFramebuffer().
  init(): boolean;
  tick(dtSeconds: number): number;
  /** Live view into WASM memory (one 4-bit value 0-15 per pixel, row-major);
   *  copy it if you need a stable snapshot. */
  getFramebuffer(): Uint8Array;
  injectInput(buttons: number, ax0: number, ay0: number): void;

  // Scene-file surface (M2, unwn #184): versioned scene JSON (ADR-0005) run
  // by the shared enjin2::ScenePlayer rig on the 127x127 authoring canvas.
  /** Load a scene document (JSON text); dispatches scene.activate on success. */
  loadScene(jsonText: string): boolean;
  sceneActive(): boolean;
  /** Dispatch an event into the scene's tables; payloadJson "" = no payload. */
  sceneDispatch(event: string, payloadJson: string): void;
  /** Advance behavior by one fixed 16 ms frame, then render. */
  sceneTick(): void;
  /** Live view of the PACKED 127x127 Canvas4 buffer (8128 bytes, 2 px/byte,
   *  64-byte row stride) — byte-for-byte the native golden .bin payload.
   *  Copy it if you need a stable snapshot. */
  getSceneFramebuffer(): Uint8Array;
  /** Display accessor (unwn #185): live view, one 4-bit value (0-15) per
   *  pixel, row-major 127x127 — same shape as getFramebuffer(). Copy it if
   *  you need a stable snapshot. */
  getSceneFramebufferUnpacked(): Uint8Array;
  getSceneCanvasWidth(): number;
  getSceneCanvasHeight(): number;

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
