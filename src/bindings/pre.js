/**
 * Pre-JS initialization for enjin2 WebAssembly module
 * 
 * This script runs before the main WebAssembly module loads and sets up
 * the runtime environment for enjin2's Lua scripting system.
 */

// Set up Module object if it doesn't exist
if (typeof Module === 'undefined') {
    Module = {};
}

// Memory allocation settings
Module['INITIAL_MEMORY'] = 16 * 1024 * 1024; // 16MB initial memory
Module['ALLOW_MEMORY_GROWTH'] = true;
Module['MAXIMUM_MEMORY'] = 256 * 1024 * 1024; // 64MB max memory

// Runtime configuration
Module['noExitRuntime'] = true;
Module['noInitialRun'] = true;

// Error handling
Module['onAbort'] = function (what) {
    console.error('enjin2 WebAssembly module aborted:', what);
};

Module['onRuntimeInitialized'] = function () {
    console.log('enjin2 WebAssembly module initialized successfully');

    // Don't override the module export - let Emscripten handle it
    // The module will be available as the return value of Enjin2Module()
};

// Print function for debugging
Module['print'] = function (text) {
    console.log('enjin2:', text);
};

Module['printErr'] = function (text) {
    console.error('enjin2 Error:', text);
};