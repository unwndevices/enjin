---
id: FileInterface
title: FileInterface
sidebar_label: FileInterface
---

# FileInterface

Abstract file interface for platform independence. 


Allows ImageCache to work with different file systems (embedded flash, SD card, etc.) 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/components/image_cache.hpp`

## Public Methods

### `virtual  ~FileInterface()=default`


        


        

---

### `bool open()=0`

Open the file for reading. 

True if file opened successfully 

---

### `void close()=0`

Close the file. 


        

---

### `size_t read(uint8_t *buffer, size_t length)=0`

Read data from the file. 

bufferOutput buffer for read data lengthNumber of bytes to read Number of bytes actually read 

---

### `bool seek(size_t position)=0`

Seek to position in the file. 

positionByte offset from beginning of file True if seek successful 

---

### `size_t size() const =0 const`

Get file size. 

File size in bytes 

---

