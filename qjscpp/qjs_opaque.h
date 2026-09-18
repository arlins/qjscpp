#pragma once
#include <string>
#include "quickjs.h"
#include "qjs_global.h"
#include "qjs_tracker.h"

// =====================================
// QcOpaque 
// Holding an opaque ptr in JSValue
// =====================================

/* ************************************************************************
[ QuickJS ]
	 JSValue (this)
		│
		▼ (Opaque)
  [ QcTracker (control block) ]   ◄──────    [ C++ ]
		├── rawPtr (NativeObject*)    ───►    NativeObject
		└── isAlive (bool)            ◄────         (delete NativeObject: isAlive = false)

************************************************************************ */

NAMESPACE_QJS_BEGIN

// Declaration
template <typename T>
class QcClass;

template <typename T, typename Enable>
struct QcNativeToJSValue;

class QcNativeConstructorInvoker;

template <QcReturnPolicy Policy, typename Ret>
typename std::enable_if<std::is_pointer<Ret>::value, JSValue>::type
WrapReturnJSValue(JSContext*, Ret, JSValueConst);


// QcOpaqueOwnedByNativeChecker
template <typename T>
struct QcOpaqueOwnedByNativeChecker {
	using type = void;
	QJS_STATIC_ASSERT_M(is_exported_class_v<T>, "T must be a exported class");
	QJS_STATIC_ASSERT_M(std::is_polymorphic<T>::value, "T must be polymorphic");
	// QJS_STATIC_ASSERT_M(is_trackable_v<T>, "Class must be based on QcTrackable");
};

// QcOpaqueOwnedByJSChecker
template <typename T>
struct QcOpaqueOwnedByJSChecker {
    using type = void;
    QJS_STATIC_ASSERT_M(is_exported_class_v<T>, "T must be a exported class");
};


// QcOpaque
class QcOpaque {
public:
	// WrapOwnedByNative
	template <typename T>
	static JSValue WrapOwnedByNative(JSContext* ctx, T* ptr) {
		using OpaqueChecker = QcOpaqueOwnedByNativeChecker<T>::type;
#ifdef QJS_DEBUG
		auto debugClassName = type_name<T>();
#endif
		if (ptr == nullptr) {
			return JS_NULL;
		}

        JSClassID classId = QcClass<T>::GetClassID();
        if (classId == 0) {
			QJS_ASSERT_M(false, format_str("Failed to wrap ptr for class which is not registered: id = %u, name = <%s>", 
				classId, debugClassName.c_str()));
			return JS_ThrowTypeError(ctx, "Failed to wrap ptr for class which is not registered.");
        }

		// Create or get tracker
        QcTracker* tracker = QcTrackerResolver::Create(ptr); // RefCount + 1
		if (tracker == nullptr) {
			return JS_ThrowTypeError(ctx, "Failed to create or retrieve QcTracker for given pointer.");
		}

        JSValue obj = JS_NewObjectClass(ctx, classId);
        if (JS_IsException(obj)) {
			tracker->Release();
            return obj;
        }
		JS_SetOpaque(obj, tracker); // Set opaque ptr of JSValue

		return obj;
	}

	// GetOpaquePtr
	template<typename T>
	static T* GetOpaquePtr(JSContext* ctx, JSValueConst val) {
		auto* tracker = GetTracker(val);
		return tracker ? tracker->GetObjectAs<T>() : nullptr;
	}

	// ResetOpaque
	static void ResetOpaque(JSValueConst val) {
		auto* tracker = GetTracker(val);
		if (tracker) {
			tracker->Release();
			JS_SetOpaque(val, nullptr);
		}
	}

	// JSFinalizer
	template<typename T>
	static void JSFinalizer(JSRuntime* rt, JSValue val) {
		auto* tracker = GetTracker(val);
		if (tracker) {
			tracker->Release();
			JS_SetOpaque(val, nullptr);
		}
	}

private:
	// GetTracker
    static QcTracker* GetTracker(JSValueConst val) {
        if (!JS_IsObject(val)) {
            return nullptr;
        }

        JSClassID classId = JS_GetClassID(val);
        auto* tracker = static_cast<QcTracker*>(JS_GetOpaque(val, classId));
        return tracker;
    }

	// WrapNewOwnedByJS
    template <typename T>
    static JSValue WrapNewOwnedByJS(JSContext* ctx, T* ptr) {
        using OpaqueChecker = QcOpaqueOwnedByJSChecker<T>::type;
        if (ptr == nullptr) {
            return JS_NULL;
        }

        JSClassID classId = QcClass<T>::GetClassID();
        if (classId == 0) {
			delete ptr; // Should delete ptr when failed
            QJS_ASSERT_M(false, "Failed to wrap ptr for class which is not registered.");
            return JS_ThrowTypeError(ctx, "Failed to wrap ptr for class which is not registered.");
        }

        JSValue obj = JS_NewObjectClass(ctx, classId);
        if (JS_IsException(obj)) {
			delete ptr;
            return obj;
        }

        // Create tracker
        QcTracker* tracker = nullptr;
        try {
            tracker = new QcTracker(ptr, true);
        } catch (...) {
            delete ptr;
            JS_FreeValue(ctx, obj);
            return JS_ThrowOutOfMemory(ctx);
        }

		// Set opaque ptr of JSValue
        JS_SetOpaque(obj, tracker);

        return obj;
    }
	
private: // friend
    template <typename U, typename Enable>
    friend struct QcNativeToJSValue;

    friend class QcNativeConstructorInvoker;

    template <QcReturnPolicy Policy, typename Ret>
    friend typename std::enable_if<std::is_pointer<Ret>::value, JSValue>::type
	WrapReturnJSValue(JSContext*, Ret, JSValueConst);
};



NAMESPACE_QJS_END