#include <windows.h>
#include <tchar.h>
#include "Registry.h"


bool ReadRegistry(bool forAllUsers, const TCHAR* subKey, const TCHAR* valueName, void* data)
{
	HKEY hKey{};
	HKEY hRootKey = forAllUsers ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
	DWORD dataSize{};
	DWORD dataType{ REG_NONE };

	// Open the registry key.
	LONG result = RegOpenKeyEx(
		hRootKey,
		subKey,
		0,
		KEY_READ,
		&hKey
	);
	if (result != ERROR_SUCCESS) {
		return false; // Key doesn't exist or access denied
	}

	// First, query the regData to get the data type and size.
	result = RegQueryValueEx(
		hKey,
		valueName,
		nullptr,
		&dataType,
		nullptr,
		&dataSize
	);
	if (result != ERROR_SUCCESS) {
		if (result == ERROR_FILE_NOT_FOUND) {
			// Value doesn't exist - no error message needed
		}
		else {
			MessageBox(nullptr, _T("Error reading registry value."), _T("Error"), MB_OK | MB_ICONERROR);
		}
		RegCloseKey(hKey);
		return false;
	}

	// Handle REG_DWORD dataType.
	if (dataType == REG_DWORD) {
		result = RegQueryValueEx(hKey, valueName, nullptr, &dataType, reinterpret_cast<LPBYTE>(data), &dataSize);
	}
	// Handle REG_SZ (string) dataType.
	else if (dataType == REG_SZ) {
		result = RegQueryValueEx(hKey, valueName, nullptr, nullptr, reinterpret_cast<LPBYTE>(data), &dataSize);
	}
	else {
		MessageBox(nullptr, _T("Unsupported registry dataType."), _T("Type Error"), MB_OK | MB_ICONERROR);
		RegCloseKey(hKey);
		return false;
	}

	// Close the registry key
	RegCloseKey(hKey);
	return (result == ERROR_SUCCESS);
}

bool WriteRegistry(bool forAllUsers, const TCHAR* subKey, const TCHAR* valueName, void* data, DWORD dataType)
{
	HKEY hKey = nullptr;
	HKEY hRootKey = forAllUsers ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
	DWORD dataSize{};

	// Create or open the registry key.
	LONG result = RegCreateKeyEx(
		hRootKey,
		subKey,
		0,
		nullptr,
		REG_OPTION_NON_VOLATILE,
		KEY_WRITE,
		nullptr,
		&hKey,
		nullptr
	);
	if (result != ERROR_SUCCESS) {
		MessageBox(NULL, _T("Failed to open or create registry key."), _T("Error"), MB_OK | MB_ICONERROR);
		return false;
	}
	if (dataType == REG_DWORD) {
		dataSize = sizeof(DWORD);
	}
	else if (dataType == REG_SZ) {
		// Calculate the size of the string including the null terminator.
		dataSize = (_tcslen(reinterpret_cast<const TCHAR*>(data)) + 1) * sizeof(TCHAR);
	}
	else {
		MessageBox(NULL, _T("Unsupported data type."), _T("Error"), MB_OK | MB_ICONERROR);
		RegCloseKey(hKey);
		return false;
	}

	// Write the data to the registry.
	result = RegSetValueEx(
		hKey,
		valueName,
		0,
		dataType,
		reinterpret_cast<const BYTE*>(data),
		dataSize
	);

	// Close the registry key
	RegCloseKey(hKey);
	return (result == ERROR_SUCCESS);
}

bool RemoveRegistry(bool forAllUsers, const TCHAR* subKey, const TCHAR* valueName)
{
	HKEY hKey;
	HKEY rootKey = forAllUsers ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

	// Open the registry key
	if (RegOpenKeyEx(rootKey, subKey, 0, KEY_WRITE, &hKey) != ERROR_SUCCESS) {
		return false;
	}

	// Delete the value
	LONG result = RegDeleteValue(
		hKey,
		valueName
	);

	if (result != ERROR_SUCCESS) {
		MessageBox(NULL, _T("Failed to delete registry value!"), _T("Error"), MB_ICONERROR);
	}

	// Close the registry key
	RegCloseKey(hKey);
	return (result == ERROR_SUCCESS);
}



