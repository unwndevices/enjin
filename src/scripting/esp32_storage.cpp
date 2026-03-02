#ifdef ESP32
#include "nvs_flash.h"
#include <cstring>

// Write the JSON blob to NVS under namespace "enjin2", key "store".
// len_including_null must include the null terminator.
// Returns true if nvs_set_blob AND nvs_commit both succeed.
bool esp32_storage_write(const char* json, size_t len_including_null) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("enjin2", NVS_READWRITE, &handle);
    if (err != ESP_OK) return false;
    err = nvs_set_blob(handle, "store", json, len_including_null);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return (err == ESP_OK);
}

// Read the NVS blob from namespace "enjin2", key "store" into caller-supplied buffer.
// Returns true on success, false if key absent or buffer too small or any NVS error.
bool esp32_storage_read(char* out, size_t cap) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("enjin2", NVS_READONLY, &handle);
    if (err != ESP_OK) return false;  // not found or not initialized — not an error for caller
    size_t required = 0;
    err = nvs_get_blob(handle, "store", nullptr, &required);
    if (err != ESP_OK || required == 0 || required > cap) {
        nvs_close(handle);
        return false;
    }
    err = nvs_get_blob(handle, "store", out, &required);
    nvs_close(handle);
    return (err == ESP_OK);
}
#endif
