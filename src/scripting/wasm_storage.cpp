#ifdef __EMSCRIPTEN__
#include <emscripten/em_js.h>

// The localStorage key for a store namespace: 'enjin2_store' when unscoped,
// 'enjin2_store:<ns>' for one owner (an applet id, Tomodachi #244).

// Write the JSON blob to localStorage under the namespace's key.
EM_JS(void, wasm_storage_write, (const char* ns_ptr, const char* json_ptr), {
    var ns = UTF8ToString(ns_ptr);
    localStorage.setItem(ns ? 'enjin2_store:' + ns : 'enjin2_store', UTF8ToString(json_ptr));
});

// Read the namespace's localStorage blob into caller-supplied buffer.
// Returns 1 on success, 0 if key absent or buffer too small.
EM_JS(int, wasm_storage_read, (const char* ns_ptr, char* out_ptr, int out_cap), {
    var ns = UTF8ToString(ns_ptr);
    var val = localStorage.getItem(ns ? 'enjin2_store:' + ns : 'enjin2_store');
    if (val === null) { return 0; }
    var encoded_len = lengthBytesUTF8(val);
    if (encoded_len + 1 > out_cap) { return 0; }
    stringToUTF8(val, out_ptr, out_cap);
    return 1;
});
#endif
