---
id: C_ImageCache
title: C_ImageCache
sidebar_label: C_ImageCache
---

# C_ImageCache

Image cache component for sprite management. 


Provides static memory allocation for 4-bit image data with efficient caching and frame-based animation support. Based on original Enjin  design. C_ImageCacheclassenjin2_1_1C__ImageCachecompound

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/components/image_cache.hpp`

## Public Methods

### ` C_ImageCache(Object *owner)`

Constructor. 

paramownerclassenjin2_1_1Component_1a3349b9210509e148f9f7625f6a39b022memberParent object 

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

paramdeltaTimeTime since last frame in milliseconds 

---

### `static ImageEntrystructenjin2_1_1ImageEntrycompound AddImage(FileInterface &file, uint16_t width, uint16_t height, uint16_t frameCount)`

Add image to cache from file. 

paramfileFile interface to read from widthImage width in pixels heightImage height in pixels frameCountNumber of animation frames exceptionImageCacheExceptionclassenjin2_1_1ImageCacheExceptioncompoundon allocation or I/O errors return descriptor for cached image ImageEntrystructenjin2_1_1ImageEntrycompound

---

### `static void ReleaseEntry(ImageEntry &entry)`

Release cached image entry. 

paramentryImage entry to release exceptionImageCacheExceptionclassenjin2_1_1ImageCacheExceptioncompoundif entry is inactive 

---

### `static const uint8_t * GetImageData(const ImageEntry &entry, size_t frameOffset=0)`

Get pointer to image data in cache. 

paramentryImage entry descriptor frameOffsetFrame number (0-based) exceptionImageCacheExceptionclassenjin2_1_1ImageCacheExceptioncompoundon invalid access returnPointer to 4-bit image data 

---

### `static std::pair&lt; size_t, size_t &gt; GetCacheStats()`

Get cache statistics. 

returnPair of (used_bytes, total_bytes) 

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

