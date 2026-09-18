#pragma once
#include <type_traits>
#include <string>
#include <utility>
#include <new> // for placement new
#include <stdexcept>
#include "qjs_global.h"
#include "qjs_traits.h"

NAMESPACE_QJS_BEGIN

// ============================================ == == == == =
// QcArg<T> for storaging value type (int, std::string, std::wstring, custom-value-type ...)
// and carries context (ctx, js_value) and supports implicit conversion: T, T&, T&&, T* 
// Supporting type:
// - Base type: int, float, double...
// - Class type: includes any class type which QcJSValueToNative<ClassT> has 
//    already been implemented. Regardless of whether the class type is an exported 
//    class or another type of class.
// 
// Note: Implementing the specialization of `QcJSValueToNative<ClassT>` means that
// JSValue can be converted to  a value of the class type. e.g. std::string, Point...
// ============================================ == == == == =

template <typename T, typename Enable = void>
class QcArg {
	QJS_STATIC_ASSERT_M(!std::is_pointer<T>::value, "T must be a value type");

public:
	using value_type = T;
	JSContext* ctx = nullptr;
	JSValueConst js_value = JS_UNDEFINED;

private:
	// Placement New
	using StorageType = typename std::aligned_storage<sizeof(value_type), alignof(value_type)>::type;
	alignas(alignof(value_type)) StorageType storage;

	value_type& value_ref() noexcept {
		return *reinterpret_cast<value_type*>(&storage);
	}

	const value_type& value_ref() const noexcept {
		return *reinterpret_cast<const value_type*>(&storage);
	}

public:
	QcArg() = delete;

	explicit QcArg(JSContext* _ctx, JSValueConst js_val, value_type&& _val) noexcept(std::is_nothrow_move_constructible<value_type>::value)
		: ctx(_ctx), js_value(js_val) {
		::new (static_cast<void*>(&storage)) value_type(std::move(_val));
	}

	// Copy
	QcArg(const QcArg& other)
		: ctx(other.ctx), js_value(other.js_value) {
		::new (static_cast<void*>(&storage)) value_type(other.value_ref());
	}

	// Copy assign
	QcArg& operator=(const QcArg& other) {
		if (this != &other) {
			ctx = other.ctx;
			js_value = other.js_value;
            value_ref().~value_type();
            ::new (static_cast<void*>(&storage)) value_type(other.value_ref());
		}
		return *this;
	}

	// Move
	QcArg(QcArg&& other) noexcept(std::is_nothrow_move_constructible<value_type>::value)
		: ctx(other.ctx), js_value(other.js_value) {
		::new (static_cast<void*>(&storage)) value_type(std::move(other.value_ref()));
		other.ctx = nullptr;
		other.js_value = JS_UNDEFINED;
	}

	// Move assign
	QcArg& operator=(QcArg&& other) noexcept(std::is_nothrow_move_assignable<value_type>::value) {
		if (this != &other) {
			ctx = other.ctx;
			js_value = other.js_value;
			value_ref().~value_type();
			::new (static_cast<void*>(&storage)) value_type(std::move(other.value_ref()));

			other.ctx = nullptr;
			other.js_value = JS_UNDEFINED;
		}
		return *this;
	}

	~QcArg() {
		value_ref().~value_type();
	}

	value_type& value() noexcept { return value_ref(); }
	const value_type& value() const noexcept { return value_ref(); }

public:
	// Implicit conversion for T&, const T&, T*, const T*
	operator value_type& () noexcept { return value_ref(); }
	operator const value_type& () const noexcept { return value_ref(); }
	operator value_type* () noexcept { return &value_ref(); }
	operator const value_type* () const noexcept { return &value_ref(); }

	// Implicit conversion for T
	template <typename U = value_type, typename std::enable_if<std::is_copy_constructible<U>::value>::type>
	operator value_type () const noexcept { return value_ref(); }

	// Implicit conversion for T&&
	template <typename U = value_type, typename std::enable_if<std::is_copy_constructible<U>::value>::type>
	operator value_type&& () noexcept { return std::move(value_ref()); }

public: // std::string and std::wstring
	// Implicit conversion for std::string to char* and const char*.
	template <typename U = value_type, typename std::enable_if<std::is_same<std::string, U>::value, int>::type = 0>
	operator char* () const noexcept { return const_cast<char*>(value_ref().c_str()); }

	template <typename U = value_type, typename std::enable_if<std::is_same<std::string, U>::value, int>::type = 0>
	operator const char* () const noexcept { return value_ref().c_str(); }

	// Implicit conversion for std::wstring to wchar_t* and const wchar_t*.
	template <typename U = value_type, typename std::enable_if<std::is_same<std::wstring, U>::value, int>::type = 0>
	operator wchar_t* () const noexcept { return const_cast<wchar_t*>(value_ref().c_str()); }

	template <typename U = value_type, typename std::enable_if<std::is_same<std::wstring, U>::value, int>::type = 0>
	operator const wchar_t* () const noexcept { return value_ref().c_str(); }
};


// =========================================
// QcArg<T*> for exported class T*
// Carries context (ctx, js_value, ptr) and supports implicit conversion.
// It is used to handle param types of T&, T* of exported class
// =========================================
template <typename T>
class QcArg<T*> {
public:
	using value_type = T;
	using pointer_type = value_type*;

	JSContext* ctx = nullptr;
	JSValueConst js_value = JS_UNDEFINED;
	pointer_type ptr = nullptr;

public:
	QcArg() = delete;

	QcArg(JSContext* _ctx, JSValueConst js_val, pointer_type _ptr) noexcept
		: ctx(_ctx), js_value(js_val), ptr(_ptr) {
		QJS_STATIC_ASSERT_M(is_exported_class_v<T>, "T must be exported class type");
	}

    // operator -> and *
    pointer_type operator-> () const noexcept { return ptr; }
    pointer_type operator* () const noexcept { return ptr; }

    // Implicit conversion for T*, const T*
    // NOTES: Implicit conversion for T is supported by `QcArg<T>` for any types, 
    // DO NOT provide implicit conversion to T in this class.
    operator pointer_type() noexcept { return ptr; }
    operator const value_type* () noexcept { return ptr; }

	// Implicit conversion for T&
	operator value_type& () const { 
        if (ptr == nullptr) {
#ifdef QJS_DEBUG
            throw std::runtime_error(format_str("QcArg<T*> error: cannot convert null object to <%s&>", 
				type_name<T>().c_str()));
#else
			throw std::runtime_error("QcArg<T*> error: cannot convert null object to <T&>");
#endif
        }
		return *ptr; 
	} 

	// Implicit conversion for const T&
	operator const value_type& () const { 
        if (ptr == nullptr) {
#ifdef QJS_DEBUG
            throw std::runtime_error(format_str("QcArg<T*> error : cannot convert null object to <const %s&>", 
				type_name<T>().c_str()));
#else
            throw std::runtime_error("QcArg<T*> error: cannot convert null object to <const T&>");
#endif
        }
		return *ptr; 
	}
};

NAMESPACE_QJS_END