## 🎯 Overview

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Standard](https://img.shields.io/badge/C%2B%2B-14%20%7C%2017%20%7C%2020-b30047.svg)](https://en.cppreference.com/w/cpp/14)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Darwin%20%7C%20Linux%20%7C%20Android%20%7C%20HarmonyOS%20%7C%20Unix-lightgrey.svg)]()

`qjscpp` is a lightweight, modern C++14 binding and bridging library designed to enable efficient and seamless interaction between C++ and the QuickJS JavaScript engine.


* **Seamless Integration**: Powered by template metaprogramming, eliminating the need to write tedious glue code manually.

* **Type Safety**: Automatically infers function signatures and type conversions, providing dual type checks at both compile time and runtime.

* **High Efficiency & Safety**: Offers a zero-overhead binding mechanism with built-in comprehensive memory management and wild pointer protection.

* **Thread Model**: **The entire library is non-thread-safe.** All calls related to `JSContext` and bound objects must take place within a single thread (or their respective isolated thread/context instances).

---

## 📌 Core Principles and Mechanisms

### 1. Memory Management Mechanism

#### Object Ownership Model

* **JS-Owned**: Suitable for C++ objects instantiated within JS via `new`. When the JS object is garbage-collected (GC) and triggers its Finalizer, the underlying C++ native instance is automatically deleted to prevent C++ memory leaks.

* **Native-Owned**: Suitable for objects whose lifecycle is managed on the C++ side. The JS side only holds a weak reference or observer pointer; when the JS wrapper object is garbage-collected, it **does not** trigger the deletion of the C++ instance.

#### Wild Pointer Protection (Tracker Mechanism)

Prevents crashes caused by "C++ native objects being destroyed externally while JS still holds a reference and continues to call member functions":

* **Intrusive Tracking**: Exported classes must inherit from the `QcTrackable` base class for automatic lifecycle management. The invalid status is automatically synchronized upon object destruction. If JS attempts to invoke a member function on a destroyed object, a `TypeError` exception is thrown instead of causing a crash.

* **Non-Intrusive Tracking**: Designed for third-party C++ classes where inheriting from a base class is impractical. Developers must manually call `QcTrackerResolver::MarkObjectDestroyed` when the object is destroyed, leveraging a global Canonical Address mapping to achieve lifecycle validation and protection.

---

### 2. Function Binding (Automatic Invoker Deduction)

The library uses template traits extraction to automatically deduce C++ function signatures, supporting automatic binding for three function types: **static/global functions**, **class member functions**, and **class constructors**.

Taking a **class member function (`QcNativeMemberFuncInvoker`)** as an example, its execution workflow is as follows:

1. **Obtain `this**`: Fetches the member object pointer via `QcOpaque::GetOpaquePtr<T>(ctx, this_val)`. If it returns `nullptr` (the object was destroyed on the C++ side, or JS passed an invalid `this`), a `TypeError` is thrown.
2. **Unpack Closure**: Reads the bound member function pointer.
3. **Unpack Arguments (`ConvertJsArgToNative`)**: Converts each parameter and **automatically guards against null pointer dereferencing** during pointer/reference dereferencing or implicit conversions.
4. **Execute Member Function**: Safely executes `(this_ptr->*method)(...)`.
5. **Wrap Return Value**: Packages and returns the return value to JS according to the specified `QcReturnPolicy`.

---

### 3. Type Conversion

* **Primitive Types & STL**: Supports arbitrary parameters and return types through specialization; out-of-the-box support includes `int`, `std::string`, `std::vector`, etc.

* **Rules for Exported Classes**:
    * Automatically supports parameter types `T*`, `T&`, and return type `T*`.
    * If the exported class contains a default constructor, it automatically supports parameter types `T`, `T&&`, and return type `T`.
    * Support for parameter types `T` and `T&&` can be manually extended by defining `QC_DEFINE_EXPORTED_CLASS_JSVALUE_CONVERTER`.

---

## 🗺️ Module Overview

| File Name | Description |
| --- | --- |
| **`qjs_api.h`** | Wraps QuickJS underlying C APIs |
| **`qjs_arg.h`** | Function argument container and conversion management |
| **`qjs_cast.h`** | Safe type conversion and downcasting |
| **`qjs_class.h`** | C++ class export and prototype chain binding |
| **`qjs_context.h`** | `JSContext` wrapper and management |
| **`qjs_converter.h`** | Two-way type conversion between JS and Native |
| **`qjs_debug.h`** | Debug logging and stack tracing tools |
| **`qjs_global.h`** | Exporting global objects and variables |
| **`qjs_invoker.h`** | Function signature traits extraction and invocation routing |
| **`qjs_module.h`** | Exporting JS modules and namespaces |
| **`qjs_opaque.h`** | Type mapping for Opaque native pointers |
| **`qjs_runtime.h`** | `JSRuntime` and Garbage Collection (GC) management |
| **`qjs_stl.h`** | STL container type conversion specializations |
| **`qjs_trackable.h`** | Intrusive object lifecycle tracking |
| **`qjs_tracker.h`** | Wild pointer protection and Tracker control |
| **`qjs_traits.h`** | Template metaprogramming traits extraction |
| **`qjs_utility.h`** | Internal utility functions and exception handling |
---

## 🚀 QuickStart

### 1. C++ Binding Example

```cpp
#include "qjscpp/qjscpp.h"

// MyPoint
struct MyPoint {
    int x = 0; int y = 0;
    MyPoint() {};
    MyPoint(int _x, int _y) : x(_x), y(_y) {}
};

// MyRect
struct MyRect {
    int x = 0; int y = 0; int w = 0; int h = 0;

    MyRect() = default;
    MyRect(int _x, int _y, int _w, int _h) : x(_x), y(_y), w(_w), h(_h) {}

    int get_x() { return x; };
    int get_y() { return y; };
    int get_w() { return w; };
    int get_h() { return h; };
};

// Namespace of qjscpp
NAMESPACE_QJS_BEGIN

// Specialization MyPoint: Supports parameter variants such as T*, T&, T&&, 
// and T, as well as the return type T.
template<>
struct QcJSValueToNative<MyPoint> {
    static MyPoint FromJs(JSContext* ctx, JSValueConst val) {
        if (!JS_IsObject(val)) return MyPoint{ 0, 0 };
        MyPoint pt;
        pt.x = QcJSValueToNative<int>::FromJs(ctx, JS_GetPropertyStr(ctx, val, "x"));
        pt.y = QcJSValueToNative<int>::FromJs(ctx, JS_GetPropertyStr(ctx, val, "y"));
        return pt;
    }
};

template<>
struct QcNativeToJSValue<MyPoint> {
    static JSValue ToJs(JSContext* ctx, const MyPoint& obj) {
        JSValue js_obj = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, js_obj, "x", QcNativeToJSValue<int>::ToJs(ctx, obj.x));
        JS_SetPropertyStr(ctx, js_obj, "y", QcNativeToJSValue<int>::ToJs(ctx, obj.y));
        return js_obj;
    }
};

// Exporting `MyRect`: Automatically supports parameter variants such as 
// `T*`, `T&`, `T&&`, and `T`, as well as return types `T` and `T*`.
QC_REGISTER_EXPORTED_CLASS(MyRect)

NAMESPACE_QJS_END


// TestMyPointArg
MyPoint TestMyPointArg(MyPoint arg0, const MyPoint& arg1, MyPoint&& arg2, MyPoint* arg3) {
    return MyPoint(arg0.x + 10, arg0.y + 20);
}

// TestMyRectArg
MyRect TestMyRectArg(MyRect arg0, const MyRect& arg1, MyRect&& arg2, MyRect* arg3) {
    return MyRect(arg0.x + 1, arg0.y + 2, arg0.w + 3, arg0.h +4);
}

// main
int main() {
    using namespace qjscpp;

    // Init runtime and context
    QcRunTime runTime;
    QcContext context(runTime.GetJSRunTime());
    JSContext* ctx = context.GetJSContext();

    // Export MyRect
    QcModule globalModule(ctx);
    auto rectCls = globalModule.ExportClass<MyRect>("MyRect");
    rectCls.AddNativeConstructor<int, int, int, int>();
    rectCls.AddNativeMemFunc("get_x", &MyRect::get_x);
    rectCls.AddNativeMemFunc("get_y", &MyRect::get_y);
    rectCls.AddNativeMemFunc("get_w", &MyRect::get_w);
    rectCls.AddNativeMemFunc("get_h", &MyRect::get_h);

    // Export demo
    QcModule testModule(ctx, "test");
    testModule.ExportNativeFunc("TestMyPointArg", TestMyPointArg);
    testModule.ExportNativeFunc("TestMyRectArg", TestMyRectArg);
    testModule.Attach();

    const char* js_code = u8R"xx(
        const pos = { x: 10, y: 20 };
        const retPos = test.TestMyPointArg(pos, pos, pos, pos);
        console.log("retPos:", JSON.stringify(retPos)); // {"x":20,"y":40}

        const rect = new MyRect(10, 10, 100, 100);
        const retRect = test.TestMyRectArg(rect, rect, rect, rect);
        console.log("retRect: x=", retRect.get_x(), " y=", retRect.get_y(), " w=", retRect.get_w(), " h=", retRect.get_h());
    )xx";

    auto res = QcApi::RunJs(ctx, js_code);
    if (res.IsError()) {
        printf("JS Exception: %s\n", res.Error().c_str());
    }

    // Detach module
    QcModule::DetachModule(ctx, "demo");
    return 0;
}

```

---

## 📜 License

This project is licensed under the [MIT License](LICENSE).