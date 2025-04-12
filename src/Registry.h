#pragma once
#include <windows.h>
#include <tchar.h>




BOOL ReadRegistry(BOOL, LPCTSTR, LPCTSTR, PVOID);

BOOL WriteRegistry(BOOL, LPCTSTR, LPCTSTR, PVOID, DWORD);

BOOL RemoveRegistry(BOOL, LPCTSTR, LPCTSTR);



