#ifndef COMMON_REGISTRY_HPP
#define COMMON_REGISTRY_HPP

#ifdef _WIN32

#include <windows.h>
#include <string>
#include <vector>
#include <optional>
#include <system_error>
#include <cstring>

namespace reg {

class Key {
private:
	HKEY hKey = nullptr;
	std::wstring sub_key;

public:
	Key() = delete;

	Key(HKEY root, const std::wstring& sub_key, REGSAM access = KEY_READ | KEY_WRITE)
	    : sub_key(sub_key) {
		openOrCreate(root, sub_key, access);
	}

	~Key() {
		if (hKey) {
			RegCloseKey(hKey);
		}
	}

	std::error_code openOrCreate(HKEY root, const std::wstring& sub_key, REGSAM access) {
		HKEY temp = nullptr;
		LONG res = RegCreateKeyExW(root, sub_key.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE,
		                           access, nullptr, &temp, nullptr);

		if (res != ERROR_SUCCESS)
			return std::error_code(res, std::system_category());

		if (hKey)
			RegCloseKey(hKey);
		hKey = temp;
		return {};
	}

	bool valid() const { return hKey != nullptr; }

	std::string get_sub_key() const { return std::string(sub_key.begin(), sub_key.end()); }

	std::error_code set_wstring(const std::wstring& name, const std::wstring& value);
	std::optional<std::wstring> get_wstring(const std::wstring& name,
	                                             std::error_code& ec) const;

	std::error_code set_dword(const std::wstring& name, const DWORD& value);
	std::optional<DWORD> get_dword(const std::wstring& name, std::error_code& ec) const;

	std::error_code set_qword(const std::wstring& name, const ULONGLONG& value);
	std::optional<ULONGLONG> get_qword(const std::wstring& name, std::error_code& ec) const;

private:
	std::error_code setRaw(const std::wstring& name, DWORD type, const BYTE* data, DWORD size) {
		LONG res = RegSetValueExW(hKey, name.c_str(), 0, type, data, size);
		if (res != ERROR_SUCCESS)
			return std::error_code(res, std::system_category());
		return {};
	}

	std::error_code queryRaw(const std::wstring& name, DWORD& type,
	                         std::vector<BYTE>& buffer) const {
		DWORD size = 0;
		LONG res = RegQueryValueExW(hKey, name.c_str(), nullptr, &type, nullptr, &size);
		if (res != ERROR_SUCCESS)
			return std::error_code(res, std::system_category());

		buffer.resize(size);
		res = RegQueryValueExW(hKey, name.c_str(), nullptr, nullptr, buffer.data(), &size);
		if (res != ERROR_SUCCESS)
			return std::error_code(res, std::system_category());

		return {};
	}
};

} // namespace reg

#endif

#endif