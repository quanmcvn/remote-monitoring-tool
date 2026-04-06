#include "common/registry.hpp"

#ifdef _WIN32
namespace reg {

std::error_code Key::set_wstring(const std::wstring& name, const std::wstring& value) {

	return setRaw(name, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
	              static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
}

std::error_code Key::set_dword(const std::wstring& name, const DWORD& value) {
	return setRaw(name, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));
}

std::error_code Key::set_qword(const std::wstring& name, const ULONGLONG& value) {
	return setRaw(name, REG_QWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));
}

std::optional<std::wstring> Key::get_wstring(const std::wstring& name, std::error_code& ec) const {
	DWORD type;
	std::vector<BYTE> buffer;

	ec = queryRaw(name, type, buffer);
	if (ec || (type != REG_SZ && type != REG_EXPAND_SZ))
		return std::nullopt;

	if (buffer.empty())
		return std::wstring();

	const wchar_t* data = reinterpret_cast<const wchar_t*>(buffer.data());
	size_t len = buffer.size() / sizeof(wchar_t);

	if (len > 0 && data[len - 1] == L'\0')
		--len;

	return std::wstring(data, len);
}

std::optional<DWORD> Key::get_dword(const std::wstring& name, std::error_code& ec) const {
	DWORD type;
	std::vector<BYTE> buffer;

	ec = queryRaw(name, type, buffer);
	if (ec || type != REG_DWORD || buffer.size() != sizeof(DWORD))
		return std::nullopt;

	DWORD value;
	std::memcpy(&value, buffer.data(), sizeof(DWORD));
	return value;
}

std::optional<ULONGLONG> Key::get_qword(const std::wstring& name, std::error_code& ec) const {
	DWORD type;
	std::vector<BYTE> buffer;

	ec = queryRaw(name, type, buffer);
	if (ec || type != REG_QWORD || buffer.size() != sizeof(ULONGLONG))
		return std::nullopt;

	ULONGLONG value;
	std::memcpy(&value, buffer.data(), sizeof(ULONGLONG));
	return value;
}
} // namespace reg

#endif