// ===================================================
// QcDynamicCastFast / QcDynamicCast
// Fast, RTTI-free bidirectional dynamic casting engine (Upcast & Downcast).
// Supports complex, multiple, diamond, and deep inheritance hierarchies
// while safely preventing invalid sidecasts.
//
// NOTE: Virtual inheritance is NOT supported.
//
// Mechanics:
// 1. Registration: Computes physical byte offsets and uses Floyd-Warshall algorithm
//    to generate all transitive paths in a lookup matrix:
//    `matrix[SourceClassID][TargetClassID] = EncodedByteOffset`.
// 2. Runtime Cast: Performs O(1) matrix lookup and pointer arithmetic (`rawPtr + offset`).
//
// Thread-Safety:
// 1. Class registrations (RegisterCastClass / registerClass) MUST be performed 
//     during the single-threaded initialization phase. 
// 2. Runtime casting (ObjectCast / castTo) is fully thread-safe for concurrent reads.
// 
// QcDynamicCastFast vs. QcDynamicCast:
// - QcDynamicCastFast: Compile-time fixed 2D static array. Zero heap allocations,
//   optimal cache locality, and fastest cast (~3.8 ns). Capped by QC_CAST_MAX_CLASS_COUNT.
// - QcDynamicCast: Dynamic 1D `std::vector` matrix. Grows automatically to support
//   unlimited classes, with slightly higher cast overhead (~5.5 ns).
// ===================================================

#pragma once
#include <algorithm>
#include <utility>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>
#include "qjs_global.h"


// ==========================
// Configures
// ==========================

// kQcCastMaxClassCount
// Maximum number of classes: QcDynamicCastFast ONLY
#ifdef QC_CAST_MAX_CLASS_COUNT
static constexpr size_t kQcCastMaxClassCount = QC_CAST_MAX_CLASS_COUNT;
#else
static constexpr size_t kQcCastMaxClassCount = 500;
#endif


// kQcCastInitClassCount
// Number of classes at initialization: QcDynamicCast ONLY
#ifdef QC_CAST_INIT_CLASS_COUNT
static constexpr size_t kQcCastInitClassCount = QC_CAST_INIT_CLASS_COUNT;
#else
static constexpr size_t kQcCastInitClassCount = 500;
#endif


// ==========================
// DynamicCast
// ==========================
NAMESPACE_QJS_BEGIN

// QcCastClassID
using QcCastClassID = uint32_t;

namespace cast {

// =========================
// Utility
// =========================

template <typename... Ts>
using void_ty = void;

// is_virtual_base_of
template <typename Base, typename Derived, typename = void>
struct can_static_downcast : std::false_type {};

template <typename Base, typename Derived>
struct can_static_downcast<Base, Derived,
    void_ty<decltype(static_cast<Derived*>(std::declval<Base*>()))>
> : std::true_type {
};

template <typename Base, typename Derived>
struct is_virtual_base_of : std::integral_constant<bool, 
    std::is_base_of<Base, Derived>::value && !can_static_downcast<Base, Derived>::value > {
};

template <typename Base, typename Derived>
static constexpr bool is_virtual_base_of_v = is_virtual_base_of<Base, Derived>::value;

// GetStaticClassOffset
template <typename ParentClass, typename DerivedClass>
inline std::ptrdiff_t GetStaticClassOffset() noexcept {
    QJS_STATIC_ASSERT_M((!is_virtual_base_of_v<ParentClass, DerivedClass>),
        "GetStaticClassOffset does not support virtual inheritance!");
    QJS_STATIC_ASSERT_M((std::is_base_of<ParentClass, DerivedClass>::value),
        "ParentClass must be base of DerivedClass!");

    const auto* d = reinterpret_cast<const DerivedClass*>(0x1000);
    const auto* p = static_cast<const ParentClass*>(d);
    return reinterpret_cast<const char*>(p) - reinterpret_cast<const char*>(d);
}


constexpr std::ptrdiff_t NO_OFFSET = 0x7FFFFFFF; // unreachable (no relationship)
constexpr std::ptrdiff_t ZERO_OFFSET = (std::numeric_limits<std::ptrdiff_t>::max)() - 1; // upcast path with a physical offset of 0

// OffsetRawToPhysical
inline std::ptrdiff_t OffsetRawToPhysical(std::ptrdiff_t rawOffset) noexcept {
    if (std::abs(rawOffset) == ZERO_OFFSET) {
        return 0;
    }
    return rawOffset;
}

// OffsetPhysicalToRaw
inline std::ptrdiff_t OffsetPhysicalToRaw(std::ptrdiff_t physicalOffset) noexcept {
    return (physicalOffset == 0) ? ZERO_OFFSET : physicalOffset;
}

inline bool IsUpcastPath(std::ptrdiff_t rawOffset) noexcept {
    return rawOffset != NO_OFFSET && rawOffset >= 0;
}


// =========================
// QcCastClassIDRegistry
// =========================
class QcCastClassIDRegistry {
    template <typename T>
    struct GetPureType {
        using type = typename std::remove_cv<
            typename std::remove_pointer<
            typename std::remove_reference<T>::type
            >::type
        >::type;
    };

public:
    static QcCastClassID GetMaxID() noexcept {
        return _ClassIDCount().load(std::memory_order_relaxed);
    }

    template <typename T>
    static QcCastClassID GetClassID() noexcept {
        using PureType = typename GetPureType<T>::type;
        return _GetClassID<PureType>();
    }

private:
    static std::atomic<QcCastClassID>& _ClassIDCount() noexcept {
        static std::atomic<QcCastClassID> s_counter{ 1 };
        return s_counter;
    }

    static QcCastClassID _AllocateClassID() noexcept {
        return _ClassIDCount().fetch_add(1, std::memory_order_relaxed);
    }

    template <typename T>
    static QcCastClassID _GetClassID() noexcept {
        static const QcCastClassID id = _AllocateClassID();
        return id;
    }
};

// =======================================
// QcDynamicCastFast (Compile-time Array Matrix)
// =======================================

class QcDynamicCastFast {
    using TableType = std::ptrdiff_t[kQcCastMaxClassCount][kQcCastMaxClassCount];

public:
    // Registers an inheritance relationship: DerivedClass -> ParentClass.
    // Virtual inheritance is prohibited.
    template <typename ParentClass, typename DerivedClass>
    static void registerClass() {
        QJS_STATIC_ASSERT_M((std::is_base_of<ParentClass, DerivedClass>::value),
            "ParentClass must be base of DerivedClass");
        QJS_STATIC_ASSERT_M((!is_virtual_base_of_v<ParentClass, DerivedClass>),
            "Registration of classes with virtual inheritance is prohibited");
        if (std::is_same<ParentClass, DerivedClass>::value) {
            QJS_ASSERT(false, "Cannot register same class to itself");
            return;
        }

        const QcCastClassID derivedID = QcCastClassIDRegistry::GetClassID<DerivedClass>();
        const QcCastClassID parentID = QcCastClassIDRegistry::GetClassID<ParentClass>();
        if (derivedID >= kQcCastMaxClassCount || parentID >= kQcCastMaxClassCount) {
            QJS_ASSERT(false, "QcCastClassID exceeded the maximum class limit");
            return;
        }

        // Track the highest registered ClassID to optimize loop bounds
        const QcCastClassID maxNeededID = (std::max)(derivedID, parentID);
        _getMaxRegisteredID() = (std::max)(_getMaxRegisteredID(), maxNeededID);

        auto& table = _getStaticTable();
        if (table[derivedID][parentID] != NO_OFFSET) {
            QJS_ASSERT(false, "Cannot register class multiple times.");
            return;
        }

        // Calculate static byte offset between DerivedClass and ParentClass
        const std::ptrdiff_t physicalStepOffset = GetStaticClassOffset<ParentClass, DerivedClass>();

        // Propagate transitive inheritance paths across the matrix
        _updateClassMatrix(derivedID, parentID, physicalStepOffset);
    }

    // Ultra-fast O(1) Runtime cast operation (~3.8 ns). Returns nullptr if cast is invalid.
    template <typename T>
    static T castTo(void* rawPtr, QcCastClassID rawID) noexcept {
        QJS_STATIC_ASSERT_M(std::is_pointer<T>::value, "Target type must be a pointer");
        if (!rawPtr) {
            return nullptr;
        }

        const QcCastClassID targetID = QcCastClassIDRegistry::GetClassID<T>();
        if (rawID >= kQcCastMaxClassCount || targetID >= kQcCastMaxClassCount) {
            return nullptr; // Exceeded maximum class capacity
        }

        const std::ptrdiff_t rawOffset = _getStaticTable()[rawID][targetID];
        if (rawOffset == NO_OFFSET) {
            return nullptr; // No inheritance relationship
        }

        const std::ptrdiff_t physicalOffset = OffsetRawToPhysical(rawOffset);
        return reinterpret_cast<T>(reinterpret_cast<char*>(rawPtr) + physicalOffset);
    }

    // Collects physical offsets of all base classes accessible from `derivedID`.
    static std::vector<std::ptrdiff_t> collectAllBaseUpcastOffsets(QcCastClassID derivedID) {
        std::vector<std::ptrdiff_t> offsets;
        if (derivedID == 0 || derivedID >= kQcCastMaxClassCount) {
            return offsets;
        }

        // Limit traversal bound to high watermark rather than kQcCastMaxClassCount (500)
        const size_t maxLimit = (std::min)(kQcCastMaxClassCount, static_cast<size_t>(_getMaxRegisteredID() + 1));
        if (derivedID >= maxLimit) {
            return offsets;
        }

        auto& table = _getStaticTable();

        for (size_t targetBaseID = 1; targetBaseID < maxLimit; ++targetBaseID) {
            if (targetBaseID == derivedID) {
                continue;
            }

            std::ptrdiff_t rawOffset = table[derivedID][targetBaseID];
            if (IsUpcastPath(rawOffset)) {
                offsets.push_back(OffsetRawToPhysical(rawOffset));
            }
        }

        return offsets;
    }

private:
    // Obtain the static 2D matrix [DerivedClassID][TargetParentClassID]
    static TableType& _getStaticTable() {
        static TableType s_table;
        static bool s_inited = false;

        if (!s_inited) {
            std::fill(&s_table[0][0], &s_table[0][0] + kQcCastMaxClassCount * kQcCastMaxClassCount, NO_OFFSET);
            for (size_t i = 0; i < kQcCastMaxClassCount; ++i) {
                s_table[i][i] = 0; // Self-to-self cast offset is zero
            }
            s_inited = true;
        }

        return s_table;
    }

    // High Watermark: Stores the maximum registered ClassID
    static QcCastClassID& _getMaxRegisteredID() {
        static QcCastClassID s_maxRegisteredID = 0;
        return s_maxRegisteredID;
    }

    // Updates transitive closure paths for newly registered inheritance pairs
    // Assuming B is reachable from A, once C becomes reachable from B, 
    // the offset for C->A must be updated to supporting casting C to A,
    // and the offset value stored in table is: table[A][C]
    static void _updateClassMatrix(QcCastClassID derivedID, QcCastClassID parentID, std::ptrdiff_t physicalStepOffset) {
        const size_t maxLimit = _getMaxRegisteredID() + 1;
        auto& table = _getStaticTable();

        const std::ptrdiff_t rawStepOffset = OffsetPhysicalToRaw(physicalStepOffset);

        // Set Offset for Parent-Derived
        table[derivedID][parentID] = rawStepOffset;   // Upcast (Derived -> Parent)
        table[parentID][derivedID] = -rawStepOffset;  // Downcast (Parent -> Derived)

        // Connect all sub-classes of derivedID to all ancestors of parentID
        for (size_t subClassID = 0; subClassID < maxLimit; ++subClassID) {
            const std::ptrdiff_t subToDerivedRawOffset = table[subClassID][derivedID];

            // subClassID must be a genuine positive Upcast descendant of derivedID.
            // Intercepts illegal Sidecast synthesizing paths (e.g., B1 -> C1 -> B2).
            if (!IsUpcastPath(subToDerivedRawOffset)) {
                continue;
            }

            const std::ptrdiff_t subToDerivedPhysOffset = OffsetRawToPhysical(subToDerivedRawOffset);

            // Find all ancestorIDs reachable from parentID 
            // Including parentID itself, since table[parentID][parentID] == 0
            for (size_t ancestorID = 0; ancestorID < maxLimit; ++ancestorID) {
                const std::ptrdiff_t parentToAncestorRawOffset = table[parentID][ancestorID];
                if (!IsUpcastPath(parentToAncestorRawOffset)) {
                    continue;
                }

                // Calculate actual physical byte offset
               // New path: i -> (derived) -> parent -> ancestor 
               // New path length = (i -> derived) + (derived -> parent) + (parent -> ancestor)
                const std::ptrdiff_t parentToAncestorPhysOffset = OffsetRawToPhysical(parentToAncestorRawOffset);
                const std::ptrdiff_t totalUpcastPhysOffset = subToDerivedPhysOffset + physicalStepOffset + parentToAncestorPhysOffset;
                const std::ptrdiff_t totalUpcastRawOffset = OffsetPhysicalToRaw(totalUpcastPhysOffset);

                // Update transitive paths in table
                table[subClassID][ancestorID] = totalUpcastRawOffset;   // Upcast
                table[ancestorID][subClassID] = -totalUpcastRawOffset;  // Downcast
            }
        }
    }
};

// ====================================
// QcDynamicCast (Flat Array Matrix Implementation)
// ====================================

class QcDynamicCast {
public:
    // Pre-allocates matrix capacity for expected number of classes.
    static void reserve(size_t expectedClassCount) {
        _ensureCapacity(expectedClassCount > 0 ? expectedClassCount - 1 : 0);
    }

    // Registers an inheritance relationship: DerivedClass -> ParentClass.
    // Virtual inheritance is prohibited.
    template <typename ParentClass, typename DerivedClass>
    static void registerClass() {
        QJS_STATIC_ASSERT_M((std::is_base_of<ParentClass, DerivedClass>::value),
            "ParentClass must be base of DerivedClass");
        QJS_STATIC_ASSERT_M((!is_virtual_base_of_v<ParentClass, DerivedClass>),
            "Registration of classes with virtual inheritance is prohibited");
        if (std::is_same<ParentClass, DerivedClass>::value) {
            QJS_ASSERT(false, "Cannot register same class to itself");
            return;
        }

        const QcCastClassID derivedID = QcCastClassIDRegistry::GetClassID<DerivedClass>();
        const QcCastClassID parentID = QcCastClassIDRegistry::GetClassID<ParentClass>();
        const QcCastClassID maxNeededID = (derivedID > parentID) ? derivedID : parentID;

        _ensureCapacity(maxNeededID);

        // Track the highest registered ClassID to optimize loop bounds
        _getMaxRegisteredID() = (std::max)(_getMaxRegisteredID(), maxNeededID);

        auto& flatTable = _getFlatTable();
        const size_t capacity = _getCapacity();
        if (flatTable[derivedID * capacity + parentID] != NO_OFFSET) {
            QJS_ASSERT(false, "Cannot register class multiple times.");
            return;
        }

        // Calculate static byte offset between DerivedClass and ParentClass
        const std::ptrdiff_t physicalStepOffset = GetStaticClassOffset<ParentClass, DerivedClass>();

        // Propagate transitive inheritance paths across the matrix
        _updateClassMatrix(derivedID, parentID, physicalStepOffset);
    }

    // O(1) Runtime cast operation. Returns nullptr if cast is invalid.
    template <typename T>
    static T castTo(void* rawPtr, QcCastClassID rawID) noexcept {
        QJS_STATIC_ASSERT_M(std::is_pointer<T>::value, "Target type must be a pointer");
        if (!rawPtr) {
            return nullptr;
        }

        const QcCastClassID targetID = QcCastClassIDRegistry::GetClassID<T>();
        const size_t capacity = _getCapacity();
        if (rawID >= capacity || targetID >= capacity) {
            return nullptr; // Out of registered bounds
        }

        const std::ptrdiff_t rawOffset = _getFlatTable()[rawID * capacity + targetID];
        if (rawOffset == NO_OFFSET) {
            return nullptr; // No inheritance relationship
        }

        const std::ptrdiff_t physicalOffset = OffsetRawToPhysical(rawOffset);
        return reinterpret_cast<T>(reinterpret_cast<char*>(rawPtr) + physicalOffset);
    }

    // Collects physical offsets of all base classes accessible from `derivedID`.
    static std::vector<std::ptrdiff_t> collectAllBaseUpcastOffsets(QcCastClassID derivedID) {
        std::vector<std::ptrdiff_t> offsets;
        if (derivedID == 0) {
            return offsets;
        }

        const size_t capacity = _getCapacity();
        // Limit traversal bound to the high watermark rather than full capacity
        const size_t maxLimit = (std::min)(capacity, _getMaxRegisteredID() + 1);
        if (derivedID >= maxLimit) {
            return offsets;
        }

        auto& flatTable = _getFlatTable();
        const size_t rowStart = derivedID * capacity;

        for (size_t targetBaseID = 1; targetBaseID < maxLimit; ++targetBaseID) {
            if (targetBaseID == derivedID) {
                continue;
            }

            std::ptrdiff_t rawOffset = flatTable[rowStart + targetBaseID];
            if (IsUpcastPath(rawOffset)) {
                offsets.push_back(OffsetRawToPhysical(rawOffset));
            }
        }

        return offsets;
    }

private:
    static std::vector<std::ptrdiff_t>& _getFlatTable() {
        static std::vector<std::ptrdiff_t> s_flatTable;
        return s_flatTable;
    }

    static size_t& _getCapacity() {
        static size_t s_capacity = 0;
        return s_capacity;
    }

    // High Watermark: Stores the maximum registered ClassID
    static QcCastClassID& _getMaxRegisteredID() {
        static QcCastClassID s_maxRegisteredID = 0;
        return s_maxRegisteredID;
    }

    // Dynamically grows matrix capacity when maxID exceeds current capacity
    static void _ensureCapacity(QcCastClassID maxID) {
        size_t& capacity = _getCapacity();
        if (capacity > static_cast<size_t>(maxID)) {
            return;
        }

        size_t newCapacity = (capacity == 0) ? kQcCastInitClassCount : capacity;
        while (newCapacity <= static_cast<size_t>(maxID)) {
            newCapacity += 500;
        }

        auto& flatTable = _getFlatTable();
        std::vector<std::ptrdiff_t> newTable(newCapacity * newCapacity, NO_OFFSET);

        // Self-to-self cast offset is zero (matrix diagonal)
        for (size_t i = 0; i < newCapacity; ++i) {
            newTable[i * newCapacity + i] = 0;
        }

        // Copy existing relationship offsets to the enlarged matrix
        if (capacity > 0) {
            for (size_t r = 0; r < capacity; ++r) {
                for (size_t c = 0; c < capacity; ++c) {
                    newTable[r * newCapacity + c] = flatTable[r * capacity + c];
                }
            }
        }

        flatTable.swap(newTable);
        capacity = newCapacity;
    }

    // Updates transitive closure paths for newly registered inheritance pairs
    static void _updateClassMatrix(QcCastClassID derivedID, QcCastClassID parentID, std::ptrdiff_t physicalStepOffset) {
        const size_t maxLimit = _getMaxRegisteredID() + 1;
        const size_t capacity = _getCapacity();
        auto& flatTable = _getFlatTable();

        const std::ptrdiff_t rawStepOffset = OffsetPhysicalToRaw(physicalStepOffset);

        // Set Offset for Parent-Derived
        flatTable[derivedID * capacity + parentID] = rawStepOffset;   // Upcast
        flatTable[parentID * capacity + derivedID] = -rawStepOffset;  // Downcast

        const size_t parentRowIndex = parentID * capacity;
        const size_t derivedColIndex = derivedID;

        // Connect all sub-classes of derivedID to all ancestors of parentID
        for (size_t subClassID = 0; subClassID < maxLimit; ++subClassID) {
            const size_t subClassRowIndex = subClassID * capacity;
            const std::ptrdiff_t subToDerivedRawOffset = flatTable[subClassRowIndex + derivedColIndex];
            if (!IsUpcastPath(subToDerivedRawOffset)) {
                continue;
            }

            const std::ptrdiff_t subToDerivedPhysOffset = OffsetRawToPhysical(subToDerivedRawOffset);

            // Find all ancestorIDs reachable from parentID 
            for (size_t ancestorID = 0; ancestorID < maxLimit; ++ancestorID) {
                const std::ptrdiff_t parentToAncestorRawOffset = flatTable[parentRowIndex + ancestorID];
                if (!IsUpcastPath(parentToAncestorRawOffset)) {
                    continue;
                }

                // Calculate actual physical byte offset
                const std::ptrdiff_t parentToAncestorPhysOffset = OffsetRawToPhysical(parentToAncestorRawOffset);
                const std::ptrdiff_t totalUpcastPhysOffset = subToDerivedPhysOffset + physicalStepOffset + parentToAncestorPhysOffset;
                const std::ptrdiff_t totalUpcastRawOffset = OffsetPhysicalToRaw(totalUpcastPhysOffset);

                // Update transitive paths
                flatTable[subClassRowIndex + ancestorID] = totalUpcastRawOffset;         // Upcast: subClass -> ancestor
                flatTable[ancestorID * capacity + subClassID] = -totalUpcastRawOffset;  // Downcast: ancestor -> subClass
            }
        }
    }
};

}; // namespace cast


// ==================================
// APIs
// ==================================
using QcDynamicCastEngine = cast::QcDynamicCast;

// QcDynamicCast only
inline void SetupExpectedCastClassCount(size_t expectedClassCount) {
    cast::QcDynamicCast::reserve(expectedClassCount);
}

template <typename ParentClass, typename DerivedClass>
inline void RegisterCastClass() noexcept {
    QcDynamicCastEngine::registerClass<ParentClass, DerivedClass>();
}

template <typename T>
inline QcCastClassID GetCastClassID() noexcept {
    return cast::QcCastClassIDRegistry::GetClassID<T>();
}

template <typename T>
inline T ObjectCast(void* rawPtr, QcCastClassID rawClassID) noexcept {
    return QcDynamicCastEngine::castTo<T>(rawPtr, rawClassID);
}

inline std::vector<std::ptrdiff_t> CollectAllBaseUpcastOffsets(QcCastClassID classID) {
    return QcDynamicCastEngine::collectAllBaseUpcastOffsets(classID);
}

NAMESPACE_QJS_END