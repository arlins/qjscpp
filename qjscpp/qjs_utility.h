#pragma once
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <sstream>
#include <memory>
#include <utility>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <codecvt>
#include <locale>
#include <regex>
#include <chrono>

#include "qjs_global.h"

#if defined(QJS_OS_WIN)
#include <windows.h>
#elif defined(QJS_OS_DARWIN)
#include <os/log.h>
#endif

#if defined(QJS_COMPILER_GCC) || defined(QJS_COMPILER_CLANG)
#include <cxxabi.h>
#endif


NAMESPACE_QJS_BEGIN

// ==============================
// String operations
// ==============================
template<typename ... Args>
QJS_FORCE_INLINE std::string format_str(const char* format, Args&&... args) {
	int size = std::snprintf(nullptr, 0, format, std::forward<Args>(args) ...); // Calc size
	if (size < 0) {
		return std::string(); // Failed
	}

	std::vector<char> buf(size + 1); // Extra space for '\0'
	std::snprintf(buf.data(), buf.size(), format, std::forward<Args>(args) ...);
	return std::string(buf.data(), size); // We don't want the '\0' inside
}

// Character formatting
// std::wstring str = format_str(L"s=%ls", L"a");
template<typename ... Args>
QJS_FORCE_INLINE std::wstring format_str(const wchar_t* format, Args&& ... args) {
	int size = std::swprintf(nullptr, 0, format, std::forward<Args>(args) ...); // Calc size
	if (size < 0) {
		return std::wstring(); // Failed
	}

	std::vector<wchar_t> buf(size + 1); // Extra space for '\0'
	std::swprintf(buf.data(), buf.size(), format, std::forward<Args>(args) ...);
	return std::wstring(buf.data(), size); // We don't want the '\0' inside
}

// String replacement
QJS_FORCE_INLINE std::string str_replace(const std::string& _str, const std::string& from, const std::string& to) {
	if (from.empty()) {
		return _str;
	}

	size_t pos = 0;
	std::string str = _str;

	while ((pos = str.find(from, pos)) != std::string::npos) {
		str.replace(pos, from.length(), to);
		pos += to.length();
	}

	return str;
}

// WString replacement
QJS_FORCE_INLINE std::wstring str_replace(const std::wstring& _str, const std::wstring& from, const std::wstring& to) {
	if (from.empty()) {
		return _str;
	}

	size_t pos = 0;
	std::wstring str = _str;

	while ((pos = str.find(from, pos)) != std::wstring::npos) {
		str.replace(pos, from.length(), to);
		pos += to.length();
	}

	return str;
}

QJS_FORCE_INLINE std::string str_regex_replace(const std::string& src, const std::string& pattern, const std::string& replacement) {
	try {
		std::regex reg(pattern, std::regex::ECMAScript | std::regex::optimize);
		return std::regex_replace(src, reg, replacement);
	} catch (const std::regex_error& e) {
		return src;
	}
}

// String to wstring
QJS_FORCE_INLINE std::wstring str2wstr(const std::string& str) {
	try {
		std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
		return converter.from_bytes(str);
	} catch (...) {
		return L""; // Failed
	}
}

// WString to string
QJS_FORCE_INLINE std::string wstr2str(const std::wstring& wstr) {
	try {
		std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
		std::string utf8_str = converter.to_bytes(wstr);
		return utf8_str;
	} catch (...) {
		return ""; // Failed
	}
}

// ==============================
// Log
// ==============================
// Print console log
QJS_FORCE_INLINE void console_log_str(const char* str) {
#ifdef QJS_OS_WIN
	std::wstring wstr = str2wstr(std::string(str));
	::OutputDebugStringW(wstr.c_str());
#elif defined(QJS_OS_DARWIN)
	os_log(OS_LOG_DEFAULT, "%s", str);
#else
	fprintf(stderr, "%s", str);
#endif
}

template<typename ... Args>
QJS_FORCE_INLINE void console_log(const char* format, Args&&... args) {
	std::string s = format_str(format, std::forward<Args>(args)...);
	console_log_str(s.c_str());
}

#if defined(QJS_COMPILER_MSVC) // MSVC
template <typename T>
QJS_FORCE_INLINE std::string type_name_for_msvc() {
	std::string str = __FUNCSIG__;
	size_t start = str.find("type_name_for_msvc<");
	size_t end = str.rfind(">(void)");

	if (start != std::string::npos && end != std::string::npos) {
		start += sizeof("type_name_for_msvc<") - 1;
		if (end > start) {
			str = str.substr(start, end - start);
		}
	} else {
		str = typeid(T).name();
	}

	return str;
}
#endif

// ==============================
// type_name: Obtain the type string
// ==============================
// type_name_internal
template <typename T>
QJS_FORCE_INLINE std::string type_name_internal() {

#if defined(QJS_COMPILER_MSVC) // MSVC
	//return std::string(typeid(T).name()); // MSVC will remove the ref and cv of type
	return type_name_for_msvc<T>();
#elif defined(QJS_COMPILER_GCC) || defined(QJS_COMPILER_CLANG) // GCC/Clang
	int status;
	const char* tname = typeid(T).name();
	char* demangled = abi::__cxa_demangle(tname, nullptr, nullptr, &status);
	std::string name = (status == 0) ? demangled : tname;
	free(demangled);
	return name;
#elif defined(QJS_COMPILER_MINGW) //MinGW
	return std::string(typeid(T).name());
#else
	return std::string(typeid(T).name());
#endif

}

// type_name_impl
template<typename... Types>
struct type_name_impl;

template<>
struct type_name_impl<> {
	static std::string str() { return ""; }
};

QJS_FORCE_INLINE void make_type_name_clear(std::string& s) {
	std::string patternStdString = R"((class\s+|struct\s+)?std::basic_string<char,\s*struct std::char_traits<char>,\s*class std::allocator<char>\s*>)";
	std::string replacementStdString = "std::string";

	s = str_regex_replace(s, patternStdString, replacementStdString);
}

template<typename T, typename... Types>
struct type_name_impl<T, Types...> {
	static std::string str() {
		std::ostringstream oss;
		oss << type_name_internal<T>();
		if (sizeof...(Types) > 0) {
			oss << ", " << type_name_impl<Types...>::str();
		}

		std::string s(oss.str());

		return s;
	}
};

// type_name: Obtain the type string
template<typename... Types>
QJS_FORCE_INLINE std::string type_name() {
	auto tn = type_name_impl<Types...>::str();
	make_type_name_clear(tn);
	return tn;
}

// Get now time
template <typename TimeUnit = std::chrono::milliseconds>
inline long long now_time() {
    auto now = std::chrono::high_resolution_clock::now();
    long long time = std::chrono::duration_cast<TimeUnit>(now.time_since_epoch()).count();
    return time;
}

template <typename TimePoint, typename TimeUnit = std::chrono::milliseconds>
inline long long time_to_longlong(TimePoint t) {
    long long time = std::chrono::duration_cast<TimeUnit>(t.time_since_epoch()).count();
    return time;
}

// ==================================
// get_canonical_addr
// ==================================
// get_canonical_addr for polymorph
template <typename T>
QJS_FORCE_INLINE typename std::enable_if<std::is_class<T>::value&& std::is_polymorphic<T>::value, void*>::type
get_canonical_addr(T* ptr) noexcept {
	return dynamic_cast<void*>(ptr);
}

// get_canonical_addr for polymorph  (const) 
template <typename T>
QJS_FORCE_INLINE typename std::enable_if<std::is_class<T>::value&& std::is_polymorphic<T>::value, const void*>::type
get_canonical_addr(const T* ptr) noexcept {
	return dynamic_cast<const void*>(ptr);
}

// get_canonical_addr for non-polymorph
template <typename T>
QJS_FORCE_INLINE typename std::enable_if<!std::is_class<T>::value || !std::is_polymorphic<T>::value, void*>::type
get_canonical_addr(T* ptr) noexcept {
	return static_cast<void*>(ptr);
}

// get_canonical_addr for non-polymorph  (const) 
template <typename T>
QJS_FORCE_INLINE typename std::enable_if<!std::is_class<T>::value || !std::is_polymorphic<T>::value, const void*>::type
get_canonical_addr(const T* ptr) noexcept {
	return static_cast<const void*>(ptr);
}

// This function provides a fake reference and MUST NOT call any methods 
// or access fields of T, as the object is not initialized (UB risk).
template <typename T>
QJS_FORCE_INLINE T& get_dummy_object_ref() {
    alignas(T) static char dummy_storage[sizeof(T)] = { 0 };
    return *reinterpret_cast<T*>(dummy_storage);
}

NAMESPACE_QJS_END
