#pragma once

#include "object_collection.hpp"
#include "signal.hpp"
#include "../graphics/canvas.hpp"
#include "../components/drawable.hpp"
#include <algorithm>
#include <iostream>
#include <type_traits>

namespace enjin2 {

class SceneStateMachine;  // forward declaration — prevents circular include

/**
 * @brief Base class for game scenes
 * 
 * Manages a collection of objects and provides lifecycle methods
 * for scene initialization, updating, and cleanup.
 */
class Scene {
protected:
    ObjectCollection objects;       ///< Objects in this scene
    bool initialized;               ///< Whether scene has been initialized
    bool active;                    ///< Whether scene is currently active
    uint32_t sceneId;              ///< Unique scene identifier
    
    // Scene lifecycle signals
    Signal<Scene*> onCreateSignal;      ///< Emitted when scene is created
    Signal<Scene*> onActivateSignal;    ///< Emitted when scene becomes active
    Signal<Scene*> onDeactivateSignal;  ///< Emitted when scene becomes inactive
    Signal<Scene*> onDestroySignal;     ///< Emitted when scene is destroyed

    SceneStateMachine* m_ssm = nullptr;  ///< Non-owning back-pointer to owning SSM. Injected at activation.

public:
    /**
     * @brief Constructor
     * @param id Unique scene identifier
     */
    explicit Scene(uint32_t id) 
        : initialized(false), active(false), sceneId(id) {}
    
    /**
     * @brief Virtual destructor
     */
    virtual ~Scene() {
        if (active) {
            deactivate();
        }
        onDestroy();
    }
    
    /**
     * @brief Initialize the scene
     * 
     * Called once when the scene is first created.
     * Override to set up initial objects and state.
     */
    void initialize() {
        if (initialized) return;
        
        onCreate();
        objects.initialize();
        initialized = true;
        
        onCreateSignal.emit(this);
    }
    
    /**
     * @brief Activate the scene
     * 
     * Called when the scene becomes the active scene.
     */
    void activate() {
        if (active) return;
        
        if (!initialized) {
            initialize();
        }
        
        onActivate();
        objects.start();
        active = true;
        
        onActivateSignal.emit(this);
    }
    
    /**
     * @brief Deactivate the scene
     * 
     * Called when the scene is no longer active.
     */
    void deactivate() {
        if (!active) return;
        
        onDeactivate();
        active = false;
        
        onDeactivateSignal.emit(this);
    }
    
    /**
     * @brief Update the scene
     * @param dt Time since last frame in seconds
     */
    void update(float dt) {
        if (!active) return;

        onUpdate(dt);
        objects.update(dt);
        objects.lateUpdate(dt);
    }
    
    /**
     * @brief Render the scene
     * @param canvas Target canvas for rendering
     */
    template<typename PixelType>
    void render(ICanvas<PixelType>& canvas) {
        if (!active) return;
        
        if constexpr (std::is_same_v<PixelType, uint8_t>) {
            onRender(canvas);
        } else {
            // For non-uint8_t canvases, skip scene-specific rendering for now
            // In a full implementation, you'd convert or provide templated onRender
        }
        renderObjects(canvas);
    }
    
    /**
     * @brief Add an object to the scene
     * @tparam T Object type (must derive from Object)
     * @tparam Args Constructor argument types
     * @param args Constructor arguments
     * @return Pointer to created object or nullptr if failed
     */
    template<typename T, typename... Args>
    T* addObject(Args&&... args) {
        return objects.addObject<T>(std::forward<Args>(args)...);
    }
    
    /**
     * @brief Remove an object from the scene
     * @param object Object to remove
     * @return True if object was removed
     */
    bool removeObject(Object* object) {
        return objects.removeObject(object);
    }
    
    /**
     * @brief Find first object of specified type
     * @tparam T Object type
     * @return Pointer to object or nullptr if not found
     */
    template<typename T>
    T* findObject() {
        return objects.findObject<T>();
    }
    
    /**
     * @brief Find object with component of specified type
     * @tparam T Component type
     * @return Pointer to object or nullptr if not found
     */
    template<typename T>
    Object* findObjectWithComponent() {
        return objects.findObjectWithComponent<T>();
    }

    /**
     * @brief Find first object with the given name
     * @param name Name to search for (string literal, case-sensitive)
     * @return Pointer to matching Object or nullptr if not found
     */
    Object* findByName(const char* name) {
        return objects.findByName(name);
    }

    /**
     * @brief Find all objects carrying the given tag
     * @param tag Tag to search for (string literal, case-sensitive)
     * @param results Caller-provided array to write matching Object pointers into
     * @param maxResults Maximum number of results to write
     * @return Number of objects written into results
     */
    size_t findAllWithTag(const char* tag, Object** results, size_t maxResults) {
        return objects.findAllWithTag(tag, results, maxResults);
    }

    /**
     * @brief Get scene ID
     * @return Scene identifier
     */
    uint32_t getId() const {
        return sceneId;
    }
    
    /**
     * @brief Check if scene is active
     * @return True if scene is active
     */
    bool isActive() const {
        return active;
    }
    
    /**
     * @brief Check if scene is initialized
     * @return True if scene is initialized
     */
    bool isInitialized() const {
        return initialized;
    }

    /**
     * @brief Inject non-owning SSM back-pointer
     * @param ssm Owning SceneStateMachine (called before activate())
     */
    void setStateMachine(SceneStateMachine* ssm) { m_ssm = ssm; }

    /**
     * @brief Reset initialized guard to allow re-initialization (used for self-transitions)
     */
    void resetInitialized() { initialized = false; }

    /**
     * @brief Get object collection
     * @return Reference to object collection
     */
    ObjectCollection& getObjects() {
        return objects;
    }
    
    /**
     * @brief Get object collection (const)
     * @return Const reference to object collection
     */
    const ObjectCollection& getObjects() const {
        return objects;
    }
    
    /**
     * @brief Connect to scene create event
     * @param callback Function called when scene is created
     * @return Signal connection handle
     */
    template<typename... Args>
    SignalConnection<Scene*> connectOnCreate(std::function<void(Scene*)> callback) {
        return SignalConnection<Scene*>(&onCreateSignal, callback);
    }

    /**
     * @brief Connect to scene activate event
     * @param callback Function called when scene becomes active
     * @return Signal connection handle
     */
    template<typename... Args>
    SignalConnection<Scene*> connectOnActivate(std::function<void(Scene*)> callback) {
        return SignalConnection<Scene*>(&onActivateSignal, callback);
    }

    /**
     * @brief Connect to scene deactivate event
     * @param callback Function called when scene becomes inactive
     * @return Signal connection handle
     */
    template<typename... Args>
    SignalConnection<Scene*> connectOnDeactivate(std::function<void(Scene*)> callback) {
        return SignalConnection<Scene*>(&onDeactivateSignal, callback);
    }

    /**
     * @brief Connect to scene destroy event
     * @param callback Function called when scene is destroyed
     * @return Signal connection handle
     */
    template<typename... Args>
    SignalConnection<Scene*> connectOnDestroy(std::function<void(Scene*)> callback) {
        return SignalConnection<Scene*>(&onDestroySignal, callback);
    }

protected:
    /**
     * @brief Called when scene is created (override in derived classes)
     * 
     * Use this to initialize scene-specific data and create initial objects.
     */
    virtual void onCreate() {}
    
    /**
     * @brief Called when scene becomes active (override in derived classes)
     * 
     * Use this to resume animations, start background processes, etc.
     */
    virtual void onActivate() {}
    
    /**
     * @brief Called when scene becomes inactive (override in derived classes)
     * 
     * Use this to pause animations, stop background processes, etc.
     */
    virtual void onDeactivate() {}
    
    /**
     * @brief Called when scene is destroyed (override in derived classes)
     * 
     * Use this to clean up scene-specific resources.
     */
    virtual void onDestroy() {}
    
    /**
     * @brief Called every frame (override in derived classes)
     * @param dt Time since last frame in seconds
     *
     * Use this for scene-specific update logic that should happen
     * before object updates.
     */
    virtual void onUpdate(float dt) {}
    
    /**
     * @brief Called during rendering for 4-bit canvas (override in derived classes)
     * @param canvas Target canvas for rendering
     * 
     * Use this for scene-specific rendering like backgrounds or UI overlays.
     */
    virtual void onRender(ICanvas<Pixel4>& canvas) {}
    
    /**
     * @brief Called during rendering for 8-bit canvas (override in derived classes)
     * @param canvas Target canvas for rendering
     * 
     * Use this for scene-specific rendering like backgrounds or UI overlays.
     */
    virtual void onRender(ICanvas<uint8_t>& canvas) {}
    
private:
    /**
     * @brief Render all objects in the scene
     * @param canvas Target canvas for rendering
     */
    template<typename PixelType>
    void renderObjects(ICanvas<PixelType>& canvas) {
        // Collect all drawable components
        static constexpr size_t MAX_DRAWABLES = 256;
        C_Drawable* drawables[MAX_DRAWABLES];
        size_t drawableCount = 0;
        
        objects.forEach([&](Object* obj) {
            if (!obj || !obj->isActive()) return;
            
            // Get all drawable components from this object
            for (size_t i = 0; i < obj->getDrawableCount(); ++i) {
                auto drawable = obj->getDrawable(i);
                if (drawable && drawable->isVisible() && drawableCount < MAX_DRAWABLES) {
                    drawables[drawableCount++] = drawable;
                }
            }
        });
        
        // Sort drawables by layer and sort order
        std::sort(drawables, drawables + drawableCount, 
                  [](const C_Drawable* a, const C_Drawable* b) {
                      return a->shouldDrawBefore(*b);
                  });
        
        
        // Render all sorted drawables
        for (size_t i = 0; i < drawableCount; ++i) {
            if constexpr (std::is_same_v<PixelType, Pixel4>) {
                // Direct drawing for Pixel4 canvas (primary path)
                drawables[i]->draw(canvas);
            } else {
                // Drawable::draw() requires ICanvas<Pixel4>. Canvas8 (uint8_t) compositing
                // will be handled by ENG-01 (Phase 25 compositor). For now, non-Pixel4
                // canvases do not render drawables.
                (void)i;  // suppress unused warning
            }
        }
    }
};

} // namespace enjin2