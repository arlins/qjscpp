#pragma once
#include <string>
#include <utility>
#include "quickjs.h"
#include "qjs_global.h"
#include "qjs_converter.h"
#include "qjs_invoker.h"
#include "qjs_class.h"

// ==================================
// QcModule：Exporting modules
// ==================================

NAMESPACE_QJS_BEGIN

class QcModule {
public:
	// name = nullptr means globalThis
	QcModule(JSContext* ctx, const char* name = nullptr) 
		: m_ctx(ctx)
		, m_name(name ? name : "")
		, m_isGlobal(name == nullptr || name[0] == '\0') {
		if (m_isGlobal) {
			m_moduleObject = JS_GetGlobalObject(m_ctx);
		} else {
			m_moduleObject = JS_NewObject(m_ctx);
		}
	}

	~QcModule() {
		if (m_ctx && !JS_IsUndefined(m_moduleObject)) {
			JS_FreeValue(m_ctx, m_moduleObject);
		}
	}

    QcModule(const QcModule&) = delete;
    QcModule& operator=(const QcModule&) = delete;

    QcModule(QcModule&& other) noexcept
        : m_name(std::move(other.m_name))
        , m_ctx(other.m_ctx)
        , m_moduleObject(other.m_moduleObject)
        , m_isGlobal(other.m_isGlobal) {
        other.m_moduleObject = JS_UNDEFINED;
        other.m_ctx = nullptr;
    }

    QcModule& operator=(QcModule&& other) noexcept {
        if (this != &other) {
            if (!JS_IsUndefined(m_moduleObject) && m_ctx) {
                JS_FreeValue(m_ctx, m_moduleObject);
            }
            m_name = std::move(other.m_name);
            m_ctx = other.m_ctx;
            m_moduleObject = other.m_moduleObject;
            m_isGlobal = other.m_isGlobal;

            other.m_moduleObject = JS_UNDEFINED;
            other.m_ctx = nullptr;
        }
        return *this;
    }

	// Detach module with name
	static void DetachModule(JSContext* ctx, const char* moduleName) {
		if (ctx == nullptr || moduleName == nullptr) {
			return;
		}

        JSValue global_obj = JS_GetGlobalObject(ctx);
        JSAtom prop = JS_NewAtom(ctx, moduleName);
        JS_DeleteProperty(ctx, global_obj, prop, 0);
        JS_FreeAtom(ctx, prop);
        JS_FreeValue(ctx, global_obj);
	}

	// Export class
	template<typename T>
	QcClass<T> ExportClass(const char* className) {
		return QcClass<T>(m_ctx, className, m_moduleObject);
	}

	// Export global functions
	QcModule& ExportCFunction(const char* name, JSCFunction* func, int argsCount = 0) {
		JSValue func_val = JS_NewCFunction(m_ctx, func, name, argsCount);
		JS_SetPropertyStr(m_ctx, m_moduleObject, name, func_val);
		return *this;
	}

	// Export global functions with automatic parameter conversion
	template<QcReturnPolicy Policy = QcReturnPolicy::Default, typename Ret, typename... Args>
	QcModule& ExportNativeFunc(const char* name, Ret(*func)(Args...)) {
#ifdef QJS_DEBUG
		if (std::is_same<decltype(func), JSCFunction*>::value) {
			QJS_ASSERT_M(false, "Do not use ExportNativeFunc to export JSCFunction, use ExportCFunction instead.");
		}
#endif

		JSValue func_val = QcNativeStaticFuncInvoker::Bind<Policy>(m_ctx, func, name);
		JS_SetPropertyStr(m_ctx, m_moduleObject, name, func_val);
		return *this;
	}

	// Attach the module to global
	void Attach() {
		if (m_isGlobal) {
			QJS_ASSERT_M(false, "No need to Attach in Global Mode");
			return;
		}

		JSValue global_obj = JS_GetGlobalObject(m_ctx);
		JS_SetPropertyStr(m_ctx, global_obj, m_name.c_str(), JS_DupValue(m_ctx, m_moduleObject));
		JS_FreeValue(m_ctx, global_obj);
	}

	// Detach the module to global
	void Detach() {
		if (m_isGlobal) {
			QJS_ASSERT_M(false, "No need to Detach in Global Mode");
			return;
		}

        JSValue global_obj = JS_GetGlobalObject(m_ctx);
        JSAtom prop = JS_NewAtom(m_ctx, m_name.c_str());
        JS_DeleteProperty(m_ctx, global_obj, prop, 0);
        JS_FreeAtom(m_ctx, prop);
        JS_FreeValue(m_ctx, global_obj);
	}

	JSValue GetModuleObject() const { 
		return m_moduleObject; 
	}

private:
	std::string m_name;
	JSContext* m_ctx = nullptr;
	JSValue m_moduleObject = JS_UNDEFINED;
	bool m_isGlobal = false;
};

NAMESPACE_QJS_END