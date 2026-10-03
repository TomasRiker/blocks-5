#include "pch.h"
#include "touchkeyboard.h"

#ifdef _WIN32

#include <windows.h>
#include <objbase.h>
#include <SDL_syswm.h>

#ifndef SM_CONVERTIBLESLATEMODE
#define SM_CONVERTIBLESLATEMODE 0x2003
#endif

namespace
{
	// The two interfaces InputPane is reached through, declared here rather
	// than taken from inputpaneinterop.h and windows.ui.viewmanagement.h,
	// which mingw lacks: IInspectable's three methods first, then each one's
	// own, in the order of the SDK's IDL (and Wine's).
	struct InspectableLayout : public IUnknown
	{
		virtual HRESULT STDMETHODCALLTYPE GetIids(ULONG* p_count, IID** pp_iids) = 0;
		virtual HRESULT STDMETHODCALLTYPE GetRuntimeClassName(void** p_className) = 0;
		virtual HRESULT STDMETHODCALLTYPE GetTrustLevel(int* p_trustLevel) = 0;
	};

	struct InputPaneInterop : public InspectableLayout
	{
		virtual HRESULT STDMETHODCALLTYPE GetForWindow(HWND window, REFIID riid, void** pp_inputPane) = 0;
	};

	// TryShow and TryHide answer a WinRT boolean, one byte.
	struct InputPane2 : public InspectableLayout
	{
		virtual HRESULT STDMETHODCALLTYPE TryShow(unsigned char* p_result) = 0;
		virtual HRESULT STDMETHODCALLTYPE TryHide(unsigned char* p_result) = 0;
	};

	const IID IID_INPUT_PANE_INTEROP = { 0x75cf2c57, 0x9195, 0x4931, { 0x83, 0x32, 0xf0, 0xb4, 0x09, 0xe9, 0x16, 0xaf } };
	const IID IID_INPUT_PANE_2 = { 0x8a6b3f26, 0x7090, 0x4793, { 0x94, 0x4c, 0xc3, 0xf2, 0xcd, 0xe2, 0x62, 0x76 } };

	// combase.dll's, loaded rather than linked: Windows 7 has none of it, and
	// there the touch keyboard is simply never asked for. An HSTRING is a
	// handle, passed through untouched.
	typedef HRESULT (WINAPI* RoGetActivationFactoryFunction)(void* classId, REFIID iid, void** pp_factory);
	typedef HRESULT (WINAPI* WindowsCreateStringFunction)(const wchar_t* p_source, UINT32 length, void** p_string);
	typedef HRESULT (WINAPI* WindowsDeleteStringFunction)(void* string);

	InputPane2* p_inputPane = 0;
	bool inputPaneTried = false;

	// The InputPane of the game's window, made once: 0 before Windows 10,
	// without COM on this thread, or without a window.
	InputPane2* inputPane()
	{
		if(inputPaneTried) return p_inputPane;
		inputPaneTried = true;

		HMODULE combase = LoadLibraryA("combase.dll");
		if(!combase) return 0;
		const RoGetActivationFactoryFunction p_getFactory = reinterpret_cast<RoGetActivationFactoryFunction>(
			reinterpret_cast<void*>(GetProcAddress(combase, "RoGetActivationFactory")));
		const WindowsCreateStringFunction p_createString = reinterpret_cast<WindowsCreateStringFunction>(
			reinterpret_cast<void*>(GetProcAddress(combase, "WindowsCreateString")));
		const WindowsDeleteStringFunction p_deleteString = reinterpret_cast<WindowsDeleteStringFunction>(
			reinterpret_cast<void*>(GetProcAddress(combase, "WindowsDeleteString")));
		if(!p_getFactory || !p_createString || !p_deleteString) return 0;

		SDL_SysWMinfo info;
		SDL_VERSION(&info.version);
		if(!SDL_GetWMInfo(&info) || !info.window) return 0;

		// The InputPane belongs to the window's thread, which is this one and
		// a single-threaded apartment. Already one, it says so and that is
		// fine; never uninitialized, since it lasts as long as the window.
		CoInitializeEx(0, COINIT_APARTMENTTHREADED);

		static const wchar_t className[] = L"Windows.UI.ViewManagement.InputPane";
		void* classId = 0;
		if(FAILED(p_createString(className, static_cast<UINT32>(sizeof(className) / sizeof(className[0]) - 1), &classId))) return 0;
		InputPaneInterop* p_interop = 0;
		const HRESULT hr = p_getFactory(classId, IID_INPUT_PANE_INTEROP, reinterpret_cast<void**>(&p_interop));
		p_deleteString(classId);
		if(FAILED(hr) || !p_interop) return 0;

		if(FAILED(p_interop->GetForWindow(info.window, IID_INPUT_PANE_2, reinterpret_cast<void**>(&p_inputPane)))) p_inputPane = 0;
		p_interop->Release();
		return p_inputPane;
	}

	// What happened, logged when it differs from the last time: the log is
	// the one place a tablet tells why no keyboard came.
	void logOutcome(const std::string& outcome)
	{
		static std::string last;
		if(outcome != last) printfLog("Touch keyboard: %s.\n", outcome.c_str());
		last = outcome;
	}
}

void TouchKeyboard::show()
{
	// In tablet posture only - folded, or with the keyboard taken off - as
	// Windows opens it for its own fields: over a keyboard that is there to
	// type on it would only cover the game. The metric is 0 in that posture.
	if(GetSystemMetrics(SM_CONVERTIBLESLATEMODE) != 0)
	{
		logOutcome("not in tablet posture, so not shown");
		return;
	}

	InputPane2* p_pane = inputPane();
	if(!p_pane)
	{
		logOutcome("no InputPane for the window, so not shown");
		return;
	}

	unsigned char shown = 0;
	p_pane->TryShow(&shown);
	logOutcome(shown ? "shown" : "Windows refused to show it");
}

void TouchKeyboard::hide()
{
	// Only through a pane that already exists: making one to hide nothing
	// would load WinRT for a player who never touched a field.
	if(!p_inputPane) return;
	unsigned char hidden = 0;
	p_inputPane->TryHide(&hidden);
}

#else

void TouchKeyboard::show()
{
}

void TouchKeyboard::hide()
{
}

#endif
