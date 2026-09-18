#pragma once

#include <cstdio>
#include <string>
#include <unordered_map>
#include <type_traits>
#include <typeindex>
#include <memory>
#include <utility>
#include "quickjs.h"
#include "qjs_global.h"
#include "qjs_utility.h"
#include "qjs_opaque.h"

// ===========================================
// QcJSValueToNative：JSValue -> ValueType (Not Pointer)
// QcNativeToJSValue：ValueType / T* (ExportedClass) -> JSValue
// ===========================================

NAMESPACE_QJS_BEGIN

// Error of QcJSValueToNative
// Note: Do not implement the `FromJs` function. Otherwise, 
// the `has_js_to_native` check will be incorrect.
template<typename T, typename Enable = void>
struct QcJSValueToNative {
	static T FromJs_Error(JSContext* ctx, JSValueConst val) {
		using ERROR_TYPE = typename T::NOT_EXIST_ERROR_TYPE;
		QJS_STATIC_ASSERT_M(always_false_v<T>, "QcJSValueToNative only supports value type (no pointer)");
		return T{};
	}
};

// Error of QcNativeToJSValue
// Note: Do not implement the `FromJs` function. Otherwise, 
// the `has_native_to_js` check will be incorrect.
template<typename T, typename Enable = void>
struct QcNativeToJSValue {
	static JSValue ToJs_Error(JSContext* ctx, T obj) {
		using ERROR_TYPE = typename T::NOT_EXIST_ERROR_TYPE;
		QJS_STATIC_ASSERT_M(always_false_v<T>, "QcNativeToJSValue only supports value type or exported class pointer");
		return JS_UNDEFINED;
	}
};

// std::string
template<>
struct QcJSValueToNative<std::string> {
	static std::string FromJs(JSContext* ctx, JSValueConst val) {
		const char* str = JS_ToCString(ctx, val);
		if (!str) {
			return "";
		}

		std::string result(str);
		JS_FreeCString(ctx, str);
		return result;
	}
};

template<>
struct QcNativeToJSValue<std::string> {
	static JSValue ToJs(JSContext* ctx, const std::string& val) {
		return JS_NewString(ctx, val.c_str());
	}
};

// std::wstring
template<>
struct QcJSValueToNative<std::wstring> {
	static std::wstring FromJs(JSContext* ctx, JSValueConst val) {
		const char* str = JS_ToCString(ctx, val);
		if (!str) {
			return L"";
		}

		std::string result(str);
		JS_FreeCString(ctx, str);
		return str2wstr(result);
	}
};

template<>
struct QcNativeToJSValue<std::wstring> {
	static JSValue ToJs(JSContext* ctx, const std::wstring& val) {
		std::string s = wstr2str(val);
		return JS_NewString(ctx, s.c_str());
	}
};

// int32_t
template<>
struct QcJSValueToNative<int32_t> {
	static int32_t FromJs(JSContext* ctx, JSValueConst val) {
		int32_t res = 0;
		JS_ToInt32(ctx, &res, val);
		return res;
	}
};

template<>
struct QcNativeToJSValue<int32_t> {
	static JSValue ToJs(JSContext* ctx, int32_t val) {
		return JS_NewInt32(ctx, val);
	}
};

// bool
template<>
struct QcJSValueToNative<bool> {
	static bool FromJs(JSContext* ctx, JSValueConst val) {
		return JS_ToBool(ctx, val) > 0;
	}
};

template<>
struct QcNativeToJSValue<bool> {
	static JSValue ToJs(JSContext* ctx, bool val) {
		return JS_NewBool(ctx, val);
	}
};

// double 
template<>
struct QcJSValueToNative<double> {
	static double FromJs(JSContext* ctx, JSValueConst val) {
		double res = 0.0;
		JS_ToFloat64(ctx, &res, val);
		return res;
	}
};

template<>
struct QcNativeToJSValue<double> {
	static JSValue ToJs(JSContext* ctx, double val) {
		return JS_NewFloat64(ctx, val);
	}
};

// float
template<>
struct QcJSValueToNative<float> {
	static float FromJs(JSContext* ctx, JSValueConst val) {
		return static_cast<float>(QcJSValueToNative<double>::FromJs(ctx, val));
	}
};

template<>
struct QcNativeToJSValue<float> {
	static JSValue ToJs(JSContext* ctx, float val) {
		return QcNativeToJSValue<double>::ToJs(ctx, static_cast<double>(val));
	}
};

// uint32_t
template<>
struct QcJSValueToNative<uint32_t> {
	static uint32_t FromJs(JSContext* ctx, JSValueConst val) {
		uint32_t res = 0;
		JS_ToUint32(ctx, &res, val);
		return res;
	}
};

template<>
struct QcNativeToJSValue<uint32_t> {
	static JSValue ToJs(JSContext* ctx, uint32_t val) {
		return JS_NewUint32(ctx, val);
	}
};

// int64_t / long long
template<>
struct QcJSValueToNative<int64_t> {
	static int64_t FromJs(JSContext* ctx, JSValueConst val) {
		int64_t res = 0;
		JS_ToInt64(ctx, &res, val);
		return res;
	}
};

template<>
struct QcNativeToJSValue<int64_t> {
	static JSValue ToJs(JSContext* ctx, int64_t val) {
		return JS_NewInt64(ctx, val);
	}
};

// uint64_t / unsigned long long
#ifndef JS_NAN_BOXING
template<>
struct QcJSValueToNative<uint64_t> {
	static uint64_t FromJs(JSContext* ctx, JSValueConst val) {
		int64_t res = 0;
		JS_ToInt64(ctx, &res, val); 
		return static_cast<uint64_t>(res);
	}
};

template<>
struct QcNativeToJSValue<uint64_t> {
	static JSValue ToJs(JSContext* ctx, uint64_t val) {
		return JS_NewInt64(ctx, static_cast<int64_t>(val));
	}
};
#endif

// JSValue
template<>
struct QcJSValueToNative<JSValue> {
	static JSValue FromJs(JSContext* ctx, JSValueConst val) {
		return val;
	}
};

template<>
struct QcNativeToJSValue<JSValue> {
	static JSValue ToJs(JSContext* ctx, JSValue val) {
		return val;
	}
};

// JSValue -> exported class T*
template<typename T>
struct QcJSValueToNative<T*, typename std::enable_if<is_exported_class_v<T>>::type> {
	static T* FromJs(JSContext* ctx, JSValueConst val) {
		T* ptr = QcOpaque::GetOpaquePtr<T>(ctx, val);
		return ptr;
	}
};

// Forbidden: JSValue -> exported class T*
// For raw pointers `T*` (exported classes), `QcNativeToJSValue` prevents implicit direct 
// conversion. requiring the Invoker to specify a conversion policy. It is because the 
// memory management for such objects is ambiguous.
template <typename T>
struct QcNativeToJSValue<T*, typename std::enable_if<is_exported_class_v<T>>::type> {
    static JSValue ToJs(JSContext* ctx, T* ptr) {
        QJS_STATIC_ASSERT_M(!is_exported_class_v<T>,
            "Direct raw pointer return is forbidden! Explicit QcReturnPolicy required in AddNativeFunc.");
        return JS_UNDEFINED;
    }
};

// T (exported_class + copy_constructible + default_constructible) -> JSValue
template<typename T>
struct QcJSValueToNative< T,
    typename std::enable_if<
		!std::is_pointer<T>::value &&
		is_exported_class_v<T> &&
		std::is_copy_constructible<T>::value &&
		std::is_default_constructible<T>::value
    >::type
> {
    static T FromJs(JSContext* ctx, JSValueConst val) {
        T* ptr = QcOpaque::GetOpaquePtr<T>(ctx, val);
        if (!ptr) {
            return T{};
        }
        return *ptr;
    }
};

// JSValue ->  T (exported_class + copy_constructible + default_constructible)
template<typename T>
struct QcNativeToJSValue<T,
    typename std::enable_if<
		!std::is_pointer<T>::value &&
		is_exported_class_v<T> &&
		std::is_copy_constructible<T>::value &&
		std::is_default_constructible<T>::value
    >::type
> {
    static JSValue ToJs(JSContext* ctx, T&& obj) {
        T* new_ptr = new T(std::move(obj));
        return QcOpaque::WrapNewOwnedByJS<T>(ctx, new_ptr);
    }

    static JSValue ToJs(JSContext* ctx, const T& obj) {
        T* new_ptr = new T(obj);
        return QcOpaque::WrapNewOwnedByJS<T>(ctx, new_ptr);
    }
};

NAMESPACE_QJS_END



// =====================================================
// QC_DEFINE_EXPORTED_CLASS_JSVALUE_CONVERTER
// Define to support param type T , T&& of ExportedClass without default constructor, 
// For NonExportedClass, you have to implement QcJSValueToNative<T> and 
// QcNativeToJSValue<T> by yourself. Please note that T must be Copy Constructible
// =====================================================

// Defines JSValue <-> T of exported class which with ErrorValue
// ErrorValue is used as return object of ClassType when `FromJs` failed
// e.g. 
//	If Rect does not has default constructor, then use this API to implement 
//	type conversion of  JSValue <-> T for exported classes.
//		QC_DEFINE_EXPORTED_CLASS_JSVALUE_CONVERTER(Rect, Rect(0,0,0,0)); 
//
#define QC_DEFINE_EXPORTED_CLASS_JSVALUE_CONVERTER(ClassType, ErrorValue) \
template<>                                                                    \
struct QcJSValueToNative<ClassType> {                                         \
	QJS_STATIC_ASSERT_M(std::is_copy_constructible<ClassType>::value,  \
    "ClassType must be Copy Constructible"); \
\
	static ClassType FromJs(JSContext* ctx, JSValueConst val) {               \
		ClassType* ptr = QcOpaque::GetOpaquePtr<ClassType>(ctx, val);          \
		if (!ptr) {                                                           \
			return ErrorValue;                                               \
		}                                                                     \
		return *ptr;                                                          \
	}                                                                         \
};                                                                            \
template<>                                                                    \
struct QcNativeToJSValue<ClassType> {                                         \
	static JSValue ToJs(JSContext* ctx, ClassType&& obj) {               \
		ClassType* new_ptr = new ClassType(std::move(obj));                              \
		return QcOpaque::WrapNewOwnedByJS<ClassType>(ctx, new_ptr); \
	}     \
	static JSValue ToJs(JSContext* ctx, const ClassType& obj) {               \
		ClassType* new_ptr = new ClassType(obj);                              \
		return QcOpaque::WrapNewOwnedByJS<ClassType>(ctx, new_ptr); \
	}     \
};                                                                            