#pragma once
#include <array>
#include <vector>
#include <list>
#include <deque>
#include <forward_list>
#include <set>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include "qjs_converter.h"

NAMESPACE_QJS_BEGIN

// is_stl_convertible_type_v
template<typename T>
static constexpr bool is_stl_convertible_type_v = !std::is_pointer<T>::value && has_js_native_converter_v<T>;

// ====================================
// std::array<T, N> <-> JS Array
// ====================================

template<typename T, std::size_t N>
struct QcJSValueToNative<std::array<T, N>,
    typename std::enable_if<is_stl_convertible_type_v<T>>::type
> {
    static std::array<T, N> FromJs(JSContext* ctx, JSValueConst val) {
        std::array<T, N> result{};

        if (!JS_IsArray(ctx, val)) {
            return result;
        }

        int64_t len = 0;
        JSValue len_val = JS_GetPropertyStr(ctx, val, "length");
        JS_ToInt64(ctx, &len, len_val);
        JS_FreeValue(ctx, len_val);

        std::size_t copy_len = (std::min)(static_cast<std::size_t>(len), N);

        for (std::size_t i = 0; i < copy_len; ++i) {
            JSValue elem_val = JS_GetPropertyUint32(ctx, val, static_cast<uint32_t>(i));
            result[i] = QcJSValueToNative<T>::FromJs(ctx, elem_val);
            JS_FreeValue(ctx, elem_val);
        }

        return result;
    }
};

template<typename T, std::size_t N>
struct QcNativeToJSValue<std::array<T, N>,
    typename std::enable_if<is_stl_convertible_type_v<T>>::type
> {
    static JSValue ToJs(JSContext* ctx, const std::array<T, N>& arr) {
        JSValue js_arr = JS_NewArray(ctx);
        if (JS_IsException(js_arr)) {
            return js_arr;
        }

        for (uint32_t i = 0; i < static_cast<uint32_t>(N); ++i) {
            JSValue elem_val = QcNativeToJSValue<T>::ToJs(ctx, arr[i]);
            JS_SetPropertyUint32(ctx, js_arr, i, elem_val);
        }

        return js_arr;
    }
};

// ====================================
// std::vector<T> <-> JS Array
// ====================================

template<typename T>
struct QcJSValueToNative<std::vector<T>,
    typename std::enable_if<is_stl_convertible_type_v<T>>::type
> {
    static std::vector<T> FromJs(JSContext* ctx, JSValueConst val) {
        std::vector<T> result;
        if (!JS_IsArray(ctx, val)) {
            return result;
        }

        int64_t len = 0;
        JSValue len_val = JS_GetPropertyStr(ctx, val, "length");
        JS_ToInt64(ctx, &len, len_val);
        JS_FreeValue(ctx, len_val);

        if (len <= 0) {
            return result;
        }

        result.reserve(static_cast<size_t>(len));
        for (int64_t i = 0; i < len; ++i) {
            JSValue elem_val = JS_GetPropertyUint32(ctx, val, static_cast<uint32_t>(i));
            result.push_back(QcJSValueToNative<T>::FromJs(ctx, elem_val));
            JS_FreeValue(ctx, elem_val);
        }

        return result;
    }
};

template<typename T>
struct QcNativeToJSValue<std::vector<T>,
    typename std::enable_if<is_stl_convertible_type_v<T>>::type
> {
    static JSValue ToJs(JSContext* ctx, const std::vector<T>& vec) {
        JSValue array = JS_NewArray(ctx);
        if (JS_IsException(array)) {
            return array;
        }

        for (uint32_t i = 0; i < static_cast<uint32_t>(vec.size()); ++i) {
            JSValue elem_val = QcNativeToJSValue<T>::ToJs(ctx, vec[i]);
            JS_SetPropertyUint32(ctx, array, i, elem_val);
        }

        return array;
    }
};

// ====================================
// std::list<T> <-> JS Array
// ====================================

template<typename T>
struct QcJSValueToNative<std::list<T>,
    typename std::enable_if<is_stl_convertible_type_v<T>>::type
> {
    static std::list<T> FromJs(JSContext* ctx, JSValueConst val) {
        std::list<T> result;
        if (!JS_IsArray(ctx, val)) {
            return result;
        }

        int64_t len = 0;
        JSValue len_val = JS_GetPropertyStr(ctx, val, "length");
        JS_ToInt64(ctx, &len, len_val);
        JS_FreeValue(ctx, len_val);

        for (int64_t i = 0; i < len; ++i) {
            JSValue elem_val = JS_GetPropertyUint32(ctx, val, static_cast<uint32_t>(i));
            result.push_back(QcJSValueToNative<T>::FromJs(ctx, elem_val));
            JS_FreeValue(ctx, elem_val);
        }

        return result;
    }
};

template<typename T>
struct QcNativeToJSValue<std::list<T>,
    typename std::enable_if<is_stl_convertible_type_v<T>>::type
> {
    static JSValue ToJs(JSContext* ctx, const std::list<T>& lst) {
        JSValue array = JS_NewArray(ctx);
        if (JS_IsException(array)) {
            return array;
        }

        uint32_t i = 0;
        for (const auto& item : lst) {
            JSValue elem_val = QcNativeToJSValue<T>::ToJs(ctx, item);
            JS_SetPropertyUint32(ctx, array, i++, elem_val);
        }

        return array;
    }
};

// ====================================
// std::map<K, V> <-> JS Object
// ====================================

template<typename K, typename V>
struct QcJSValueToNative<std::map<K, V>,
    typename std::enable_if<is_stl_convertible_type_v<K> && is_stl_convertible_type_v<V>>::type
> {
    static std::map<K, V> FromJs(JSContext* ctx, JSValueConst val) {
        std::map<K, V> result;
        if (!JS_IsObject(val)) {
            return result;
        }

        JSPropertyEnum* ptab = nullptr;
        uint32_t len = 0;
        if (JS_GetOwnPropertyNames(ctx, &ptab, &len, val, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) < 0) {
            return result;
        }

        for (uint32_t i = 0; i < len; ++i) {
            JSValue key_val = JS_AtomToValue(ctx, ptab[i].atom);
            JSValue prop_val = JS_GetProperty(ctx, val, ptab[i].atom);

            K key = QcJSValueToNative<K>::FromJs(ctx, key_val);
            V value = QcJSValueToNative<V>::FromJs(ctx, prop_val);
            result.emplace(std::move(key), std::move(value));

            JS_FreeValue(ctx, key_val);
            JS_FreeValue(ctx, prop_val);
            JS_FreeAtom(ctx, ptab[i].atom);
        }
        js_free(ctx, ptab);

        return result;
    }
};

template<typename K, typename V>
struct QcNativeToJSValue<std::map<K, V>,
    typename std::enable_if<is_stl_convertible_type_v<K>&& is_stl_convertible_type_v<V>>::type
> {
    static JSValue ToJs(JSContext* ctx, const std::map<K, V>& m) {
        JSValue obj = JS_NewObject(ctx);
        if (JS_IsException(obj)) {
            return obj;
        }

        for (const auto& kv : m) {
            JSValue key_val = QcNativeToJSValue<K>::ToJs(ctx, kv.first);
            JSValue val_val = QcNativeToJSValue<V>::ToJs(ctx, kv.second);

            JSAtom atom = JS_ValueToAtom(ctx, key_val);
            JS_SetProperty(ctx, obj, atom, val_val);

            JS_FreeAtom(ctx, atom);
            JS_FreeValue(ctx, key_val);
        }

        return obj;
    }
};


// ====================================
// std::deque<T> <-> JS Array
// ====================================

template<typename T>
struct QcJSValueToNative<std::deque<T>,
    typename std::enable_if<is_stl_convertible_type_v<T>>::type
> {
    static std::deque<T> FromJs(JSContext* ctx, JSValueConst val) {
        std::deque<T> result;
        if (!JS_IsArray(ctx, val)) {
            return result;
        }

        int64_t len = 0;
        JSValue len_val = JS_GetPropertyStr(ctx, val, "length");
        JS_ToInt64(ctx, &len, len_val);
        JS_FreeValue(ctx, len_val);

        for (int64_t i = 0; i < len; ++i) {
            JSValue elem_val = JS_GetPropertyUint32(ctx, val, static_cast<uint32_t>(i));
            result.push_back(QcJSValueToNative<T>::FromJs(ctx, elem_val));
            JS_FreeValue(ctx, elem_val);
        }

        return result;
    }
};

template<typename T>
struct QcNativeToJSValue<std::deque<T>,
    typename std::enable_if<is_stl_convertible_type_v<T>>::type
> {
    static JSValue ToJs(JSContext* ctx, const std::deque<T>& deq) {
        JSValue array = JS_NewArray(ctx);
        if (JS_IsException(array)) {
            return array;
        }

        uint32_t i = 0;
        for (const auto& item : deq) {
            JSValue elem_val = QcNativeToJSValue<T>::ToJs(ctx, item);
            JS_SetPropertyUint32(ctx, array, i++, elem_val);
        }

        return array;
    }
};

// ====================================
// std::forward_list<T> <-> JS Array
// ====================================

template<typename T>
struct QcJSValueToNative<std::forward_list<T>,
    typename std::enable_if<is_stl_convertible_type_v<T>>::type
> {
    static std::forward_list<T> FromJs(JSContext* ctx, JSValueConst val) {
        std::forward_list<T> result;
        if (!JS_IsArray(ctx, val)) {
            return result;
        }

        int64_t len = 0;
        JSValue len_val = JS_GetPropertyStr(ctx, val, "length");
        JS_ToInt64(ctx, &len, len_val);
        JS_FreeValue(ctx, len_val);
        if (len <= 0) {
            return result;
        }

        // Insert in reverse order to maintain the original sequence.
        for (int64_t i = len - 1; i >= 0; --i) {
            JSValue elem_val = JS_GetPropertyUint32(ctx, val, static_cast<uint32_t>(i));
            result.push_front(QcJSValueToNative<T>::FromJs(ctx, elem_val));
            JS_FreeValue(ctx, elem_val);
        }

        return result;
    }
};

template<typename T>
struct QcNativeToJSValue<std::forward_list<T>,
    typename std::enable_if<is_stl_convertible_type_v<T>>::type
> {
    static JSValue ToJs(JSContext* ctx, const std::forward_list<T>& flst) {
        JSValue array = JS_NewArray(ctx);
        if (JS_IsException(array)) {
            return array;
        }

        uint32_t i = 0;
        for (const auto& item : flst) {
            JSValue elem_val = QcNativeToJSValue<T>::ToJs(ctx, item);
            JS_SetPropertyUint32(ctx, array, i++, elem_val);
        }

        return array;
    }
};

// ====================================
// std::set<T> <-> JS Array
// ====================================

template<typename T>
struct QcJSValueToNative<std::set<T>,
    typename std::enable_if<is_stl_convertible_type_v<T>>::type
> {
    static std::set<T> FromJs(JSContext* ctx, JSValueConst val) {
        std::set<T> result;
        if (!JS_IsArray(ctx, val)) {
            return result;
        }

        int64_t len = 0;
        JSValue len_val = JS_GetPropertyStr(ctx, val, "length");
        JS_ToInt64(ctx, &len, len_val);
        JS_FreeValue(ctx, len_val);

        for (int64_t i = 0; i < len; ++i) {
            JSValue elem_val = JS_GetPropertyUint32(ctx, val, static_cast<uint32_t>(i));
            result.insert(QcJSValueToNative<T>::FromJs(ctx, elem_val));
            JS_FreeValue(ctx, elem_val);
        }

        return result;
    }
};

template<typename T>
struct QcNativeToJSValue<std::set<T>,
    typename std::enable_if<is_stl_convertible_type_v<T>>::type
> {
    static JSValue ToJs(JSContext* ctx, const std::set<T>& s) {
        JSValue array = JS_NewArray(ctx);
        if (JS_IsException(array)) {
            return array;
        }

        uint32_t i = 0;
        for (const auto& item : s) {
            JSValue elem_val = QcNativeToJSValue<T>::ToJs(ctx, item);
            JS_SetPropertyUint32(ctx, array, i++, elem_val);
        }

        return array;
    }
};

// ====================================
// std::unordered_set<T> <-> JS Array
// ====================================

template<typename T>
struct QcJSValueToNative<std::unordered_set<T>,
    typename std::enable_if<is_stl_convertible_type_v<T>>::type
> {
    static std::unordered_set<T> FromJs(JSContext* ctx, JSValueConst val) {
        std::unordered_set<T> result;
        if (!JS_IsArray(ctx, val)) {
            return result;
        }

        int64_t len = 0;
        JSValue len_val = JS_GetPropertyStr(ctx, val, "length");
        JS_ToInt64(ctx, &len, len_val);
        JS_FreeValue(ctx, len_val);

        for (int64_t i = 0; i < len; ++i) {
            JSValue elem_val = JS_GetPropertyUint32(ctx, val, static_cast<uint32_t>(i));
            result.insert(QcJSValueToNative<T>::FromJs(ctx, elem_val));
            JS_FreeValue(ctx, elem_val);
        }

        return result;
    }
};

template<typename T>
struct QcNativeToJSValue<std::unordered_set<T>,
    typename std::enable_if<is_stl_convertible_type_v<T>>::type
> {
    static JSValue ToJs(JSContext* ctx, const std::unordered_set<T>& s) {
        JSValue array = JS_NewArray(ctx);
        if (JS_IsException(array)) {
            return array;
        }

        uint32_t i = 0;
        for (const auto& item : s) {
            JSValue elem_val = QcNativeToJSValue<T>::ToJs(ctx, item);
            JS_SetPropertyUint32(ctx, array, i++, elem_val);
        }

        return array;
    }
};

// ====================================
// std::unordered_map<K, V> <-> JS Object
// ====================================

template<typename K, typename V>
struct QcJSValueToNative<std::unordered_map<K, V>,
    typename std::enable_if<is_stl_convertible_type_v<K>&& is_stl_convertible_type_v<V>>::type
> {
    static std::unordered_map<K, V> FromJs(JSContext* ctx, JSValueConst val) {
        std::unordered_map<K, V> result;
        if (!JS_IsObject(val)) {
            return result;
        }

        JSPropertyEnum* ptab = nullptr;
        uint32_t len = 0;
        if (JS_GetOwnPropertyNames(ctx, &ptab, &len, val, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) < 0) {
            return result;
        }

        for (uint32_t i = 0; i < len; ++i) {
            JSValue key_val = JS_AtomToValue(ctx, ptab[i].atom);
            JSValue prop_val = JS_GetProperty(ctx, val, ptab[i].atom);

            K key = QcJSValueToNative<K>::FromJs(ctx, key_val);
            V value = QcJSValueToNative<V>::FromJs(ctx, prop_val);
            result.emplace(std::move(key), std::move(value));

            JS_FreeValue(ctx, key_val);
            JS_FreeValue(ctx, prop_val);
            JS_FreeAtom(ctx, ptab[i].atom);
        }
        js_free(ctx, ptab);

        return result;
    }
};

template<typename K, typename V>
struct QcNativeToJSValue<std::unordered_map<K, V>,
    typename std::enable_if<is_stl_convertible_type_v<K>&& is_stl_convertible_type_v<V>>::type
> {
    static JSValue ToJs(JSContext* ctx, const std::unordered_map<K, V>& m) {
        JSValue obj = JS_NewObject(ctx);
        if (JS_IsException(obj)) {
            return obj;
        }

        for (const auto& kv : m) {
            JSValue key_val = QcNativeToJSValue<K>::ToJs(ctx, kv.first);
            JSValue val_val = QcNativeToJSValue<V>::ToJs(ctx, kv.second);

            JSAtom atom = JS_ValueToAtom(ctx, key_val);
            JS_SetProperty(ctx, obj, atom, val_val);

            JS_FreeAtom(ctx, atom);
            JS_FreeValue(ctx, key_val);
        }

        return obj;
    }
};

// ====================================
// std::multiset<T> <-> JS Array
// ====================================

template<typename T>
struct QcJSValueToNative<std::multiset<T>,
    typename std::enable_if<is_stl_convertible_type_v<T>>::type
> {
    static std::multiset<T> FromJs(JSContext* ctx, JSValueConst val) {
        std::multiset<T> result;
        if (!JS_IsArray(ctx, val)) {
            return result;
        }

        int64_t len = 0;
        JSValue len_val = JS_GetPropertyStr(ctx, val, "length");
        JS_ToInt64(ctx, &len, len_val);
        JS_FreeValue(ctx, len_val);

        for (int64_t i = 0; i < len; ++i) {
            JSValue elem_val = JS_GetPropertyUint32(ctx, val, static_cast<uint32_t>(i));
            result.insert(QcJSValueToNative<T>::FromJs(ctx, elem_val));
            JS_FreeValue(ctx, elem_val);
        }

        return result;
    }
};

template<typename T>
struct QcNativeToJSValue<std::multiset<T>,
    typename std::enable_if<is_stl_convertible_type_v<T>>::type
> {
    static JSValue ToJs(JSContext* ctx, const std::multiset<T>& ms) {
        JSValue array = JS_NewArray(ctx);
        if (JS_IsException(array)) {
            return array;
        }

        uint32_t i = 0;
        for (const auto& item : ms) {
            JSValue elem_val = QcNativeToJSValue<T>::ToJs(ctx, item);
            JS_SetPropertyUint32(ctx, array, i++, elem_val);
        }

        return array;
    }
};

// ====================================
// std::unordered_multiset<T> <-> JS Array
// ====================================

template<typename T>
struct QcJSValueToNative<std::unordered_multiset<T>,
    typename std::enable_if<is_stl_convertible_type_v<T>>::type
> {
    static std::unordered_multiset<T> FromJs(JSContext* ctx, JSValueConst val) {
        std::unordered_multiset<T> result;
        if (!JS_IsArray(ctx, val)) {
            return result;
        }

        int64_t len = 0;
        JSValue len_val = JS_GetPropertyStr(ctx, val, "length");
        JS_ToInt64(ctx, &len, len_val);
        JS_FreeValue(ctx, len_val);

        for (int64_t i = 0; i < len; ++i) {
            JSValue elem_val = JS_GetPropertyUint32(ctx, val, static_cast<uint32_t>(i));
            result.insert(QcJSValueToNative<T>::FromJs(ctx, elem_val));
            JS_FreeValue(ctx, elem_val);
        }

        return result;
    }
};

template<typename T>
struct QcNativeToJSValue<std::unordered_multiset<T>,
    typename std::enable_if<is_stl_convertible_type_v<T>>::type
> {
    static JSValue ToJs(JSContext* ctx, const std::unordered_multiset<T>& ums) {
        JSValue array = JS_NewArray(ctx);
        if (JS_IsException(array)) {
            return array;
        }

        uint32_t i = 0;
        for (const auto& item : ums) {
            JSValue elem_val = QcNativeToJSValue<T>::ToJs(ctx, item);
            JS_SetPropertyUint32(ctx, array, i++, elem_val);
        }

        return array;
    }
};

// =============================================
// std::multimap<K, V> <-> JS Array or JS Object
// JS Array: [[k1, v1], [k2, v2], ...]
// JS Object: { k1: v1, k2: v2 }
// =============================================

template<typename K, typename V>
struct QcJSValueToNative<std::multimap<K, V>,
    typename std::enable_if<is_stl_convertible_type_v<K>&& is_stl_convertible_type_v<V>>::type
> {
    static std::multimap<K, V> FromJs(JSContext* ctx, JSValueConst val) {
        std::multimap<K, V> result;

        if (JS_IsArray(ctx, val)) { // JS Array: [[k1, v1], [k2, v2], ...]
            int64_t len = 0;
            JSValue len_val = JS_GetPropertyStr(ctx, val, "length");
            JS_ToInt64(ctx, &len, len_val);
            JS_FreeValue(ctx, len_val);

            for (int64_t i = 0; i < len; ++i) {
                JSValue pair_val = JS_GetPropertyUint32(ctx, val, static_cast<uint32_t>(i));
                if (JS_IsArray(ctx, pair_val)) {
                    JSValue key_val = JS_GetPropertyUint32(ctx, pair_val, 0);
                    JSValue prop_val = JS_GetPropertyUint32(ctx, pair_val, 1);

                    K key = QcJSValueToNative<K>::FromJs(ctx, key_val);
                    V value = QcJSValueToNative<V>::FromJs(ctx, prop_val);
                    result.insert(std::make_pair(key, value));

                    JS_FreeValue(ctx, key_val);
                    JS_FreeValue(ctx, prop_val);
                }
                JS_FreeValue(ctx, pair_val);
            }
        } else if (JS_IsObject(val)) { // JS Object: { k1: v1, k2: v2 }
            JSPropertyEnum* ptab = nullptr;
            uint32_t len = 0;
            if (JS_GetOwnPropertyNames(ctx, &ptab, &len, val, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) < 0) {
                return result;
            }

            for (uint32_t i = 0; i < len; ++i) {
                JSValue key_val = JS_AtomToValue(ctx, ptab[i].atom);
                JSValue prop_val = JS_GetProperty(ctx, val, ptab[i].atom);

                K key = QcJSValueToNative<K>::FromJs(ctx, key_val);
                V value = QcJSValueToNative<V>::FromJs(ctx, prop_val);
                result.insert(std::make_pair(key, value));

                JS_FreeValue(ctx, key_val);
                JS_FreeValue(ctx, prop_val);
                JS_FreeAtom(ctx, ptab[i].atom);
            }

            js_free(ctx, ptab);
        }

        //
        return result;
    }
};

// std::multimap<K, V> -> JS Array: [[k1, v1], [k2, v2], ...]
template<typename K, typename V>
struct QcNativeToJSValue<std::multimap<K, V>,
    typename std::enable_if<is_stl_convertible_type_v<K>&& is_stl_convertible_type_v<V>>::type
> {
    static JSValue ToJs(JSContext* ctx, const std::multimap<K, V>& mm) {
        JSValue array = JS_NewArray(ctx);
        if (JS_IsException(array)) {
            return array;
        }

        uint32_t i = 0;
        for (const auto& kv : mm) {
            JSValue pair_arr = JS_NewArray(ctx);
            JS_SetPropertyUint32(ctx, pair_arr, 0, QcNativeToJSValue<K>::ToJs(ctx, kv.first));
            JS_SetPropertyUint32(ctx, pair_arr, 1, QcNativeToJSValue<V>::ToJs(ctx, kv.second));

            JS_SetPropertyUint32(ctx, array, i++, pair_arr);
        }

        return array;
    }
};

// =============================================
// std::unordered_multimap<K, V> <-> JS Array or JS Object
// JS Array: [[k1, v1], [k2, v2], ...]
// JS Object: { k1: v1, k2: v2 }
// =============================================

template<typename K, typename V>
struct QcJSValueToNative<std::unordered_multimap<K, V>,
    typename std::enable_if<is_stl_convertible_type_v<K>&& is_stl_convertible_type_v<V>>::type
> {
    static std::unordered_multimap<K, V> FromJs(JSContext* ctx, JSValueConst val) {
        std::unordered_multimap<K, V> result;

        if (JS_IsArray(ctx, val)) { // JS Array: [[k1, v1], [k2, v2], ...]
            int64_t len = 0;
            JSValue len_val = JS_GetPropertyStr(ctx, val, "length");
            JS_ToInt64(ctx, &len, len_val);
            JS_FreeValue(ctx, len_val);

            for (int64_t i = 0; i < len; ++i) {
                JSValue pair_val = JS_GetPropertyUint32(ctx, val, static_cast<uint32_t>(i));
                if (JS_IsArray(ctx, pair_val)) {
                    JSValue key_val = JS_GetPropertyUint32(ctx, pair_val, 0);
                    JSValue prop_val = JS_GetPropertyUint32(ctx, pair_val, 1);

                    K key = QcJSValueToNative<K>::FromJs(ctx, key_val);
                    V value = QcJSValueToNative<V>::FromJs(ctx, prop_val);
                    result.insert(std::make_pair(key, value));

                    JS_FreeValue(ctx, key_val);
                    JS_FreeValue(ctx, prop_val);
                }
                JS_FreeValue(ctx, pair_val);
            }
        } else if (JS_IsObject(val)) { // JS Object: { k1: v1, k2: v2 }
            JSPropertyEnum* ptab = nullptr;
            uint32_t len = 0;
            if (JS_GetOwnPropertyNames(ctx, &ptab, &len, val, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) < 0) {
                return result;
            }

            for (uint32_t i = 0; i < len; ++i) {
                JSValue key_val = JS_AtomToValue(ctx, ptab[i].atom);
                JSValue prop_val = JS_GetProperty(ctx, val, ptab[i].atom);

                K key = QcJSValueToNative<K>::FromJs(ctx, key_val);
                V value = QcJSValueToNative<V>::FromJs(ctx, prop_val);
                result.insert(std::make_pair(key, value));
                
                JS_FreeValue(ctx, key_val);
                JS_FreeValue(ctx, prop_val);
                JS_FreeAtom(ctx, ptab[i].atom);
            }

            js_free(ctx, ptab);
        }

        //
        return result;
    }
};

// std::unordered_multimap<K, V> -> JS Array: [[k1, v1], [k2, v2], ...]
template<typename K, typename V>
struct QcNativeToJSValue<std::unordered_multimap<K, V>,
    typename std::enable_if<is_stl_convertible_type_v<K>&& is_stl_convertible_type_v<V>>::type
> {
    static JSValue ToJs(JSContext* ctx, const std::unordered_multimap<K, V>& umm) {
        JSValue array = JS_NewArray(ctx);
        if (JS_IsException(array)) {
            return array;
        }

        uint32_t i = 0;
        for (const auto& kv : umm) {
            JSValue pair_arr = JS_NewArray(ctx);
            JS_SetPropertyUint32(ctx, pair_arr, 0, QcNativeToJSValue<K>::ToJs(ctx, kv.first));
            JS_SetPropertyUint32(ctx, pair_arr, 1, QcNativeToJSValue<V>::ToJs(ctx, kv.second));

            JS_SetPropertyUint32(ctx, array, i++, pair_arr);
        }

        return array;
    }
};

NAMESPACE_QJS_END