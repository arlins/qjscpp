#pragma once

// ===============================
// Internal defines
// ===============================

#define NAMESPACE_QJS qjscpp
#define NAMESPACE_QJS_BEGIN namespace NAMESPACE_QJS {
#define NAMESPACE_QJS_END };


// ===============================
// DEBUG
// ===============================

#if !defined(NDEBUG) || defined(_DEBUG) || defined(DEBUG)
#define QJS_DEBUG 1
#endif


// ===============================
// CXX
// ===============================

#define QJS_CXX __cplusplus

#define QJS_CXX20 202002L
#define QJS_CXX17 201703L
#define QJS_CXX14 201402L
#define QJS_CXX11 201103L
#define QJS_CXX98 199711L


// ==================================
// OS
// ==================================

// Windows
#if defined(_WIN32) || defined(__WIN32__) || defined(_WIN64) 
#define QJS_OS_WIN 1 // Win32
#if defined(WINAPI_FAMILY) && (WINAPI_FAMILY != 100 /*WINAPI_FAMILY_DESKTOP_APP*/)
#define QJS_OS_WINRT 1 // WinRT
#endif
// Apple: iOS / macOS / WatchOS / tvOS
#elif defined(__APPLE__) 

#include <TargetConditionals.h> // For TARGET_OS_X
#define QJS_OS_DARWIN 1 // Apple

#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#define QJS_OS_iOS 1 // iOS
#elif defined(TARGET_OS_WATCH) && TARGET_OS_WATCH
#define QJS_OS_WATCHOS 1 // WatchOS
#elif defined(TARGET_OS_TV) && TARGET_OS_TV
#define QJS_OS_TVOS 1 // tvOS
#elif defined(TARGET_OS_OSX) && TARGET_OS_OSX
#define QJS_OS_MACOS 1 // macOS
#endif

#if defined(TARGET_OS_MAC) && TARGET_OS_MAC
#define QJS_OS_DARWIN_NATIVE
#endif

// HarmonyOS / OpenHarmony
#elif defined(__OHOS__) || defined(__OpenHarmony__) 
#define QJS_OS_OHOS 1
// Android
#elif defined(__ANDROID__) || defined(ANDROID) 
#define QJS_OS_ANDROID 1 // Android
#define QJS_OS_LINUX 1 // Linux
// Linux
#elif defined(__linux__) || defined(__linux) 
#define QJS_OS_LINUX 1 // Linux
// Unix-like
#elif defined(__FreeBSD__) || defined(__DragonFly__) || defined(__FreeBSD_kernel__)
#define QJS_OS_FREEBSD 1
#define QJS_OS_BSD 1
#elif defined(__QNXNTO__)
#define QJS_OS_QNX 1
#elif defined(__HAIKU__)
#define QJS_OS_HAIKU 1
#endif

// UNIX / POSIX
#if !defined(QJS_OS_WIN)
#define QJS_OS_UNIX 1
#include <unistd.h> // For _POSIX_VERSION
#if defined(_POSIX_VERSION)
#define QJS_OS_POSIX 1
#endif
#endif


// ==================================
// 32-64 bits
// ==================================

#if defined(_M_X64) || defined(__x86_64__) || defined(__aarch64__) || defined(__LP64__)
#define QJS_BIT_64 1
#elif defined(_M_IX86) || defined(__i386__) || defined(__arm__)
#define QJS_BIT_32 1
#endif

// ==================================
// Arch
// ==================================

// x86
#if defined(_M_IX86) || defined(__i386__)
#define QJS_ARCH_X86 1
#define QJS_ARCH_X86_32 1
#elif defined(_M_X64) || defined(__x86_64__)
#define QJS_ARCH_X86 1
#define QJS_ARCH_X86_64 1
#endif

// ARM
#if defined(_M_ARM) || defined(__arm__)
#define QJS_ARCH_ARM 1
#define QJS_ARCH_ARM_32 1
#elif defined(_M_ARM64) || defined(__aarch64__)
#define QJS_ARCH_ARM 1 //  Apple Silicon (ARM64)
#define QJS_ARCH_ARM_64 1
#endif

// MIPS
#if defined(__mips__)
#define QJS_ARCH_MIPS 1
#endif

// PowerPC
#if defined(__powerpc__) || defined(__ppc__)
#define QJS_ARCH_PPC 1
#endif

// RISC-V
#if defined(__riscv)
#define QJS_ARCH_RISCV 1
#endif

// ======================================
// Compiler
// ======================================

#if defined(_MSC_VER)
#define QJS_COMPILER_MSVC 1
#define QJS_COMPILER_VER _MSC_VER
#elif defined(__clang__)
#define QJS_COMPILER_CLANG 1
#define QJS_COMPILER_VER (__clang_major__ * 100 + __clang_minor__)
#elif defined(__GNUC__)
#define QJS_COMPILER_GCC 1
#define QJS_COMPILER_VER (__GNUC__ * 100 + __GNUC_MINOR__)
#elif defined(__MINGW32__)
#define QJS_COMPILER_MINGW
#define QJS_COMPILER_VER (__GNUC__ * 100 + __GNUC_MINOR__)  
#elif defined(__INTEL_COMPILER)
#define QJS_COMPILER_INTEL 1
#define QJS_COMPILER_VER __INTEL_COMPILER
#endif

// ======================================
// Disable warning
// ======================================

#define QJS_MAKESTR(s) #s
#define QJS_JOINSTR(x,y) QJS_MAKESTR(x ## y)
#define QJS_DOPRAGMA(x) _Pragma(#x)

#if defined(QJS_COMPILER_GCC) // GCC
#define QJS_DISABLE_WARNING_PUSH                QJS_DOPRAGMA(GCC diagnostic push)
#define QJS_DISABLE_WARNING_POP                  QJS_DOPRAGMA(GCC diagnostic pop)
#define QJS_DISABLE_WARNING(warningName)   QJS_DOPRAGMA(GCC diagnostic ignored warningName)

#elif defined(QJS_COMPILER_CLANG) // Clang
#define QJS_DISABLE_WARNING_PUSH                QJS_DOPRAGMA(clang diagnostic push)
#define QJS_DISABLE_WARNING_POP                  QJS_DOPRAGMA(clang diagnostic pop)
#define QJS_DISABLE_WARNING(warningName)   QJS_DOPRAGMA(clang diagnostic ignored warningName)

#elif defined(QJS_COMPILER_MSVC) // MSVC
#define QJS_DISABLE_WARNING_PUSH                 __pragma(warning(push))
#define QJS_DISABLE_WARNING_POP                   __pragma(warning(pop))
#define QJS_DISABLE_WARNING(warningNum)      __pragma(warning(disable: warningNum))
#else
#define QJS_DISABLE_WARNING_PUSH          
#define QJS_DISABLE_WARNING_POP            
#define QJS_DISABLE_WARNING(warning)       
#endif


// ==================================
// Common Tools
// ==================================

// QJS_UNUSED
#define QJS_UNUSED(x) (void)x;

// QJS_FORCE_INLINE
#if defined(QJS_COMPILER_MSVC)
#define QJS_FORCE_INLINE __forceinline
#elif defined(QJS_COMPILER_GCC) || defined(QJS_COMPILER_CLANG)
#define QJS_FORCE_INLINE inline __attribute__((always_inline))
#else
#define QJS_FORCE_INLINE inline
#endif

// The object accompanies the entire application lifecycle 
// and does not need to be released, thus avoiding the problem of uncontrollable 
// timing of static variable destruction when the program exits.
#define QJS_LEAKY_SINGLETON_DEFINE(obj) do{ }while(0);

// QJS_ASSERT
#ifdef QJS_DEBUG
#include <assert.h>
#include <cassert>
#define QJS_ASSERT(x) assert(x)

#if defined(QJS_OS_WIN)
    #include <windows.h>
    #include <crtdbg.h>
    #define QJS_ASSERT_M(cond, msg) \
            do { \
                if (!(cond)) { \
                   if (_CrtDbgReport(_CRT_ASSERT, __FILE__, __LINE__, NULL, "%s", msg) == 1) { \
                        __debugbreak(); \
                    } \
                } \
            } while (false)
    #else
    #define QJS_ASSERT_M(cond, msg) assert((cond) && (msg))
#endif
#else 
#define QJS_ASSERT(x) do {} while (false);
#define QJS_ASSERT_M(cond, msg) do {} while (false);
#endif // QJS_DEBUG

#define QJS_STATIC_ASSERT(cond) static_assert(bool(cond), #cond);
#define QJS_STATIC_ASSERT_M(cond, msg) static_assert(bool(cond), msg);

// QJS_SAFE_DELETE
#define QJS_SAFE_DELETE(ptr) \
if (ptr !=  nullptr) { \
    delete ptr; \
    ptr = nullptr; \
}

// QJS_PRETTY_FUNC: Pretty function name
#if defined(QJS_COMPILER_GCC) || defined(QJS_COMPILER_CLANG)
#define QJS_PRETTY_FUNC __PRETTY_FUNCTION__
#elif defined(QJS_COMPILER_MSVC)
#define QJS_PRETTY_FUNC __FUNCSIG__
#else
#define QJS_PRETTY_FUNC __func__  
#endif

// QJS_DISABLE_COPY
#define QJS_DISABLE_COPY(Class) \
    Class(const Class &) = delete;\
    Class &operator=(const Class &) = delete;

// QJS_DISABLE_MOVE
#define QJS_DISABLE_MOVE(Class) \
    Class(Class &&) = delete; \
    Class &operator=(Class &&) = delete;

#define QJS_DISABLE_COPY_AND_MOVE(Class) \
QJS_DISABLE_COPY(Class) \
QJS_DISABLE_MOVE(Class)

// ======================================
// Frameworks
// ======================================

// GLIB
#if !defined(QJS_HAS_GLIB)
#if defined(__G_LIB_H__) || defined(GLIB_MAJOR_VERSION)
#define QJS_HAS_GLIB 1
#endif
#endif

// GTK
#if !defined(QJS_HAS_GTK)
#if defined(__GTK_H__) || defined(GTK_MAJOR_VERSION)
#define QJS_HAS_GTK 1
#endif
#endif

// Qt
#ifdef QT_VERSION
#define QJS_HAS_QT 1
#define QJS_QT_VERSION QT_VERSION
#define QJS_QT_MAJOR_VERSION ((QT_VERSION >> 16) & 0xFF)
#define QJS_QT_MINOR_VERSION ((QT_VERSION >> 8) & 0xFF)
#define QJS_QT_PATCH_VERSION (QT_VERSION & 0xFF)
#define QJS_QT_VERSION_CHECK QT_VERSION_CHECK // QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)) Qt 5.12+
#endif

// QJS_HAS_OBJC
#ifdef QJS_OS_DARWIN
#if defined(__has_feature)
    #if __has_feature(blocks)
    #define QJS_HAS_OBJC 1
    #endif
#elif defined(__OBJC__)
    #define QJS_HAS_OBJC 1
#endif
#endif // QJS_OS_DARWIN

// ==================================
// CPU Instructions
// ==================================

// QJS_PAUSE_ASM
// QJS_PAUSE_ASM is used to pause the CPU for dozens or hundreds 
// of CPU cycles of decoding instructions.
#if defined(QJS_ARCH_X86)
#if defined(QJS_COMPILER_MSVC)
#include <intrin.h>
#define QJS_PAUSE_ASM() _mm_pause()
#else
#define QJS_PAUSE_ASM() __builtin_ia32_pause()
#endif
#elif defined(QJS_ARCH_ARM)
#if defined(QJS_COMPILER_MSVC)
#define QJS_PAUSE_ASM() __yield()
#else
#define QJS_PAUSE_ASM() __asm__ __volatile__("yield")
#endif
#else
#define QJS_PAUSE_ASM() ((void)0)
#endif

// ======================================
// Platform Export/Import Primitive Macros
// ======================================
#if defined(QJS_OS_WIN)
    #define QJS_DECL_EXPORT __declspec(dllexport)
    #define QJS_DECL_IMPORT __declspec(dllimport)
    #define QJS_DECL_HIDDEN
#elif defined(QJS_COMPILER_GCC) || defined(QJS_COMPILER_CLANG)
    #define QJS_DECL_EXPORT __attribute__((visibility("default")))
    #define QJS_DECL_IMPORT __attribute__((visibility("default")))
    #define QJS_DECL_HIDDEN __attribute__((visibility("hidden")))
#else
    #define QJS_DECL_EXPORT
    #define QJS_DECL_IMPORT
    #define QJS_DECL_HIDDEN
#endif

// ======================================
// QJS_API & QJS_LOCAL
// Static library: No configuration required
// Dynamic library: 
// Use "#define QJS_DYNAMIC_LIB" to build for a dynamic library.
// Use "#define QJS_EXPORT" to export APIs
// ======================================

#if defined(QJS_DYNAMIC_LIB)
    #if defined(QJS_EXPORT)
        #define QJS_API QJS_DECL_EXPORT
        #define QJS_LOCAL QJS_DECL_HIDDEN
    #else
        #define QJS_API QJS_DECL_IMPORT
        #define QJS_LOCAL QJS_DECL_HIDDEN
    #endif
#else
    #define QJS_API
    #define QJS_LOCAL
#endif