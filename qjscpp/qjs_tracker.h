#pragma once
#include <cstdint>
#include <cstddef>
#include <functional>
#include <type_traits>
#include <map>
#include <mutex>
#include <unordered_map>
#include "qjs_global.h"
#include "qjs_traits.h"
#include "qjs_cast.h"
#include "qjs_trackable.h"

NAMESPACE_QJS_BEGIN

class QcTrackable;

// is_trackable
template <typename T>
struct is_trackable {
    static constexpr bool value = std::is_base_of<QcTrackable, pure_type_t<T>>::value;
};

template <typename T>
static constexpr bool is_trackable_v = is_trackable<T>::value;

// static_cast
template <typename T>
static inline typename std::enable_if<
    is_trackable_v<T> && !is_virtual_base_of_v<QcTrackable, pure_type_t<T>>, QcTrackable*
>::type
ToTrackablePtr(T* ptr) noexcept {
    return static_cast<QcTrackable*>(ptr);
}

// dynamic_cast
template <typename T>
static inline typename std::enable_if<
    is_trackable_v<T>&& is_virtual_base_of_v<QcTrackable, pure_type_t<T>>, QcTrackable*
>::type
ToTrackablePtr(T* ptr) noexcept {
    return dynamic_cast<QcTrackable*>(ptr);
}

// nullptr
template <typename T>
static inline typename std::enable_if<!is_trackable_v<T>, QcTrackable*>::type
ToTrackablePtr(T* /*ptr*/) noexcept {
    return nullptr;
}

// ==========================================
// QcTracker
// A control block used for managing the lifecycle of native C++ 
// objects between C++ and JS.
// 
// Please NOTE that pointer conversion in GetObjectAs is not supported 
// for objects of classes that involve virtual inheritance but do not inherit 
// from `QcTrackable`.
// ==========================================
class QcTracker : public QcIObjectTracker {
public:
    template <typename T>
    explicit QcTracker(T* ptr, bool ownedByJS = true)
        : m_rawPtr(static_cast<void*>(ptr))
        , m_canonicalAddr(get_canonical_addr(ptr))
        , m_rawClassID(GetCastClassID<T>())
        , m_trackable(is_trackable_v<T>)
        , m_trackablePtr(ToTrackablePtr(ptr))
        , m_isAlive(ptr != nullptr)
        , m_isOwnedByJS(ownedByJS)
        , m_refCount(1) {

        m_deleter = [](void* rawPtr) {
            if (rawPtr != nullptr) {
                delete static_cast<T*>(rawPtr);
            }
        };

#ifdef QJS_DEBUG
        m_rawClassName = type_name<T>();
#endif // QJS_DEBUG

    }

    void AddRef() noexcept {
        ++m_refCount;
    }

	// Decrement the reference count.
	// Internally, the system automatically uses `m_isOwnedByJS` to determine 
    // whether to `delete` the native C++ object when the count reaches zero.
    void Release() noexcept {
        if (--m_refCount == 0) {
            if (m_isAlive && m_isOwnedByJS && m_rawPtr) {
                if (m_deleter) {
                    m_deleter(m_rawPtr);
                }
                m_rawPtr = nullptr;
                m_trackablePtr = nullptr;
                m_isAlive = false;
            }

            delete this; // Destroy self
        }
    }

    template <typename T>
    T* GetObjectAs() const noexcept {
#ifdef QJS_DEBUG
        auto debugTargetClassName = type_name<T>();
#endif // QJS_DEBUG
        if (!m_isAlive || !m_rawPtr) {
            return nullptr;
        }

        // If object is trackable (base on QcTrackable)
        // Use dynamic_cast to support classes using virtual inheritance
        if (m_trackable && m_trackablePtr) {
            return dynamic_cast<T*>(m_trackablePtr);
        }

        // Or use ObjectCast to cast raw ptr to target ptr
        // ObjectCast requires that an inheritance relationship between the target class and 
        // the original class be registered with QcDynamicCast (this registration occurs 
        // automatically when inheritance is declared for a JS class). ObjectCast does not 
        // support classes using virtual inheritance
        if (m_rawClassID == GetCastClassID<T>()) {
            return static_cast<T*>(m_rawPtr);
        }
        return ObjectCast<T*>(m_rawPtr, m_rawClassID);
    }

    template <typename T>
    T* TakeObject() noexcept {
        if (!m_isAlive || !m_rawPtr) {
            return nullptr;
        }

        T* typedPtr = GetObjectAs<T>();
        m_rawPtr = nullptr;
        m_trackablePtr = nullptr;
        m_isAlive = false;

        return typedPtr;
    }

    void MarkObjectDestroyed() noexcept {
        m_rawPtr = nullptr;
        m_trackablePtr = nullptr;
        m_isAlive = false;
    }

    bool IsAlive() const noexcept {
        return m_isAlive && (m_rawPtr != nullptr);
    }

    bool IsOwnedByJS() const noexcept {
        return m_isOwnedByJS;
    }

    void SetDestroyHandler(std::function<void(QcTracker*, void*)> handler) {
        m_destroyHandler = std::move(handler);
    }

private:
    ~QcTracker() {
        QJS_ASSERT_M(!m_isOwnedByJS || m_rawPtr == nullptr, "Object leaked detected which is owned by JS");
        if (m_destroyHandler) {
            m_destroyHandler(this, m_canonicalAddr);
        }
    }

private: // QcIObjectTracker
    void TrackerOnObjectDestroyed() noexcept override {
        MarkObjectDestroyed();
    };

    void TrackerAddRef() noexcept override {
        AddRef();
    };

    void TrackerRelease() noexcept override {
        Release();
    }

private:
    void* m_rawPtr{ nullptr };
    void* m_canonicalAddr;
    QcCastClassID m_rawClassID{ 0 };
    
    bool m_trackable{ false };
    QcTrackable* m_trackablePtr {nullptr};

    bool m_isAlive{ false };
    bool m_isOwnedByJS{ true }; // Mark whether the object is owned by the JS
    int m_refCount{ 1 };

    std::function<void(void*)> m_deleter;
    std::function<void(QcTracker*, void*)> m_destroyHandler;

#ifdef QJS_DEBUG
    std::string m_rawClassName;
#endif
};

// =============================
// QcTrackerMgr: requires is_polymorphic
// =============================
class QcTrackerMgr {
    struct DummyMutex {
        void lock() {};
        void unlock() {};
    };

    using MutexType = DummyMutex; // std::mutex

public:
    static constexpr size_t BUCKET_COUNT = 1009; // 1009 / 4099 / 8191

    struct TrackerBucket {
        std::unordered_map<void*, QcTracker*> trackerMap;
        mutable MutexType lock; // Each bucket has its own lock.
    };

    using TrackerBucketArray = TrackerBucket[BUCKET_COUNT];
    static TrackerBucketArray& GetBuckets() {
        static TrackerBucket s_buckets[BUCKET_COUNT];
        return s_buckets;
    }

    static TrackerBucket& GetBucket(void* key) {
        auto& buckets = GetBuckets();
        size_t hashValue = std::hash<void*>{}(key);
        return buckets[hashValue % BUCKET_COUNT];
    }

public:
    // Get tracker
    template <typename T>
    static QcTracker* Get(T* ptr) {
        QJS_STATIC_ASSERT_M(std::is_polymorphic<T>::value, 
            "Tracker requires managed classes to be polymorphic");
        if (!ptr) {
            return nullptr;
        }

        void* key = get_canonical_addr(ptr);
        auto& bucket = GetBucket(key);

        std::lock_guard<MutexType> guard(bucket.lock);
        auto it = bucket.trackerMap.find(key);
        if (it != bucket.trackerMap.end()) {
            QJS_ASSERT_M(!it->second->IsOwnedByJS(), "Tracker must not be owned by JS");
            it->second->AddRef();
            return it->second;
        }
        return nullptr;
    }

    // Create tracker
    template <typename T>
    static QcTracker* Create(T* ptr) {
        QJS_STATIC_ASSERT_M(std::is_polymorphic<T>::value,
            "Tracker requires managed classes to be polymorphic");
        if (!ptr) {
            return nullptr;
        }

        void* key = get_canonical_addr(ptr);
        auto& bucket = GetBucket(key);

        // Check if exists
        {
            std::lock_guard<MutexType> guard(bucket.lock);
            auto it = bucket.trackerMap.find(key);
            if (it != bucket.trackerMap.end()) {
                it->second->AddRef();
                return it->second;
            }
        }

        // Create new one
        auto* tracker = new QcTracker(ptr, false);
        tracker->SetDestroyHandler([](QcTracker* trackerPtr, void* canonicalAddr) {
            auto& b = GetBucket(canonicalAddr);
            std::lock_guard<MutexType> g(b.lock);
            b.trackerMap.erase(canonicalAddr);
        });
        {
            std::lock_guard<MutexType> guard(bucket.lock);
            bucket.trackerMap[key] = tracker;
        }

        return tracker;
    }

    // Used when explicitly releasing objects in C++
    template <typename T>
    static void MarkObjectDestroyed(T* ptr) {
        QJS_STATIC_ASSERT_M(std::is_polymorphic<T>::value,
            "Tracker requires managed classes to be polymorphic");
        if (!ptr) {
            return;
        }

        void* key = get_canonical_addr(ptr);
        auto& bucket = GetBucket(key);

        std::lock_guard<MutexType> guard(bucket.lock);
        auto it = bucket.trackerMap.find(key);
        if (it != bucket.trackerMap.end()) {
            QJS_ASSERT_M(!it->second->IsOwnedByJS(), "Tracker must not be owned by JS");
            it->second->MarkObjectDestroyed();
        }
    }
};

// =========================================
// QcTrackerResolver:  QcTrackerMgr and QcTrackable Wrapper
// =========================================
class QcTrackerResolver {
public:
    // Get tracker for QcTrackable
    template <typename T>
    static typename std::enable_if<is_trackable_v<T>, QcTracker*>::type
        Get(T* ptr) noexcept {
        if (ptr == nullptr) {
            return nullptr;
        }

        auto* objectTracker = ptr->qc_GetTracker();
        if (objectTracker == nullptr) {
            return nullptr;
        }

        auto* tracker = static_cast<QcTracker*>(objectTracker);
        QJS_ASSERT_M(!tracker->IsOwnedByJS(), "Tracker must not be owned by JS");
        tracker->AddRef(); // RefCount + 1
        return tracker;
    }

    // Get tracker for Non-QcTrackable
    template <typename T>
    static typename std::enable_if<!is_trackable_v<T>, QcTracker*>::type
        Get(T* ptr) noexcept {
        if (ptr == nullptr) {
            return nullptr;
        }
        return QcTrackerMgr::Get(ptr); // RefCount + 1
    }

    // Create tracker for QcTrackable
    // If tracker exists, return the existing one.
    // Otherwise, create a new one and return it.
    template <typename T>
    static typename std::enable_if<is_trackable_v<T>, QcTracker*>::type
        Create(T* ptr)  noexcept {
        if (ptr == nullptr) {
            return nullptr;
        }

        auto* objectTracker = ptr->qc_GetTracker();

        if (objectTracker) {
            auto* tracker = static_cast<QcTracker*>(objectTracker);
            tracker->AddRef(); // RefCount + 1
            return tracker;
        } else {
            auto* tracker = new QcTracker(ptr, false); // RefCount = 1
            ptr->qc_SetTracker(tracker);
            return tracker;
        }
    }

    // Create tracker for Non-QcTrackable
    template <typename T>
    static typename std::enable_if<!is_trackable_v<T>, QcTracker*>::type 
        Create(T* ptr)  noexcept {
        if (ptr == nullptr) {
            return nullptr;
        }
        return QcTrackerMgr::Create(ptr);
    }

    // MarkObjectDestroyed for QcTrackable
    template <typename T>
    static typename std::enable_if<is_trackable_v<T>, void>::type
        MarkObjectDestroyed(T* ptr) {
        if (ptr == nullptr) {
            return;
        }

        auto* objectTracker = ptr->qc_GetTracker();

        if (objectTracker) {
            auto* tracker = static_cast<QcTracker*>(objectTracker);
            QJS_ASSERT_M(!tracker->IsOwnedByJS(), "Tracker must not be owned by JS");
            tracker->MarkObjectDestroyed();
        }
    }

    // MarkObjectDestroyed for Non-QcTrackable
    template <typename T>
    static typename std::enable_if<!is_trackable_v<T>, void>::type
        MarkObjectDestroyed(T* ptr) {
        if (ptr == nullptr) {
            return;
        }
        return QcTrackerMgr::MarkObjectDestroyed(ptr);
    }
};

NAMESPACE_QJS_END