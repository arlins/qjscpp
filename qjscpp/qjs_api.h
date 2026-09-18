#pragma once
#include <string>
#include <utility>
#include <cstring>
#include "quickjs.h"
#include "qjs_global.h"

// ===========================
// JS API Wrapper
// ===========================
NAMESPACE_QJS_BEGIN

class QcResult {
public:
	QcResult() 
		: m_ctx(nullptr), m_val(JS_UNDEFINED), m_hasError(false) {
	}

	QcResult(JSContext* ctx, JSValue val)
		: m_ctx(ctx), m_val(val), m_hasError(false) {
	}

	QcResult(std::string err_msg)
		: m_ctx(nullptr), m_val(JS_EXCEPTION), m_error(std::move(err_msg)), m_hasError(true) {
	}

	~QcResult() {
		Reset();
	}

	QcResult(const QcResult&) = delete;
	QcResult& operator=(const QcResult&) = delete;

	QcResult(QcResult&& other) noexcept
		: m_ctx(other.m_ctx), m_val(other.m_val), m_error(std::move(other.m_error)), m_hasError(other.m_hasError) {
		other.m_ctx = nullptr;
		other.m_val = JS_UNDEFINED;
		other.m_hasError = false;
	}

	QcResult& operator=(QcResult&& other) noexcept {
		if (this != &other) {
			Reset();

			m_ctx = other.m_ctx;
			m_val = other.m_val;
			m_error = std::move(other.m_error);
			m_hasError = other.m_hasError;

			other.m_ctx = nullptr;
			other.m_val = JS_UNDEFINED;
			other.m_hasError = false;
		}
		return *this;
	}

	QcResult Clone() const {
        if (m_hasError) {
            return QcResult(m_error);
        }
        if (!m_ctx) {
            return QcResult("Null ctx");
        }
		
		return QcResult(m_ctx, JS_DupValue(m_ctx, m_val));
	}

	bool IsError() const { return m_hasError; }
	const std::string& Error() const { return m_error; }
	JSValue Get() const { return m_val; }

private:
	void Reset() {
		if (m_ctx && !JS_IsUndefined(m_val)) {
			JS_FreeValue(m_ctx, m_val);
			m_val = JS_UNDEFINED;
		}
		m_ctx = nullptr;
	}

private:
	JSContext* m_ctx = nullptr;
	JSValue m_val = JS_UNDEFINED;
	std::string m_error;
	bool m_hasError = false;
};

struct QcApi {
	// RunJs
	static QcResult RunJs(JSContext* ctx, const char* js_code, const char* file = "eval.js") {
		if (!ctx || !js_code) {
			return QcResult("Invalid JSContext or code string");
		}

		JSValue val = JS_Eval(ctx, js_code, std::strlen(js_code), file, JS_EVAL_TYPE_GLOBAL);

		if (JS_IsException(val)) {
			JSValue exc = JS_GetException(ctx);
			const char* err_str = JS_ToCString(ctx, exc);
			std::string err_msg = err_str ? err_str : "Unknown JS Exception";
			if (err_str) JS_FreeCString(ctx, err_str);

#ifdef QJS_DEBUG
			// Get stack
            JSValue stack = JS_GetPropertyStr(ctx, exc, "stack");
            if (!JS_IsUndefined(stack)) {
                const char* stack_str = JS_ToCString(ctx, stack);
                if (stack_str) {
                    err_msg += "\nStack Trace:\n";
                    err_msg += stack_str;
                    JS_FreeCString(ctx, stack_str);
                }
            }
			JS_FreeValue(ctx, stack);
#endif

			JS_FreeValue(ctx, exc);
			JS_FreeValue(ctx, val);

			return QcResult(err_msg);
		}

		return QcResult(ctx, val);
	}
};

NAMESPACE_QJS_END