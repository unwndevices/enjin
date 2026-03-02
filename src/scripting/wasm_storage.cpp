#ifdef __EMSCRIPTEN__
#include <emscripten/em_js.h>

// Write the JSON blob to localStorage under key 'enjin2_store'.
EM_JS(void, wasm_storage_write, (const char* json_ptr), {
    localStorage.setItem('enjin2_store', UTF8ToString(json_ptr));
});

// Read localStorage['enjin2_store'] into caller-supplied buffer.
// Returns 1 on success, 0 if key absent or buffer too small.
EM_JS(int, wasm_storage_read, (char* out_ptr, int out_cap), {
    var val = localStorage.getItem('enjin2_store');
    if (val === null) { return 0; }
    var encoded_len = lengthBytesUTF8(val);
    if (encoded_len + 1 > out_cap) { return 0; }
    stringToUTF8(val, out_ptr, out_cap);
    return 1;
});
#endif
