---
id: C_ImageCache
title: C_ImageCache
sidebar_label: C_ImageCache
---

# C_ImageCache

Image cache component for sprite management. 


Provides static memory allocation for 4-bit image data with efficient caching and frame-based animation support. Based on original Enjin  design. C_ImageCache

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/components/image_cache.hpp`

## Public Methods

### ` C_ImageCache(Object *owner)`

Constructor. 

ownerParent object 

---

### ` ~C_ImageCache()=default`

Destructor. 


        

---

### `virtual void awake() override`

Awake is called when the component is created. 

Use this for initialization that doesn't depend on other components. This is called before Start(). 

---

### `virtual void start() override`

Start is called before the first frame update. 

Use this for initialization that depends on other components or objects being fully set up. 

---

### `virtual void update(uint16_t deltaTime) override`

Update is called once per frame. 

deltaTimeTime since last frame in milliseconds 

---

### `static ImageEntry AddImage(FileInterface &file, uint16_t width, uint16_t height, uint16_t frameCount)`

Add image to cache from file. 

fileFile interface to read from widthImage width in pixels heightImage height in pixels frameCountNumber of animation frames ImageCacheExceptionon allocation or I/O errors  descriptor for cached image ImageEntry

---

### `static void ReleaseEntry(ImageEntry &entry)`

Release cached image entry. 

entryImage entry to release ImageCacheExceptionif entry is inactive 

---

### `static const uint8_t * GetImageData(const ImageEntry &entry, size_t frameOffset=0)`

Get pointer to image data in cache. 

entryImage entry descriptor frameOffsetFrame number (0-based) ImageCacheExceptionon invalid access Pointer to 4-bit image data 

---

### `static std::pair&lt; size_t, size_t &gt; GetCacheStats()`

Get cache statistics. 

Pair of (used_bytes, total_bytes) 

---

### `static void ClearCache()`

Clear entire cache. 


        

---

## Private Methods

### `static size_t FindFreeSpace(size_t requiredSize)`


        


        

---

### `static void ValidateImageParameters(uint16_t width, uint16_t height, uint16_t frameCount, size_t fileSize)`


        


        

---

### `static void SerialLog(const char *message)`


        


        

---

### `static void InitializeCache()`


        


        

---

