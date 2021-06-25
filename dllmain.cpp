// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include <Windows.h>
#include <iostream>
#include <shellapi.h>

using drag_end_callback = void(__stdcall* )(int len, WCHAR** result);

#define UNITY_WINDOW_CLASS_NAME L"UnityWndClass"
#define MAX_CLASS_NAME_LENGTH 256
#define MAX_PATH_LENGTH 1024

HWND main_window = nullptr;
HHOOK hook;
WCHAR** result;

drag_end_callback callback;

BOOL APIENTRY DllMain(HMODULE hModule,
                      DWORD ul_reason_for_call,
                      LPVOID lpReserved
)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
#if _DEBUG
		AllocConsole();

		FILE* file;
		freopen_s(&file, "CONIN$", "r", stdin);
		freopen_s(&file, "CONOUT$", "w", stdout);
		freopen_s(&file, "CONOUT$", "w", stderr);
#endif
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	default: 
		break;
	}
	
	return TRUE;
}

BOOL CALLBACK enum_callback(const HWND hwnd, LPARAM lParam)
{
	// skip invisible window
	if (!IsWindowVisible(hwnd)) 
		return TRUE;

	// skip when we already have main window handle
	if (main_window == nullptr)
	{
		main_window = hwnd;
	}
	else
	{
		// FindWindow searches all unity window classess of all instanced of unity.
		// So, use sub window enumeration in current thread instead FindWindow
		WCHAR lp_class_name[MAX_CLASS_NAME_LENGTH];
		GetClassName(hwnd, lp_class_name, MAX_CLASS_NAME_LENGTH);
		if (wcscmp(lp_class_name, UNITY_WINDOW_CLASS_NAME) == 0)
		{
			main_window = hwnd;
		}
	}

	return TRUE;
}

LRESULT windows_hook_callback(int code, WPARAM wParam, LPARAM lParam)
{
	if (code == WH_JOURNALRECORD)
	{
		const auto msg = reinterpret_cast<PMSG>(lParam);
		if (msg->message == WM_DROPFILES)
		{
			POINT pos;
			const auto h_drop = reinterpret_cast<HDROP>(msg->wParam);

			DragQueryPoint(h_drop, &pos);

			const auto n = static_cast<int>(DragQueryFile(h_drop, 0xFFFFFFFF, nullptr, 0));

			result = new WCHAR*[static_cast<int>(n)];

			for (int i = 0; i < n; i++)
			{
				const auto path = new WCHAR[MAX_PATH_LENGTH];
				int len = DragQueryFile(h_drop, i, path, MAX_PATH_LENGTH);

				result[i] = path;
			}

			DragFinish(h_drop);

#if _DEBUG
			for (int i = 0; i < n; i++)
			{
				std::wcout << result[i] << std::endl;
			}
#endif

			callback(static_cast<int>(n), result);
			delete[] result;
		}
	}

	return CallNextHookEx(hook, code, wParam, lParam);
}

extern "C" __declspec(dllexport) void AddHook(
	drag_end_callback cb
)
{
	callback = cb;

	const auto tid = GetCurrentThreadId();
	if (tid > 0)
	{
		EnumThreadWindows(tid, enum_callback, 0);
	}

	const auto h_module = GetModuleHandle(nullptr);

	hook = SetWindowsHookEx(3, windows_hook_callback, h_module, tid);
	DragAcceptFiles(main_window, true);
}

extern "C" __declspec(dllexport) void RemoveHook()
{
	UnhookWindowsHookEx(hook);
	DragAcceptFiles(main_window, false);

	hook = nullptr;
	callback = nullptr;
}
