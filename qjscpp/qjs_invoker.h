/** ************************************************************************************************
  Parameter Types & Requirements
  1. Non-Exported Types (int, float, std::string...)
	 - Supports T, T&, T&&, T*
	  - Requirements: QcJSValueToNative<T> implemented, T must be Copy Constructible

  2. Exported Classes (Exported via QcClass)
	 - Supports T : Requires QcJSValueToNative<T>, T must be Copy Constructible
	 - Supports T& : Requires T to be an Exported Class (modifies native instance directly)
	 - Supports T* : Requires T to be an Exported Class
	 - Supported T&& : Requires QcJSValueToNative<T>, T must be Copy Constructible

  3. Explicit QcArg Wrappers
	 - Supports QcArg<T*> : Requires T to be an Exported Class Pointer
	 - Supports QcArg<T>  : Requires QcJSValueToNative<T>, T must be Copy Constructible


  NOTES:
  1. SPECIAL CASE: Opaque Pointer Passthrough & Re-wrapping
	  If a function needs to extract an exported native pointer from an incoming JS argument
	  and pass it back to JS as a JSValue:
	   1). Use QcArg<NativeClass*> as the function parameter to obtain the raw JSValue from JS.
	   2). Use QcOpaque to extract the native C++ pointer from the JSValue, and re-wrap it into
			 a JSValue to return to JS.

  2. T of QcArg<T>
	 The parameter for QcArg<T> ONLY supports:
	 1). Exported class raw pointers, written as QcArg<ExportedClass*>
	 2). Plain value-convertible types (has QcJSValueToNative<T>), written as QcArg<ValueType>
		  (no cv no ref...), which means QcArg<const Class&> is Illegal.
  ************************************************************************************************ */

#pragma once
#include <string>
#include <type_traits>
#include <utility>
#include "quickjs.h"
#include "qjs_global.h"
#include "qjs_converter.h"
#include "qjs_arg.h"
#include "qjs_opaque.h"
#include "qjs_traits.h"
#include "qjs_utility.h"
#include "qjs_debug.h"


NAMESPACE_QJS_BEGIN

/************************************************************************************************
// QcParamsTypeChecker: Invoker param types checker
// QcReturnTypeChecker: : Invoker return types checker
************************************************************************************************/

// ========================================
// QcParamTypeCheckerTPP: Error checker of parameter type T**
// ========================================
template <typename T, typename Enable = void>
struct QcParamTypeCheckerTPP {
    using type = void;
    static constexpr const char* error = "No error";
};

// T**: Error
template <typename T>
struct QcParamTypeCheckerTPP<T**> {
    using type = void;
    static constexpr const char* error = "Unsupported invoker paramter type: T** etc.";
    QJS_STATIC_ASSERT_M(always_false_v<T>, "Unsupported invoker paramter type: T** etc.");
};

// Mid-level const: T* const*
template <typename T>
struct QcParamTypeCheckerTPP<T* const*>
    : QcParamTypeCheckerTPP<T**> {
};

// Top-level const：Foo** const
template <typename T>
struct QcParamTypeCheckerTPP<T, typename std::enable_if<std::is_const<T>::value>::type>
    : QcParamTypeCheckerTPP<std::remove_const_t<T>> {
};


// ========================================================
// QcParamRequirementsChecker: Validation for param types
// 1. T*, T& of Exported Class: No requirements.
// 2. QcArg<T*> Wrapper: Requires T to be an Exported Class.
// 3. QcArg<T> Wrapper: Requires QcJSValueToNative<T> and T must be Copy Constructible.
// 4. Other Types: Requires QcJSValueToNative<T> and T must be Copy Constructible.
// ========================================================

template <typename T, typename Enable = void>
struct QcParamRequirementsChecker {
    using type = void;
};

// T* T& of exported classes
template <typename T>
struct QcParamRequirementsChecker<T, typename std::enable_if<
    is_exported_class_v<pure_type_t<T>> && (std::is_pointer<T>::value || std::is_lvalue_reference<T>::value)
>::type
> {
    using type = void;
};

// C style string
template <> struct QcParamRequirementsChecker<char*> { using type = void; };
template <> struct QcParamRequirementsChecker<const char*> { using type = void; };
template <> struct QcParamRequirementsChecker<wchar_t*> { using type = void; };
template <> struct QcParamRequirementsChecker<const wchar_t*> { using type = void; };

// QcArg<T*> for exported classes
template <typename T>
struct QcParamRequirementsChecker<QcArg<T*>> {
    using type = void;
    QJS_STATIC_ASSERT_M(is_exported_class_v<T>,
        "QcArg<T*> parameter requires T to be an Exported Class!");
};

// QcArg<T>
template <typename T>
struct QcParamRequirementsChecker<QcArg<T>> {
    using PureT = pure_type_t<T>;
    using type = void;

    QJS_STATIC_ASSERT_M(has_js_to_native_v<PureT>,
        "QcArg<T> parameter type must support QcJSValueToNative<T>");
    QJS_STATIC_ASSERT_M(std::is_copy_constructible<PureT>::value,
        "QcArg<T> parameter type must be Copy Constructible");
};

// Except for: T* T& of exported classes and string
template <typename T>
struct QcParamRequirementsChecker<T, typename std::enable_if<
    !(is_exported_class_v<pure_type_t<T>> && (std::is_pointer<T>::value || std::is_lvalue_reference<T>::value))
    && !std::is_same<pure_type_t<T>, char>::value
    && !std::is_same<pure_type_t<T>, wchar_t>::value
    && !is_qc_arg_v<T>
>::type
> {
    using PureT = pure_type_t<T>;
    using type = void;

    QJS_STATIC_ASSERT_M(has_js_to_native_v<PureT>,
        "Parameter type must support QcJSValueToNative<T>");
    QJS_STATIC_ASSERT_M(std::is_copy_constructible<PureT>::value,
        "Parameter type must be Copy Constructible");
};

// =====================
// QcParamsTypeChecker
// =====================
template <typename... Args>
struct QcParamsTypeChecker {
    using CheckParamsTypeTPP = void_ty<typename QcParamTypeCheckerTPP<Args>::type...>;
    using CheckRequirements = void_ty<typename QcParamRequirementsChecker<Args>::type...>;

    using type = void_ty<CheckParamsTypeTPP, CheckRequirements>;
};


// ====================================================
// QcReturnRequirementsChecker: Validation for return types
// 1. T, T&, T&&: Requires QcNativeToJSValue<T> and T must be Copy Constructible.
// 2. T* of Non-Exported Types / Primitives(int...): Strictly forbidden.
// 3. T* of Exported Class: Requires explicit Policy (Policy != QcReturnPolicy::Default).
// ====================================================
template <typename Ret, QcReturnPolicy Policy, typename Enable = void>
struct QcReturnRequirementsChecker {
    using type = void;
};

// void
template <QcReturnPolicy Policy>
struct QcReturnRequirementsChecker<void, Policy> {
    using type = void;
};

// T, T&, T&&: has  QcNativeToJSValue<T> and T must be Copy Constructible
template <typename Ret, QcReturnPolicy Policy>
struct QcReturnRequirementsChecker<Ret, Policy, typename std::enable_if<!std::is_pointer<Ret>::value>::type> {
    using type = void;
    using PureT = pure_type_t<Ret>;
    QJS_STATIC_ASSERT_M(has_native_to_js_v<PureT>,
        "Return type must support QcNativeToJSValue<T>");
    QJS_STATIC_ASSERT_M(std::is_copy_constructible<PureT>::value,
        "Non-pointer return type must be Copy Constructible");
};

// T* (NonExportedClass): Forbidden
template <typename Ret, QcReturnPolicy Policy>
struct QcReturnRequirementsChecker<Ret, Policy, typename std::enable_if<
    std::is_pointer<Ret>::value && !is_exported_class_v<typename std::remove_pointer<Ret>::type>
>::type> {
    using type = void;
    QJS_STATIC_ASSERT_M(always_false_v<Ret>,
        "Returning raw pointer of non-exported types/primitives(int...) is strictly unsupported!");
};

// T* (ExportedClass) -> (Policy != QcReturnPolicy::Default)
template <typename Ret, QcReturnPolicy Policy>
struct QcReturnRequirementsChecker<Ret, Policy, typename std::enable_if<
    std::is_pointer<Ret>::value&& is_exported_class_v<typename std::remove_pointer<Ret>::type>
>::type> {
    using type = void;
    QJS_STATIC_ASSERT_M(Policy != QcReturnPolicy::Default,
        "Exported class pointer return type requires explicit QcReturnPolicy in AddNativeFunc!");
};


// =====================
// QcReturnTypeChecker
// =====================
template <QcReturnPolicy Policy, typename Ret>
struct QcReturnTypeChecker {
    using CheckReturn = void_ty<typename QcReturnRequirementsChecker<Ret, Policy>::type>;
    using type = void_ty<CheckReturn>;
};

/************************************************************************************************
    QcJSTargetNativeTypeMapper
    Mapping C++ parameter signatures to JS-to-Native target types

    1. Value Type (T):
        - Maps directly to pure value type: JS Target Type -> T (e.g., int -> int, std::string -> std::string).

    2. Rvalue Reference (T&&):
        - Decay-maps to underlying value type: JS Target Type -> T (e.g., std::string&& -> std::string).

    3. Lvalue Reference (T&):
        - Exported Class: Maps to native pointer: JS Target Type -> T* (enables in-place mutation of JS instances).
        - Other Types (Primitives / Non-Exported): Decay-maps to value type: JS Target Type -> T.

    4. Pointer Type (T*):
        - Exported Class: Preserves pointer semantics: JS Target Type -> T*.
        - Other Types (Primitives / Non-Exported): Decay-maps to value type: JS Target Type -> T.

    5. Explicit Wrapper Type (QcArg<T>):
        - QcArg<T*> (Exported Class): Maps to native class pointer: JS Target Type -> T*.
        - QcArg<T> (Value Type): Maps to underlying value type: JS Target Type -> T.

    Specializations:
        - C-Style String Pointers (char*, const char*, wchar_t*, const wchar_t*) automatically map to
            std::string or std::wstring JS target containers.
************************************************************************************************ */

// QcArgPointerTargetType
template <typename T>
struct QcArgPointerTargetType {
    using type = typename std::conditional<
        is_exported_class_v<pure_type_t<T>>,
        pure_type_t<T>*, // exported-class：T*
        pure_type_t<T>   // non-exported-class：T
    >::type;
};

// Default Error: T -> T
template<typename T, typename Enable = void>
struct QcJSTargetNativeTypeMapper {
	QJS_STATIC_ASSERT_M(always_false_v<T>, 
		"Unsupported parameter type detected");
	static constexpr const char* type_desc = "default mapper for T";
	using js_target_type = pure_type_t<T>;
	using arg_type = QcArg<js_target_type>;
};

// T -> T: const int -> int, std::string -> std::string
template<typename T>
struct QcJSTargetNativeTypeMapper<T, 
	typename std::enable_if<  !std::is_pointer<T>::value && !std::is_reference<T>::value && has_js_to_native_v<T> >::type
> {
	static constexpr const char* type_desc = "mapper for T (value type) with js_to_native";
	using js_target_type = pure_type_t<T>;
	using arg_type = QcArg<js_target_type>;
};

// T&& -> T: const int&& -> int, const std::string&& -> std::string
template<typename T>
struct QcJSTargetNativeTypeMapper<T&&,	
	typename std::enable_if<has_js_to_native_v<pure_type_t<T>>>::type
> {
	static constexpr const char* type_desc = "mapper for T&& with js_to_native";
	using js_target_type = pure_type_t<T>;
	using arg_type = QcArg<js_target_type>;
};

// T* (ExportClass) -> T*
template<typename T>
struct QcJSTargetNativeTypeMapper<T*, 
	typename std::enable_if<is_exported_class_v<T>>::type> {
	static constexpr const char* type_desc = "mapper for export-class T*";
	using js_target_type = pure_type_t<T>*;
	using arg_type = QcArg<js_target_type>;
};

// T*  (NonExportClass) -> T
template<typename T>
struct QcJSTargetNativeTypeMapper<T*, 
	typename std::enable_if<!is_exported_class_v<T>>::type> {
	static constexpr const char* type_desc = "mapper for non-export-class T*";
	using js_target_type = pure_type_t<T>;
	using arg_type = QcArg<js_target_type>;
};

// T& (ExportClass) -> T*
template<typename T>
struct QcJSTargetNativeTypeMapper<T&, 
	typename std::enable_if<is_exported_class_v<T>>::type> {
	static constexpr const char* type_desc = "mapper for export-class T&";
	using js_target_type = pure_type_t<T>*;
	using arg_type = QcArg<js_target_type>;
};

// T& (NonExportClass) -> T
template<typename T>
struct QcJSTargetNativeTypeMapper<T&, 
	typename std::enable_if<!is_exported_class_v<T>>::type> {
	static constexpr const char* type_desc = "mapper for non-export-class T&";
	using js_target_type = pure_type_t<T>;
	using arg_type = QcArg<js_target_type>;
};

// QcArg<T*> -> T* (ExportClass) / T (NonExportClass)
template <typename T>
struct QcJSTargetNativeTypeMapper<QcArg<T*>,typename std::enable_if< std::is_same<T, pure_type_t<T>>::value>::type> {
	static constexpr const char* type_desc = "mapper for QcArg<T*>";
	using js_target_type = typename QcArgPointerTargetType<T>::type;
	using arg_type = QcArg<js_target_type>;
};

// QcArg<T> -> T
template <typename T>
struct QcJSTargetNativeTypeMapper<QcArg<T>,
	typename std::enable_if< 
		!std::is_pointer<T>::value && std::is_same<T, pure_type_t<T>>::value && has_js_to_native_v<T>
	>::type
> {
	static constexpr const char* type_desc = "mapper for QcArg<T> of value type";
	using js_target_type = pure_type_t<T>;
	using arg_type = QcArg<js_target_type>;
};

// const char* -> std::string
template<> struct QcJSTargetNativeTypeMapper<const char*> {
	static constexpr const char* type_desc = "mapper for const char*";
	using js_target_type = std::string;
	using arg_type = QcArg<js_target_type>;
};

// char* -> std::string
template<> struct QcJSTargetNativeTypeMapper<char*> {
	static constexpr const char* type_desc = "mapper for char *";
	using js_target_type = std::string;
	using arg_type = QcArg<js_target_type>;
};

// const wchar_t* -> std::wstring
template<> struct QcJSTargetNativeTypeMapper<const wchar_t*> {
	static constexpr const char* type_desc = "mapper for const wchar_t*";
	using js_target_type = std::wstring;
	using arg_type = QcArg<js_target_type>;
};

// wchar_t* -> std::wstring
template<> struct QcJSTargetNativeTypeMapper<wchar_t*> {
	static constexpr const char* type_desc = "mapper for wchar_t";
	using js_target_type = std::wstring;
	using arg_type = QcArg<js_target_type>;
};


/****************************************************************************
ConvertJsArgToNative: Convert JSValue to C++ function argument

Data Flow:
JSValue -> JSTargetType -> QcArg -> (Implicit Cast) -> ArgType

Pipeline Steps:
1. Type Normalization (QcJSTargetNativeTypeMapper):
	- int / int& / const int* / QcArg -> int
	- const char* / std::string&           -> std::string
	- MyClass* / MyClass& / QcArg<MyClass*> -> MyClass*

2. JS Parsing (QcJSValueToNative):
	- Extracts JSValue into JSTargetType (primitive, string, or opaque obj pointer).

3. Contextual Wrapper (QcArg):
	- Stores (JSContext*, JSValueConst, JSTargetType native_val).

4. Argument Delivery (Implicit Cast):
	-QcArg      -> int / int& / const int& / int*
	- QcArg<MyClass*> -> MyClass* / const MyClass* / QcArg<MyClass*>
***************************************************************************** */

template<typename ArgType>
typename QcJSTargetNativeTypeMapper<ArgType>::arg_type
ConvertJsArgToNative(JSContext* ctx, JSValueConst val, size_t arg_idx) {
	// JSValue ─> JSTargetType ─> QcArg<JSTargetType>
	using MapperType = QcJSTargetNativeTypeMapper<ArgType>;
	using JSTargetType = typename MapperType::js_target_type;
	using QcArgType = typename MapperType::arg_type;
	QJS_STATIC_ASSERT_M(has_js_to_native_v<JSTargetType>, "JSTargetType must supports "
		"QcJSValueToNative<JSTargetType>");

#ifdef QJS_DEBUG
	auto debugArgType = type_name<ArgType>();
	auto debugMapperType = type_name<MapperType>();
	auto debugMapperDesc = MapperType::type_desc;
	auto debugJSTargetType = type_name<JSTargetType>();
	auto debugQcArgType = type_name<QcArgType>();
#endif

	JSTargetType native_val = QcJSValueToNative<JSTargetType>::FromJs(ctx, val);
	return QcArg<JSTargetType>(ctx, val, std::move(native_val));
}

// ==============================================
// QcNativeFuncStorage
// Stores function pointers, including global functions and member functions.
// ==============================================
struct QcNativeFuncStorage {
	static constexpr int kFuncPoniterMaxSize = 32;
	uint8_t buffer[kFuncPoniterMaxSize];
	size_t size{ 0 };

	template<typename T>
	void Store(const T& val) {
		QJS_STATIC_ASSERT_M(sizeof(T) <= kFuncPoniterMaxSize, "Function pointer size exceeds storage capacity!");
		QJS_STATIC_ASSERT_M(std::is_trivially_copyable<T>::value, "Stored function object must be trivially copyable!");
		size = sizeof(T);
		std::memcpy(buffer, &val, size);
	}

	template<typename T>
	void Read(T& val) const {
		QJS_STATIC_ASSERT_M(sizeof(T) <= kFuncPoniterMaxSize, "Function pointer size exceeds storage capacity!");
		QJS_STATIC_ASSERT_M(std::is_trivially_copyable<T>::value, "Stored function object must be trivially copyable!");
		std::memcpy(&val, buffer, (std::min)(size, sizeof(T)));
	}
};

// ====================================
// WrapReturnJSValue 
// Wrap the return value as JSValue
// ====================================
template <QcReturnPolicy Policy, typename Ret>
typename std::enable_if<!std::is_pointer<Ret>::value, JSValue>::type
WrapReturnJSValue(JSContext* ctx, Ret&& res, JSValueConst this_val = JS_UNDEFINED) {
    using RetPureType = pure_type_t<Ret>;
	return QcNativeToJSValue<RetPureType>::ToJs(ctx, std::forward<Ret>(res));
}

template <QcReturnPolicy Policy, typename Ret>
typename std::enable_if<std::is_pointer<Ret>::value, JSValue>::type
WrapReturnJSValue(JSContext* ctx, Ret ptr, JSValueConst this_val = JS_UNDEFINED) {
    using ClassType = pure_type_t<Ret>;
	QJS_STATIC_ASSERT_M(is_exported_class_v<ClassType>, "Return pointer type must be exported class");
	if (!ptr) {
		return JS_NULL;
	}

	try {
        switch (Policy) {
            case QcReturnPolicy::NewOwnedByJS:
                return QcOpaque::WrapNewOwnedByJS<ClassType>(ctx, ptr);
            case QcReturnPolicy::OwnedByNative:
                return QcOpaque::WrapOwnedByNative<ClassType>(ctx, ptr);
            default:
                return JS_ThrowTypeError(ctx, "Invalid ReturnPolicy for raw pointer");
        }
	} catch (const std::exception&) {
		return JS_ThrowTypeError(ctx, "An error occurred when wrapping raw pointer as a JSValue");
	} catch (...) {
		return JS_ThrowTypeError(ctx, "Unknown error occurred when wrapping raw pointer as a JSValue.");
	}
}

// ====================================================
// QcNativeStaticFuncInvoker
// Responsible for binding C++ global/static functions:
// 1. Pack function pointer into JS closure via JS_NewCFunctionData.
// 2. Unpack JS arguments using ConvertJsArgToNative.
// 3. Invoke static function and wrap return value into JSValue.
// ====================================================

class QcNativeStaticFuncInvoker {
public:
	template<QcReturnPolicy Policy = QcReturnPolicy::Default, typename Ret, typename... Args>
	static JSValue Bind(JSContext* ctx, Ret(*func)(Args...), const char* name = nullptr) {
		QJS_STATIC_ASSERT_M(sizeof(func) <= QcNativeFuncStorage::kFuncPoniterMaxSize, 
			"Function pointer size exceeds 32 bytes");
        using CheckReturn = typename QcReturnTypeChecker<Policy, Ret>::type;
        using CheckPrams = typename QcParamsTypeChecker<Args...>::type;

		QcNativeFuncStorage storage;
		storage.Store(func);

#ifdef QJS_DEBUG
		if (name) {
			int64_t debugFuncHashId = 0;
			std::memcpy(&debugFuncHashId, &func, (std::min)(sizeof(debugFuncHashId), sizeof(func)));
			QcDebugSymbolRegistry::GetInstance().RegisterSymbol(std::to_string(debugFuncHashId), name);
		}
#endif

		JSValue data = JS_NewArrayBufferCopy(ctx, (const uint8_t*)&storage, sizeof(storage));
		JSValue func_val = JS_NewCFunctionData(ctx, &StaticFunctionInvoker<Policy, Ret, Args...>, sizeof...(Args), 0, 1, &data);
		JS_FreeValue(ctx, data);

		return func_val;
	}

private:
	template<QcReturnPolicy Policy = QcReturnPolicy::Default, typename Ret, typename... Args>
	static JSValue StaticFunctionInvoker(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic, JSValue* func_data) {
		// Check the number of parameters
		constexpr int expected_argc = static_cast<int>(sizeof...(Args));
		if (argc < expected_argc) {
			return JS_ThrowTypeError(ctx, "Wrong number of args while calling function (expected %d, got %d)",
				expected_argc, argc);
		}

		// Get function pointer from func_data
		size_t func_buffer_size = 0;
		uint8_t* func_buffer = JS_GetArrayBuffer(ctx, &func_buffer_size, func_data[0]);
		if (func_buffer == nullptr || func_buffer_size != sizeof(QcNativeFuncStorage)) {
			return JS_ThrowTypeError(ctx, "Invalid Function Pointer Storage");
		}

		using FuncPtr = Ret(*)(Args...);
		FuncPtr func = nullptr;
		const QcNativeFuncStorage* storage = reinterpret_cast<const QcNativeFuncStorage*>(func_buffer);
		storage->Read(func);
		if (func == nullptr) {
			return JS_ThrowTypeError(ctx, "Invalid Function Pointer");
		}

#ifdef QJS_DEBUG
		int64_t debugFuncHashId = 0;
		std::memcpy(&debugFuncHashId, &func, (std::min)(sizeof(debugFuncHashId), sizeof(func)));
		auto debugFuncName = QcDebugSymbolRegistry::GetSymbolName(std::to_string(debugFuncHashId));
#endif

		// Call function
		return StaticInvokerHelper<Policy, Ret, Args...>(ctx, this_val, func, argv, std::make_index_sequence<sizeof...(Args)>{});
	}

	template<QcReturnPolicy Policy, typename Ret, typename... Args, size_t... Is>
	static JSValue StaticInvokerHelper(JSContext* ctx, JSValueConst this_val, Ret(*func)(Args...), JSValueConst* argv, std::index_sequence<Is...>) {
#ifdef QJS_DEBUG
		auto debugParamTypes = type_name<Args...>();
		auto debugJSTargetTypes = type_name<typename QcJSTargetNativeTypeMapper<Args>::js_target_type...>();
		auto debugQcArgTypes = type_name<decltype(ConvertJsArgToNative<Args>(ctx, argv[Is], Is))...>();
#endif

        try {
            // Unpack arguments and call the function.
			return CallStaticAndReturn<Policy, Ret, Args...>(
				ctx, this_val, func, ConvertJsArgToNative<Args>(ctx, argv[Is], Is)...);
        } catch (const std::exception& e) {
            return JS_ThrowTypeError(ctx, "C++ Exception: %s", e.what());
        } catch (...) {
            return JS_ThrowTypeError(ctx, "Unknown C++ native exception occurred during function execution");
        }
	}

	template<QcReturnPolicy Policy, typename Ret, typename... Args, typename... QcArgs>
	static typename std::enable_if<!std::is_same<Ret, void>::value, JSValue>::type
		CallStaticAndReturn(JSContext* ctx, JSValueConst this_val, Ret(*func)(Args...), QcArgs&&... qc_args) {
		Ret res = func(static_cast<Args>(std::forward<QcArgs>(qc_args))...);
		return WrapReturnJSValue<Policy, Ret>(ctx, std::move(res), this_val);
	}

	template<QcReturnPolicy Policy, typename Ret, typename... Args, typename... QcArgs>
	static typename std::enable_if<std::is_same<Ret, void>::value, JSValue>::type
		CallStaticAndReturn(JSContext* ctx, JSValueConst this_val, void(*func)(Args...), QcArgs&&... qc_args) {
		func(static_cast<Args>(std::forward<QcArgs>(qc_args))...);
		return JS_UNDEFINED;
	}
};

// ====================================================
// QcNativeMemberFuncInvoker
// Responsible for binding C++ class member functions:
// 1. Pack member function pointer into JS closure via JS_NewCFunctionData.
// 2. Retrieve C++ instance pointer from JS `this` context.
// 3. Unpack JS arguments using ConvertJsArgToNative.
// 4. Invoke member function on instance and wrap return value into JSValue.
// ====================================================

class QcNativeMemberFuncInvoker {
public:
	template<QcReturnPolicy Policy = QcReturnPolicy::Default, typename T, typename Ret, typename... Args>
	static JSValue Bind(JSContext* ctx, Ret(T::* method)(Args...), const char* name = nullptr) {
		return BindImpl<Policy, T, Ret(T::*)(Args...), Ret, Args...>(ctx, method, name);
	}
	
	// const
	template<QcReturnPolicy Policy = QcReturnPolicy::Default, typename T, typename Ret, typename... Args>
	static JSValue Bind(JSContext* ctx, Ret(T::* method)(Args...) const, const char* name = nullptr) {
		using ConstMethodPtr = Ret(T::*)(Args...) const;
		return BindImpl<Policy, T, ConstMethodPtr, Ret, Args...>(ctx, method, name);
	}

private:
	template<QcReturnPolicy Policy, typename T, typename MethodPtr, typename Ret, typename... Args>
	static JSValue BindImpl(JSContext* ctx, MethodPtr method, const char* name) {
		QJS_STATIC_ASSERT_M(sizeof(method) <= QcNativeFuncStorage::kFuncPoniterMaxSize, 
			"Member function pointer size exceeds 32 bytes");
        using CheckReturn = typename QcReturnTypeChecker<Policy, Ret>::type;
        using CheckPrams = typename QcParamsTypeChecker<Args...>::type;

		QcNativeFuncStorage storage;
		storage.Store(method);

#ifdef QJS_DEBUG
		if (name) {
			int64_t debugFuncHashId = 0;
			std::memcpy(&debugFuncHashId, &method, (std::min)(sizeof(debugFuncHashId), sizeof(method)));
			QcDebugSymbolRegistry::GetInstance().RegisterSymbol(std::to_string(debugFuncHashId), name);
		}
#endif

		JSValue data = JS_NewArrayBufferCopy(ctx, (const uint8_t*)&storage, sizeof(storage));
		JSValue func_val = JS_NewCFunctionData(ctx, &MethodInvoker<Policy, T, MethodPtr, Ret, Args...>, sizeof...(Args), 0, 1, &data);
		JS_FreeValue(ctx, data);

		return func_val;
	}

private:
	template<QcReturnPolicy Policy, typename T, typename MethodPtr, typename Ret, typename... Args>
	static JSValue MethodInvoker(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic, JSValue* func_data) {
		// Check the number of parameters
		constexpr int expected_argc = static_cast<int>(sizeof...(Args));
		if (argc < expected_argc) {
			return JS_ThrowTypeError(ctx, "Wrong number of args while calling method (expected %d, got %d)",
				expected_argc, argc);
		}

		// Get method pointer from func_data
		size_t func_buffer_size = 0;
		uint8_t* func_buffer = JS_GetArrayBuffer(ctx, &func_buffer_size, func_data[0]);
		if (func_buffer == nullptr || func_buffer_size != sizeof(QcNativeFuncStorage)) {
			return JS_ThrowTypeError(ctx, "Invalid Member Function Pointer Storage");
		}

		const QcNativeFuncStorage* storage = reinterpret_cast<const QcNativeFuncStorage*>(func_buffer);
		MethodPtr method = nullptr;
		storage->Read(method);
		if (method == nullptr) {
			return JS_ThrowTypeError(ctx, "Invalid Member Function Pointer");
		}

#ifdef QJS_DEBUG
		int64_t debugFuncHashId = 0;
		std::memcpy(&debugFuncHashId, &method, (std::min)(sizeof(debugFuncHashId), sizeof(method)));
		auto debugFuncName = QcDebugSymbolRegistry::GetSymbolName(std::to_string(debugFuncHashId));
		auto debugClassName = type_name<T>();
#endif

		// Get the this pointer
		T* this_ptr = (T*)QcOpaque::GetOpaquePtr<T>(ctx, this_val);
		if (!this_ptr) {
#ifdef QJS_DEBUG
			std::string err = format_str("The <this> is nullptr when calling member function: %s::%s()", 
				debugClassName.c_str(), debugFuncName.c_str());
			return JS_ThrowTypeError(ctx, err.c_str());
#else
			return JS_ThrowTypeError(ctx, "The <this> is nullptr when calling member function");
#endif
		}

		// Call method
		return MethodInvokerHelper<Policy, T, MethodPtr, Ret, Args...>(ctx, this_val, this_ptr, method, argv, std::make_index_sequence<sizeof...(Args)>{});
	}

	template<QcReturnPolicy Policy, typename T, typename MethodPtr, typename Ret, typename... Args, size_t... Is>
	static JSValue MethodInvokerHelper(JSContext* ctx, JSValueConst this_val, T* this_ptr, MethodPtr method, JSValueConst* argv, std::index_sequence<Is...>) {
#ifdef QJS_DEBUG
		auto debugClassName = type_name<T>();
		auto debugParamTypes = type_name<Args...>();
		auto debugJSTargetTypes = type_name<typename QcJSTargetNativeTypeMapper<Args>::js_target_type...>();
		auto debugQcArgTypes = type_name<decltype(ConvertJsArgToNative<Args>(ctx, argv[Is], Is))...>();
#endif

		try {
			// Unpack arguments and call the method.
			return CallMethodAndReturn<Policy, T, MethodPtr, Ret, Args...>(
				ctx, this_val, this_ptr, method, ConvertJsArgToNative<Args>(ctx, argv[Is], Is)...);
        } catch (const std::exception& e) {
            return JS_ThrowTypeError(ctx, "C++ Exception: %s", e.what());
        } catch (...) {
            return JS_ThrowTypeError(ctx, "Unknown C++ native exception occurred during method execution");
        }
	}

	template<QcReturnPolicy Policy, typename T, typename MethodPtr, typename Ret, typename... Args, typename... QcArgs>
	static typename std::enable_if<!std::is_same<Ret, void>::value, JSValue>::type
		CallMethodAndReturn(JSContext* ctx, JSValueConst this_val, T* this_ptr, MethodPtr method, QcArgs&&... qc_args) {
		Ret res = (this_ptr->*method)(static_cast<Args>(std::forward<QcArgs>(qc_args))...);
		return WrapReturnJSValue<Policy, Ret>(ctx, std::move(res), this_val);
	}

	template<QcReturnPolicy Policy, typename T, typename MethodPtr, typename Ret, typename... Args, typename... QcArgs>
	static typename std::enable_if<std::is_same<Ret, void>::value, JSValue>::type
		CallMethodAndReturn(JSContext* ctx, JSValueConst this_val, T* this_ptr, MethodPtr method, QcArgs&&... qc_args) {
		(this_ptr->*method)(static_cast<Args>(std::forward<QcArgs>(qc_args))...);
		return JS_UNDEFINED;
	}
};

// ====================================================
// QcNativeConstructorInvoker
// Responsible for automatically deriving C++ constructor parameters:
// 1. Unpack JS arguments using ConvertJsArgToNative.
// 2. Instantiate C++ object using `new T(args...)`.
// 3. Wrap C++ instance as JSValue owned by JS via QcOpaque.
// ====================================================

class QcNativeConstructorInvoker {
public:
	template<typename T, typename... Args>
	static JSValue Bind(JSContext* ctx, const char* className) {
		using CheckPrams = typename QcParamsTypeChecker<Args...>::type;
		if (className == nullptr) {
			return JS_UNDEFINED;
		}

		return JS_NewCFunction2( ctx, &ConstructorInvoker<T, Args...>, className, sizeof...(Args),
			JS_CFUNC_constructor, 0	);
	}

private:
	template<typename T, typename... Args>
	static JSValue ConstructorInvoker(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
		// Check the number of parameters
		constexpr int expected_argc = static_cast<int>(sizeof...(Args));
		if (argc < expected_argc) {
			return JS_ThrowTypeError(ctx, "Wrong number of args while calling constructor (expected %d, got %d)",
				expected_argc, argc);
		}

		// Call constructor
		return ConstructorInvokerHelper<T, Args...>(ctx, argv, std::make_index_sequence<sizeof...(Args)>{});
	}

	template<typename T, typename... Args, size_t... Is>
	static JSValue ConstructorInvokerHelper(JSContext* ctx, JSValueConst* argv, std::index_sequence<Is...>) {
#ifdef QJS_DEBUG
		auto debugClassName = type_name<T>();
		auto debugParamTypes = type_name<Args...>();
		auto debugJSTargetTypes = type_name<typename QcJSTargetNativeTypeMapper<Args>::js_target_type...>();
		auto debugQcArgTypes = type_name<decltype(ConvertJsArgToNative<Args>(ctx, argv[Is], Is))...>();
#endif

        try {
            // Unpack arguments and call the constructor.
			return CallConstructorAndReturn<T, Args...>(ctx, ConvertJsArgToNative<Args>(ctx, argv[Is], Is)...);
        } catch (const std::exception& e) {
            return JS_ThrowTypeError(ctx, "C++ Exception: %s", e.what());
        } catch (...) {
            return JS_ThrowTypeError(ctx, "Unknown C++ native exception occurred during constructor execution");
        }
	}

	template<typename T, typename... Args, typename... QcArgs>
	static JSValue CallConstructorAndReturn(JSContext* ctx, QcArgs&&... qc_args) {
		T* native_obj = new T(static_cast<Args>(std::forward<QcArgs>(qc_args))...);
		return QcOpaque::WrapNewOwnedByJS<T>(ctx, native_obj); // Owned by JS
	}
};

NAMESPACE_QJS_END