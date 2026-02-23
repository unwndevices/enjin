---
id: SignalConnection
title: SignalConnection
sidebar_label: SignalConnection
---

# SignalConnection

Auto-disconnecting signal connection. 


RAII wrapper that automatically disconnects when destroyed. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/core/signal.hpp`

## Public Methods

### ` SignalConnection(Signal&lt; Args... &gt; *sig, std::function&lt; void(Args...)&gt; callback)`

Constructor. 

sigSignal to connect to callbackCallback function 

---

### ` SignalConnection(SignalConnection &&other) noexcept`

Move constructor. 

otherConnection to move from 

---

### `SignalConnection & operator=(SignalConnection &&other) noexcept`

Move assignment operator. 

otherConnection to move from Reference to this connection 

---

### ` ~SignalConnection()`

Destructor - automatically disconnects. 

---

### `void disconnect()`

Manually disconnect. 

---

### `bool isConnected() const`

Check if connection is valid. 

True if connected 

---

### ` SignalConnection(const SignalConnection &)=delete`

---

### `SignalConnection & operator=(const SignalConnection &)=delete`

---

