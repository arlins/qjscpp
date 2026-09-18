#pragma once
#include "quickjs.h"
#include "qjs_global.h"

NAMESPACE_QJS_BEGIN

class QcRunTime {
public:
	QcRunTime() {
		m_jsRunTime = JS_NewRuntime();
        if (m_jsRunTime) {
            js_std_init_handlers(m_jsRunTime);
        }
	}

	~QcRunTime() {
		if (m_jsRunTime) {
			js_std_free_handlers(m_jsRunTime);
			JS_FreeRuntime(m_jsRunTime);
			m_jsRunTime = nullptr;
		}
	}

    QcRunTime(const QcRunTime&) = delete;
    QcRunTime& operator=(const QcRunTime&) = delete;

    QcRunTime(QcRunTime&& other) noexcept
        : m_jsRunTime(other.m_jsRunTime) {
        other.m_jsRunTime = nullptr;
    }

    QcRunTime& operator=(QcRunTime&& other) noexcept {
        if (this != &other) {
            if (m_jsRunTime) {
                js_std_free_handlers(m_jsRunTime);
                JS_FreeRuntime(m_jsRunTime);
            }
            m_jsRunTime = other.m_jsRunTime;
            other.m_jsRunTime = nullptr;
        }
        return *this;
    }

	void SetRuntimeInfo(const char* info) {
		if (m_jsRunTime) {
			JS_SetRuntimeInfo(m_jsRunTime, info);
		}
	}

	void SetMemoryLimit(size_t limit) {
		if (m_jsRunTime) {
			JS_SetMemoryLimit(m_jsRunTime, limit);
		}
	}

	void SetGCThreshold(size_t gc_threshold) {
		if (m_jsRunTime) {
			JS_SetGCThreshold(m_jsRunTime, gc_threshold);
		}
	}

	void SetMaxStackSize(size_t stack_size) {
		if (m_jsRunTime) {
			JS_SetMaxStackSize(m_jsRunTime, stack_size);
		}
	}

	void RunGC() {
		if (m_jsRunTime) {
			JS_RunGC(m_jsRunTime);
		}
	}

	JSRuntime* GetJSRunTime() const {
		return m_jsRunTime;
	}

private:
	JSRuntime* m_jsRunTime = nullptr;
};

NAMESPACE_QJS_END
