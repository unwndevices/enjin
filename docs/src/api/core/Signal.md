---
id: Signal
title: Signal
sidebar_label: Signal
---

# Signal

 class for observer pattern implementation. Signalclassenjin2_1_1Signalcompound


Provides a lightweight event system using static allocation.  can be emitted with parameters and connected to callbacks. Signalsnamespaceenjin2_1_1Signalscompound

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/core/signal.hpp`

## Public Methods

### `` Signal()``

Constructor. 


        

---

### ``int connect(CallbackType callback)``

Connect a callback to this signal. 

paramcallbackFunction to call when signal is emitted returnConnection ID (can be used to disconnect) or -1 if failed 

---

### ``void disconnect(int connectionId)``

Disconnect a callback by connection ID. 

paramconnectionIdConnection ID returned by  connect()classenjin2_1_1Signal_1a3842d5435fcb3bef84a5eb16531e7aabmember

---

### ``void emit(Args... args)``

Emit the signal, calling all connected callbacks. 

paramargsArguments to pass to callbacks 

---

### ``void disconnectAll()``

Disconnect all callbacks. 


        

---

### ``size_t getConnectionCount() const const``

Get number of active connections. 

returnNumber of connected callbacks 

---

### ``bool hasConnections() const const``

Check if signal has any connections. 

returnTrue if there are active connections 

---

### ``void operator()(Args... args)``

Operator() for emitting signals. 

paramargsArguments to pass to callbacks 

---

