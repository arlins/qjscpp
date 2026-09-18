#pragma once
#include "quickjs.h"
#include "libc/quickjs-libc.h"
#include "qjs_global.h"
#include "qjs_api.h"

NAMESPACE_QJS_BEGIN

class QcContext {
public:
    explicit QcContext(JSRuntime* runTime) : m_jsRunTime(runTime) {
		if (m_jsRunTime) {
			m_jsContext = JS_NewContext(m_jsRunTime);

            if (m_jsContext) {
                // console.log & std helpers
                js_std_add_helpers(m_jsContext, 0, nullptr);
                js_init_module_std(m_jsContext, "std");
                js_init_module_os(m_jsContext, "os");
            }
		}
	}

	~QcContext() {
		if (m_jsContext) {
			JS_FreeContext(m_jsContext);
		}
	}

    QcContext(const QcContext&) = delete;
    QcContext& operator=(const QcContext&) = delete;

    QcContext(QcContext&& other) noexcept
        : m_jsRunTime(other.m_jsRunTime)
        , m_jsContext(other.m_jsContext) {
        other.m_jsRunTime = nullptr;
        other.m_jsContext = nullptr;
    }

    QcContext& operator=(QcContext&& other) noexcept {
        if (this != &other) {
            if (m_jsContext) {
                JS_FreeContext(m_jsContext);
            }
            m_jsRunTime = other.m_jsRunTime;
            m_jsContext = other.m_jsContext;
            other.m_jsRunTime = nullptr;
            other.m_jsContext = nullptr;
        }
        return *this;
    }

	JSRuntime* GetJSRunTime() const {
		return m_jsRunTime;
	}

	JSContext* GetJSContext() const {
		return m_jsContext;
	}

	QcResult RunJs(const char* js_code, const char* file = "eval.js") {
		if (m_jsContext == nullptr || js_code == nullptr) {
			return QcResult("Invalid JSContext or code string");
		}
		return QcApi::RunJs(m_jsContext, js_code, file);
	}

private:
	JSRuntime* m_jsRunTime = nullptr;
	JSContext* m_jsContext = nullptr;
};

NAMESPACE_QJS_END
