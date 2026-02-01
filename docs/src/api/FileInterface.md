---
id: FileInterface
title: FileInterface
sidebar_label: FileInterface
---

# FileInterface

Abstract file interface for platform independence. 


Allows ImageCache to work with different file systems (embedded flash, SD card, etc.) 

---

**Namespace:** enjin2

**Header:** include/enjin2/components/image_cache.hpp

## Public Methods

### `cpp
*virtual  ~FileInterface()=default*
``


        


        

---

### `cpp
*bool open()=0*
``


        


        

---

### `cpp
*void close()=0*
``


        


        

---

### `cpp
*size_t read(uint8_t *buffer, size_t length)=0*
``


        


        

---

### `cpp
*bool seek(size_t position)=0*
``


        


        

---

### `cpp
*size_t size() const =0 const*
``


        


        

---

