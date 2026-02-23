#pragma once

#include <array>
#include <functional>

namespace enjin2 {

/**
 * @brief Signal class for observer pattern implementation
 * 
 * Provides a lightweight event system using static allocation.
 * Signals can be emitted with parameters and connected to callbacks.
 */
template<typename... Args>
class Signal {
private:
    static constexpr size_t MAX_CONNECTIONS = 16;  ///< Maximum connections per signal
    
    using CallbackType = std::function<void(Args...)>;
    
    std::array<CallbackType, MAX_CONNECTIONS> callbacks;
    std::array<bool, MAX_CONNECTIONS> active;
    size_t connectionCount;
    
public:
    /**
     * @brief Constructor
     */
    Signal() : connectionCount(0) {
        active.fill(false);
    }
    
    /**
     * @brief Connect a callback to this signal
     * @param callback Function to call when signal is emitted
     * @return Connection ID (can be used to disconnect) or -1 if failed
     */
    int connect(CallbackType callback) {
        // Find an empty slot
        for (size_t i = 0; i < MAX_CONNECTIONS; ++i) {
            if (!active[i]) {
                callbacks[i] = std::move(callback);
                active[i] = true;
                if (i >= connectionCount) {
                    connectionCount = i + 1;
                }
                return static_cast<int>(i);
            }
        }
        return -1; // No space available
    }
    
    /**
     * @brief Disconnect a callback by connection ID
     * @param connectionId Connection ID returned by connect()
     */
    void disconnect(int connectionId) {
        if (connectionId >= 0 && connectionId < static_cast<int>(MAX_CONNECTIONS)) {
            active[connectionId] = false;
            callbacks[connectionId] = nullptr;
            
            // Update connection count if we removed the last connection
            if (connectionId == static_cast<int>(connectionCount - 1)) {
                while (connectionCount > 0 && !active[connectionCount - 1]) {
                    connectionCount--;
                }
            }
        }
    }
    
    /**
     * @brief Emit the signal, calling all connected callbacks
     * @param args Arguments to pass to callbacks
     */
    void emit(Args... args) {
        for (size_t i = 0; i < connectionCount; ++i) {
            if (active[i] && callbacks[i]) {
                callbacks[i](args...);
            }
        }
    }
    
    /**
     * @brief Disconnect all callbacks
     */
    void disconnectAll() {
        active.fill(false);
        connectionCount = 0;
        for (auto& callback : callbacks) {
            callback = nullptr;
        }
    }
    
    /**
     * @brief Get number of active connections
     * @return Number of connected callbacks
     */
    size_t getConnectionCount() const {
        size_t count = 0;
        for (size_t i = 0; i < connectionCount; ++i) {
            if (active[i]) {
                count++;
            }
        }
        return count;
    }
    
    /**
     * @brief Check if signal has any connections
     * @return True if there are active connections
     */
    bool hasConnections() const {
        return getConnectionCount() > 0;
    }
    
    /**
     * @brief Operator() for emitting signals
     * @param args Arguments to pass to callbacks
     */
    void operator()(Args... args) {
        emit(args...);
    }
};

/**
 * @brief Simple signal with no parameters
 */
using SimpleSignal = Signal<>;

/**
 * @brief Signal with single parameter
 */
template<typename T>
using Signal1 = Signal<T>;

/**
 * @brief Signal with two parameters
 */
template<typename T1, typename T2>
using Signal2 = Signal<T1, T2>;

/**
 * @brief Common signal types for UI events
 */
namespace Signals {
    using Clicked = SimpleSignal;                           ///< Button clicked
    using ValueChanged = Signal1<float>;                    ///< Value changed (float)
    using IntValueChanged = Signal1<int>;                   ///< Value changed (int)
    using PositionChanged = Signal2<int16_t, int16_t>;     ///< Position changed
    using SizeChanged = Signal2<uint16_t, uint16_t>;       ///< Size changed
    using StateChanged = Signal1<bool>;                     ///< Boolean state changed
}

/**
 * @brief Auto-disconnecting signal connection
 * 
 * RAII wrapper that automatically disconnects when destroyed.
 */
template<typename... Args>
class SignalConnection {
private:
    Signal<Args...>* signal;
    int connectionId;
    
public:
    /**
     * @brief Constructor
     * @param sig Signal to connect to
     * @param callback Callback function
     */
    SignalConnection(Signal<Args...>* sig, std::function<void(Args...)> callback)
        : signal(sig), connectionId(-1) {
        if (signal) {
            connectionId = signal->connect(callback);
        }
    }
    
    /**
     * @brief Move constructor
     * @param other Connection to move from
     */
    SignalConnection(SignalConnection&& other) noexcept
        : signal(other.signal), connectionId(other.connectionId) {
        other.signal = nullptr;
        other.connectionId = -1;
    }
    
    /**
     * @brief Move assignment operator
     * @param other Connection to move from
     * @return Reference to this connection
     */
    SignalConnection& operator=(SignalConnection&& other) noexcept {
        if (this != &other) {
            disconnect();
            signal = other.signal;
            connectionId = other.connectionId;
            other.signal = nullptr;
            other.connectionId = -1;
        }
        return *this;
    }
    
    /**
     * @brief Destructor - automatically disconnects
     */
    ~SignalConnection() {
        disconnect();
    }
    
    /**
     * @brief Manually disconnect
     */
    void disconnect() {
        if (signal && connectionId >= 0) {
            signal->disconnect(connectionId);
            signal = nullptr;
            connectionId = -1;
        }
    }
    
    /**
     * @brief Check if connection is valid
     * @return True if connected
     */
    bool isConnected() const {
        return signal != nullptr && connectionId >= 0;
    }
    
    // Disable copy
    SignalConnection(const SignalConnection&) = delete;
    SignalConnection& operator=(const SignalConnection&) = delete;
};

} // namespace enjin2