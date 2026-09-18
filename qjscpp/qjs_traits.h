#pragma once
#include <type_traits>
#include "quickjs.h"
#include "qjs_global.h"

NAMESPACE_QJS_BEGIN

// void_ty
template <typename... Ts>
using void_ty = void;

// always_false_v
template <typename T>
static constexpr bool always_false_v = false;

// pure_type: const int* const* / int& -> int
template <typename T>
struct pure_type {
    using type = typename std::remove_cv<T>::type;
};

template <typename T>
struct pure_type<T*> {
    using type = typename pure_type<typename std::remove_cv<T>::type>::type;
};

template <typename T>
struct pure_type<T&> {
    using type = typename pure_type<typename std::remove_cv<T>::type>::type;
};

template <typename T>
struct pure_type<T&&> {
    using type = typename pure_type<typename std::remove_cv<T>::type>::type;
};

template<typename T>
using pure_type_t = typename pure_type<typename std::remove_cv<T>::type>::type;

// dump_type_error
template<typename T>
struct dump_type_error;

// ============================
// mem_fn_traits
// ============================
template<typename T>
struct mem_fn_traits {
	using return_type = void; 
	static constexpr bool is_valid = false;
};

// mem_fn_traits: R(T*)(Args...)
template<typename ClassType, typename ReturnType, typename... Args>
struct mem_fn_traits<ReturnType(ClassType::*)(Args...)> {
	using return_type = ReturnType;
	using class_type = ClassType;
	static constexpr bool is_valid = true;
};

// mem_fn_traits: R(T*)(Args...) const
template<typename ClassType, typename ReturnType, typename... Args>
struct mem_fn_traits<ReturnType(ClassType::*)(Args...) const> {
	using return_type = ReturnType;
	using class_type = ClassType;
	static constexpr bool is_valid = true;
};

#if QJS_CXX >= QJS_CXX17
// R(T*)(Args...) noexcept
template<typename ClassType, typename ReturnType, typename... Args>
struct mem_fn_traits<ReturnType(ClassType::*)(Args...) noexcept> {
    using return_type = ReturnType;
    using class_type = ClassType;
    static constexpr bool is_valid = true;
};

// R(T*)(Args...) const noexcept
template<typename ClassType, typename ReturnType, typename... Args>
struct mem_fn_traits<ReturnType(ClassType::*)(Args...) const noexcept> {
    using return_type = ReturnType;
    using class_type = ClassType;
    static constexpr bool is_valid = true;
};
#endif

// is_double_pointer
template <typename T>
struct is_double_pointer {
	static constexpr bool value = std::is_pointer<T>::value &&
		std::is_pointer<typename std::remove_pointer<T>::type>::value;
};

template <typename T>
static constexpr bool is_double_pointer_v = is_double_pointer<T>::value;

// ========================
// is_virtual_base_of
// ========================
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

// ========================
// is_exported_class
// ========================
// QcClass declaration
template <typename T>
class QcClass;

// is_exported_class_v
template <typename T>
constexpr bool is_exported_class_v = QcClass<pure_type_t<T>>::is_exported;

// ===========================================
// Check if JSValue can be converted to T: JSValue -> T
// QcJSValueToNative<T>::FromJs has already been implemented 
// ===========================================
// QcNativeToJSValue declaration
template <typename T, typename Enable = void>
struct QcNativeToJSValue;

// QcJSValueToNative declaration
template <typename T, typename Enable = void>
struct QcJSValueToNative;

template <typename T, typename = void>
struct has_js_to_native : std::false_type {};

template <typename T>
struct has_js_to_native<T,
	void_ty<decltype(QcJSValueToNative<T>::FromJs(std::declval<JSContext*>(), std::declval<JSValueConst>()))>
	> : std::true_type {
};

template <typename T>
constexpr bool has_js_to_native_v = has_js_to_native<T>::value;

// ===========================================
// Check if JSValue can be converted to T: JSValue <- T
// QcNativeToJSValue<T>::ToJs has already been implemented 
// ===========================================
template <typename T, typename = void>
struct has_native_to_js : std::false_type {};

template <typename T>
struct has_native_to_js<T,
	void_ty<decltype(QcNativeToJSValue<T>::ToJs(std::declval<JSContext*>(), std::declval<const T&>()))>
> : std::true_type {
};

template <typename T>
constexpr bool has_native_to_js_v = has_native_to_js<T>::value;


template <typename T>
constexpr bool has_js_native_converter_v = has_native_to_js_v<T> && has_js_to_native_v<T>;


// QcArg declaration
template <typename T, typename Enable = void>
class QcArg;

// is_qc_arg
template <typename T> 
struct is_qc_arg : std::false_type {};

template <typename T> 
struct is_qc_arg<QcArg<T>> : std::true_type {};

template <typename T> 
constexpr bool is_qc_arg_v = is_qc_arg<T>::value;

// QcReturnPolicy
enum class QcReturnPolicy {
    Default,					// Default means not ptr
    OwnedByNative,		// Object is owned by Native C++
    NewOwnedByJS,		// New object which owned by JS
};

NAMESPACE_QJS_END