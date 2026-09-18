#pragma once
#include <unordered_map>
#include <string>
#include <mutex>
#include "qjs_global.h"

NAMESPACE_QJS_BEGIN

#ifdef QJS_DEBUG

class QcDebugSymbolRegistry {
public:
	static QcDebugSymbolRegistry& GetInstance() {
		static QcDebugSymbolRegistry instance;
		return instance;
	}

	static void RegisterSymbol(const std::string& key, const std::string& val) {
		GetInstance()._RegisterSymbol(key, val);
	}

	static std::string GetSymbolName(const std::string& key) {
		return GetInstance()._GetSymbolName(key);
	}

private:
	void _RegisterSymbol(const std::string& key, const std::string& val) {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_symbolMap[key] = val;
	}

	std::string _GetSymbolName(const std::string& key) {
		std::lock_guard<std::mutex> lock(m_mutex);
		auto it = m_symbolMap.find(key);
		if (it != m_symbolMap.end()) {
			return it->second;
		}

		return "UnknownSymbol";
	}

private:
	std::mutex m_mutex;
	std::unordered_map<std::string, std::string> m_symbolMap;
};

#endif // QJS_DEBUG

NAMESPACE_QJS_END