/*

#pragma comment(lib, "Msimg32.lib")


// Define GET_X_LPARAM and GET_Y_LPARAM if not available
#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif

#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif


int WINAPI WinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR lpCmdLine,
	_In_ int nShowCmd)
{

c:\\background.png
*/




// 1. Injector (Console App)
// Compile this as a console application (injector.exe):
/*

#include <windows.h>
#include <tlhelp32.h>
#include <iostream>

DWORD GetProcessID(const wchar_t* processName) {
	DWORD processID = 0;
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapshot != INVALID_HANDLE_VALUE) {
		PROCESSENTRY32 pe;
		pe.dwSize = sizeof(PROCESSENTRY32);
		if (Process32First(hSnapshot, &pe)) {
			do {
				if (!_wcsicmp(pe.szExeFile, processName)) {
					processID = pe.th32ProcessID;
					break;
				}
			} while (Process32Next(hSnapshot, &pe));
		}
	}
	CloseHandle(hSnapshot);
	return processID;
}

BOOL InjectDLL(DWORD dwProcessId, const char* dllPath) {
	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwProcessId);
	if (!hProcess) return FALSE;

	LPVOID pRemoteMemory = VirtualAllocEx(hProcess, NULL, strlen(dllPath) + 1, MEM_COMMIT, PAGE_READWRITE);
	if (!pRemoteMemory) {
		CloseHandle(hProcess);
		return FALSE;
	}

	WriteProcessMemory(hProcess, pRemoteMemory, dllPath, strlen(dllPath) + 1, NULL);
	HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0,
		(LPTHREAD_START_ROUTINE)LoadLibraryA, pRemoteMemory, 0, NULL);

	if (!hThread) {
		VirtualFreeEx(hProcess, pRemoteMemory, 0, MEM_RELEASE);
		CloseHandle(hProcess);
		return FALSE;
	}

	WaitForSingleObject(hThread, INFINITE);
	VirtualFreeEx(hProcess, pRemoteMemory, 0, MEM_RELEASE);
	CloseHandle(hThread);
	CloseHandle(hProcess);
	return TRUE;
}

int main() {
	const wchar_t* targetProcess = L"notepad.exe"; // Change to your target process
	const char* dllPath = "C:\\Path\\To\\YourDLL.dll"; // Change to your DLL path

	DWORD processID = GetProcessID(targetProcess);
	if (processID == 0) {
		std::cout << "Target process not found!" << std::endl;
		return 1;
	}

	if (InjectDLL(processID, dllPath)) {
		std::cout << "DLL Injected Successfully!" << std::endl;
	} else {
		std::cout << "DLL Injection Failed!" << std::endl;
	}

	return 0;
}

*/




// 2. DLL(Injected Library)
// Compile this as a DLL(YourDLL.dll):
/*

#include <windows.h>

void HideFromTaskbar(HWND hWnd) {
	LONG exStyle = GetWindowLong(hWnd, GWL_EXSTYLE);
	exStyle &= ~WS_EX_APPWINDOW;
	exStyle |= WS_EX_TOOLWINDOW;
	SetWindowLong(hWnd, GWL_EXSTYLE, exStyle);

	ShowWindow(hWnd, SW_HIDE);
	ShowWindow(hWnd, SW_SHOW);
}

DWORD WINAPI MyThread(LPVOID param) {
	Sleep(1000); // Give process time to create its window
	HWND hWnd = FindWindow(NULL, L"Untitled - Notepad"); // Change title if needed

	if (hWnd) {
		HideFromTaskbar(hWnd);
	}

	return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved) {
	if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
		CreateThread(NULL, 0, MyThread, NULL, 0, NULL);
	}
	return TRUE;
}

*/