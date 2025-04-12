#include "Registry.h"


BOOL ReadRegistry(BOOL forAllUsers, LPCTSTR subKey, LPCTSTR valueName, PVOID data)
{
	HKEY hKey{};
	HKEY hRootKey = forAllUsers ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
	DWORD dataSize{};
	DWORD dataType{ REG_NONE };

	// Open the registry key
	LONG result = RegOpenKeyEx(
		hRootKey,
		subKey,
		0,
		KEY_READ,
		&hKey
	);
	if (result != ERROR_SUCCESS) {
		return FALSE; // Key doesn't exist or access denied
	}

	// First, query the regData to get the data type and size
	result = RegQueryValueEx(
		hKey,
		valueName,
		NULL,
		&dataType,
		NULL,
		&dataSize
	);
	if (result != ERROR_SUCCESS) {
		if (result == ERROR_FILE_NOT_FOUND) {
			// Value doesn't exist - no error message needed
		}
		else {
			MessageBox(NULL, _T("Error reading registry value."), _T("Error"), MB_OK | MB_ICONERROR);
		}
		RegCloseKey(hKey);
		return FALSE;
	}

	// Handle REG_DWORD dataType
	if (dataType == REG_DWORD) {
		result = RegQueryValueEx(hKey, valueName, NULL, &dataType, reinterpret_cast<LPBYTE>(data), &dataSize);
	}
	// Handle REG_SZ (string) dataType
	else if (dataType == REG_SZ) {
		result = RegQueryValueEx(hKey, valueName, NULL, NULL, reinterpret_cast<LPBYTE>(data), &dataSize);
	}
	else {
		MessageBox(NULL, _T("Unsupported registry dataType."), _T("Type Error"), MB_OK | MB_ICONERROR);
		RegCloseKey(hKey);
		return FALSE;
	}

	// Close the registry key
	RegCloseKey(hKey);
	return (result == ERROR_SUCCESS);
}

BOOL WriteRegistry(BOOL forAllUsers, LPCTSTR subKey, LPCTSTR valueName, PVOID data, DWORD dataType)
{
	HKEY hKey = NULL;
	HKEY hRootKey = forAllUsers ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
	SIZE_T dataSize{};

	// Create or open the registry key
	LONG result = RegCreateKeyEx(
		hRootKey,
		subKey,
		0,
		NULL,
		REG_OPTION_NON_VOLATILE,
		KEY_WRITE,
		NULL,
		&hKey,
		NULL
	);
	if (result != ERROR_SUCCESS) {
		MessageBox(NULL, _T("Failed to open or create registry key."), _T("Error"), MB_OK | MB_ICONERROR);
		return FALSE;
	}
	if (dataType == REG_DWORD) {
		dataSize = sizeof(DWORD);
	}
	else if (dataType == REG_SZ) {
		// Calculate the size of the string including the null terminator
		dataSize = (_tcslen(reinterpret_cast<LPCTSTR>(data)) + 1) * sizeof(TCHAR);
	}
	else {
		MessageBox(NULL, _T("Unsupported data type."), _T("Error"), MB_OK | MB_ICONERROR);
		RegCloseKey(hKey);
		return FALSE;
	}

	// Write the data to the registry
	result = RegSetValueEx(
		hKey,
		valueName,
		0,
		dataType,
		reinterpret_cast<const PBYTE>(data),
		dataSize
	);

	// Close the registry key
	RegCloseKey(hKey);
	return (result == ERROR_SUCCESS);
}

BOOL RemoveRegistry(BOOL forAllUsers, LPCTSTR subKey, LPCTSTR valueName)
{
	HKEY hKey;
	HKEY rootKey = forAllUsers ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

	// Open the registry key
	if (RegOpenKeyEx(rootKey, subKey, 0, KEY_WRITE, &hKey) != ERROR_SUCCESS) {
		return FALSE;
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




