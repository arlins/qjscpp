#pragma once
#include <string>
#include <type_traits>
#include "quickjs.h"
#include "qjs_global.h"
#include "qjs_invoker.h"
#include "qjs_opaque.h"
#include "qjs_cast.h"
#include "qjs_tracker.h"

// ==================================
// QcClass<T>：Export class wrapper
// ==================================

NAMESPACE_QJS_BEGIN

// QcClassMeta
struct QcClassMeta {
	JSClassID classId = 0;
	std::string className = "";
	std::type_index typeIndex = std::type_index(typeid(void));

	QcClassMeta* parentClassMeta = nullptr;

	QcClassMeta() {}
};

// QcClass
template<typename T>
class QcClass {
public:
	static constexpr bool is_exported = false;

public:
	QcClass(JSContext* ctx, const char* className, JSValue parentModule)
		: m_ctx(ctx), m_className(className), m_parentModule(parentModule) {
		// Init class meta
		if (s_classMeta.classId == 0) {
			JS_NewClassID(&s_classMeta.classId);
			s_classMeta.className = className;
			s_classMeta.typeIndex = typeid(T);

			JSClassDef classDef = { 0 };
			classDef.class_name = className;
			classDef.finalizer = &QcOpaque::JSFinalizer<T>;

			JS_NewClass(JS_GetRuntime(m_ctx), s_classMeta.classId, &classDef);
		}

		// Create Class.prototype and associate it with class id
		m_protoTypeObject = JS_NewObject(m_ctx); // Class.prototype
		JS_SetClassProto(m_ctx, s_classMeta.classId, JS_DupValue(m_ctx, m_protoTypeObject));
	}

	QcClass(const QcClass& other) = delete;
	QcClass& operator=(QcClass& other) noexcept = delete;
	QcClass& operator=(QcClass&& other) noexcept = delete;

    QcClass(QcClass&& other) noexcept
        : m_ctx(other.m_ctx)
        , m_className(std::move(other.m_className))
        , m_parentModule(other.m_parentModule)
        , m_protoTypeObject(other.m_protoTypeObject)
        , m_classConstructor(other.m_classConstructor)
		, m_applyStaticInheritance(std::move(other.m_applyStaticInheritance)) {

        other.m_protoTypeObject = JS_UNDEFINED;
        other.m_classConstructor = JS_UNDEFINED;
        other.m_parentModule = JS_UNDEFINED;
        other.m_ctx = nullptr;
    }

	~QcClass() {
        if (m_ctx) {
            JS_FreeValue(m_ctx, m_protoTypeObject);
            JS_FreeValue(m_ctx, m_classConstructor);
        }
	}

	static JSClassID GetClassID() { 
		return s_classMeta.classId; 
	}

	static std::string GetClassName() {
		return s_classMeta.className;
	}

	static QcClassMeta& GetClassMeta() { 
		return s_classMeta; 
	}

	static JSValue GetClassConstructor(JSContext* ctx) {
		JSClassID classId = QcClass<T>::GetClassID();
		if (classId == 0) {
			return JS_UNDEFINED;
		}

		JSValue proto = JS_GetClassProto(ctx, classId);
        if (JS_IsUndefined(proto) || JS_IsNull(proto) || JS_IsException(proto)) {
            JS_FreeValue(ctx, proto);
            return JS_UNDEFINED;
        }

		JSValue ctor = JS_GetPropertyStr(ctx, proto, "constructor");
		JS_FreeValue(ctx, proto);
		return ctor;
	}

public:
	// Binding constructor
	QcClass& AddConstructor(JSCFunction* ctor_func) {
		if (!JS_IsUndefined(m_classConstructor)) {
			JS_FreeValue(m_ctx, m_classConstructor);
		}

		// JS_SetConstructor：Class of JS
		// m_classConstructor->prototype = m_protoTypeObject;
		// m_protoTypeObject->self_properties["constructor"] = m_classConstructor;
		m_classConstructor = JS_NewCFunction2(m_ctx, ctor_func, m_className.c_str(), 0, JS_CFUNC_constructor, 0);
		JS_SetConstructor(m_ctx, m_classConstructor, m_protoTypeObject);
		
        if (m_applyStaticInheritance) {
            m_applyStaticInheritance(m_ctx, m_classConstructor);
			m_applyStaticInheritance = nullptr;
        }

		if (!JS_IsUndefined(m_parentModule)) {
			JS_SetPropertyStr(m_ctx, m_parentModule, m_className.c_str(), JS_DupValue(m_ctx, m_classConstructor));
		}
		return *this;
	}

	// Binding constructor with automatic parameter conversion
	// The return value is owned by JS, Using AddConstructor to 
	// wrap return value owned by native for managing object 
	// memory Manually
	template<typename... Args>
	QcClass& AddNativeConstructor() {
		if (!JS_IsUndefined(m_classConstructor)) {
			JS_FreeValue(m_ctx, m_classConstructor);
		}

		m_classConstructor = QcNativeConstructorInvoker::Bind<T, Args...>(m_ctx, m_className.c_str());
		JS_SetConstructor(m_ctx, m_classConstructor, m_protoTypeObject);

        if (m_applyStaticInheritance) {
            m_applyStaticInheritance(m_ctx, m_classConstructor);
			m_applyStaticInheritance = nullptr;
        }

		if (!JS_IsUndefined(m_parentModule)) {
			JS_SetPropertyStr(m_ctx, m_parentModule, m_className.c_str(), JS_DupValue(m_ctx, m_classConstructor));
		}
		return *this;
	}

	// Setup class inherits from ParentType (JS prototype chain).
	template<typename ParentType>
	QcClass& Inherit() {
		QcClassMeta& parentMeta = QcClass<ParentType>::GetClassMeta();
		JSClassID parentClassId = parentMeta.classId;

#ifdef QJS_DEBUG
		if (parentClassId == 0) {
			auto err = format_str("Inherit failed: Base class <%s> has not been registered yet!", type_name<ParentType>().c_str());
			QJS_ASSERT_M(false, err.c_str());
		}
#endif

		// Inheriting instance methods: T.prototype.__proto__ = ParentType.prototype
		JSValue targetProtoType = JS_GetClassProto(m_ctx, parentClassId); // ParentType.prototype
		if (!JS_IsUndefined(targetProtoType) && !JS_IsNull(targetProtoType)) {
			// T.prototype.__proto__ = ParentType.prototype
			JS_SetPrototype(m_ctx, m_protoTypeObject, targetProtoType);
			JS_FreeValue(m_ctx, targetProtoType);
		}

		// Inheriting static methods: T.__proto__ = ParentType (Constructor)
        m_applyStaticInheritance = [](JSContext* ctx, JSValue ctor) {
			if (JS_IsUndefined(ctor) || JS_IsNull(ctor)) {
				return;
			}

            JSValue targetClassCtor = QcClass<ParentType>::GetClassConstructor(ctx);
            if (!JS_IsUndefined(targetClassCtor) && !JS_IsNull(targetClassCtor) && !JS_IsException(targetClassCtor)) {
                JS_SetPrototype(ctx, ctor, targetClassCtor);
                JS_FreeValue(ctx, targetClassCtor);
            }
        };

        if (!JS_IsUndefined(m_classConstructor) && m_applyStaticInheritance) {
            m_applyStaticInheritance(m_ctx, m_classConstructor);
			m_applyStaticInheritance = nullptr;
        }

		// Meta chain
		s_classMeta.parentClassMeta = &parentMeta;

		// Register classes relationship
        // Registration of classes relationship is required to support the safe pointer conversion 
		// performed by `GetObjectAs` in QcTracker. Only non-`Trackable` types require the 
		// registration of static cast offset matrices; for `Trackable` types, `GetObjectAs` uses 
		// `dynamic_cast` for conversion, so registration is unnecessary.
        _RegisterCastClassIfNonTrackable<ParentType, T>();

		return *this;
	}

	// Add member functions of type JSCFunction
	QcClass& AddMemFunc(const char* name, JSCFunction* func, int argsCount = 0) {
		JSValue func_val = JS_NewCFunction(m_ctx, func, name, argsCount);
		JS_DefinePropertyValueStr(m_ctx, m_protoTypeObject, name, func_val, JS_PROP_CONFIGURABLE | JS_PROP_WRITABLE);
		return *this;
	}

	// Add member functions with automatic parameter conversion
	// Return type must be value type or pointer to class
	template<QcReturnPolicy Policy = QcReturnPolicy::Default, typename MemFunc>
	QcClass& AddNativeMemFunc(const char* name, MemFunc memFunc) {
		JSValue func_val = QcNativeMemberFuncInvoker::Bind<Policy, T>(m_ctx, memFunc, name);
		JS_DefinePropertyValueStr(m_ctx, m_protoTypeObject, name, func_val, JS_PROP_CONFIGURABLE | JS_PROP_WRITABLE);
		return *this;
	}

	// Add static functions to the class of type JSCFunction
	QcClass& AddStaticFunc(const char* name, JSCFunction* func, int argsCount = 0) {
		if (!JS_IsUndefined(m_classConstructor)) {
			JSValue func_val = JS_NewCFunction(m_ctx, func, name, argsCount);
			JS_DefinePropertyValueStr(m_ctx, m_classConstructor, name, func_val, JS_PROP_CONFIGURABLE | JS_PROP_WRITABLE);
		}
		return *this;
	}

	// Add static functions to the class with automatic parameter conversion
	template<QcReturnPolicy Policy = QcReturnPolicy::Default, typename Ret, typename... Args>
	QcClass& AddNativeStaticFunc(const char* name, Ret(*func)(Args...)) {
#ifdef QJS_DEBUG
		if (std::is_same<decltype(func), JSCFunction*>::value) {
			QJS_ASSERT_M(false, "Do not use AddNativeStaticFunc to export JSCFunction, use AddStaticFunc instead.");
		}
#endif

		if (!JS_IsUndefined(m_classConstructor)) {
			JSValue func_val = QcNativeStaticFuncInvoker::Bind<Policy>(m_ctx, func, name);
			JS_DefinePropertyValueStr(m_ctx, m_classConstructor, name, func_val, JS_PROP_CONFIGURABLE | JS_PROP_WRITABLE);
		}
		return *this;
	}

	JSValue GetConstructor() const {
		return m_classConstructor;
	}

	JSValue GetProtoType() const {
		return m_protoTypeObject;
	}

private:
    template <typename ParentType, typename ChildType>
    static typename std::enable_if<!is_trackable_v<ParentType> || !is_trackable_v<ChildType>>::type
        _RegisterCastClassIfNonTrackable() noexcept {
        RegisterCastClass<ParentType, ChildType>();
    }

    template <typename ParentType, typename ChildType>
    static typename std::enable_if<is_trackable_v<ParentType>&& is_trackable_v<ChildType>>::type
        _RegisterCastClassIfNonTrackable() noexcept {
        // Do nothing. Trackable uses dynamic_cast through 
		// m_trackablePtr natively for pointer conversion in QcTracker
    }
  
private:
	std::string m_className;
	JSContext* m_ctx = nullptr;
	JSValue m_parentModule = JS_UNDEFINED;
	JSValue m_protoTypeObject = JS_UNDEFINED; // JSObject*
	JSValue m_classConstructor = JS_UNDEFINED; // JSFunction*
	std::function<void(JSContext*, JSValue)> m_applyStaticInheritance;

private:
	static QcClassMeta s_classMeta;
};

template<typename T>
QcClassMeta QcClass<T>::s_classMeta = QcClassMeta();

NAMESPACE_QJS_END


// QC_REGISTER_EXPORTED_CLASS
// To determine at compile time whether T is an exported class 
#define QC_REGISTER_EXPORTED_CLASS(ClassType) \
template<> \
constexpr bool ::qjscpp::QcClass<ClassType>::is_exported = true;