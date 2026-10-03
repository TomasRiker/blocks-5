#include "pch.h"
#include "touchkeyboard.h"

#ifdef _WIN32

#include <windows.h>
#include <objbase.h>
#include <SDL_syswm.h>

#ifndef SM_CONVERTIBLESLATEMODE
#define SM_CONVERTIBLESLATEMODE 0x2003
#endif
#ifndef LOAD_LIBRARY_SEARCH_SYSTEM32
#define LOAD_LIBRARY_SEARCH_SYSTEM32 0x00000800
#endif

namespace
{
	// The two interfaces InputPane is reached through, declared here: mingw
	// has no inputpaneinterop.h, and its windows.ui.viewmanagement.h declares
	// no InputPane. IInspectable's three methods first, then each one's own,
	// in the order of the SDK's IDL (and Wine's).
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
	// user32's since Windows 8; its state is a set of AR_* bits.
	typedef BOOL (WINAPI* GetAutoRotationStateFunction)(int* p_state);
	const int ROTATION_NO_SENSOR = 0x10, ROTATION_NOT_SUPPORTED = 0x20, ROTATION_LAPTOP = 0x80;

	InputPane2* p_inputPane = 0;

	std::string withResult(const char* p_what, HRESULT hr)
	{
		char buffer[160];
		sprintf(buffer, "%s (0x%08lX), so not shown", p_what, static_cast<unsigned long>(hr));
		return buffer;
	}

	// The InputPane of the game's window, made the first time it can be: 0,
	// and why, before Windows 8, without a window or where WinRT refuses -
	// asked again at the next tap. combase.dll from the system's own folder
	// and from nowhere else, where it is not a known DLL.
	InputPane2* inputPane(std::string* p_why)
	{
		if(p_inputPane) return p_inputPane;

		static HMODULE combase = LoadLibraryExA("combase.dll", 0, LOAD_LIBRARY_SEARCH_SYSTEM32);
		const RoGetActivationFactoryFunction p_getFactory = combase ? reinterpret_cast<RoGetActivationFactoryFunction>(
			reinterpret_cast<void*>(GetProcAddress(combase, "RoGetActivationFactory"))) : 0;
		const WindowsCreateStringFunction p_createString = combase ? reinterpret_cast<WindowsCreateStringFunction>(
			reinterpret_cast<void*>(GetProcAddress(combase, "WindowsCreateString"))) : 0;
		const WindowsDeleteStringFunction p_deleteString = combase ? reinterpret_cast<WindowsDeleteStringFunction>(
			reinterpret_cast<void*>(GetProcAddress(combase, "WindowsDeleteString"))) : 0;
		if(!p_getFactory || !p_createString || !p_deleteString)
		{
			*p_why = "no WinRT in combase.dll (before Windows 8), so not shown";
			return 0;
		}

		SDL_SysWMinfo info;
		SDL_VERSION(&info.version);
		if(!SDL_GetWMInfo(&info) || !info.window)
		{
			*p_why = "no window to show it for, so not shown";
			return 0;
		}

		// The InputPane belongs to the window's thread, which is this one and
		// a single-threaded apartment. Already one, it says so and that is
		// fine; never uninitialized, since it lasts as long as the window.
		static bool comInitialized = false;
		if(!comInitialized) CoInitializeEx(0, COINIT_APARTMENTTHREADED);
		comInitialized = true;

		static const wchar_t className[] = L"Windows.UI.ViewManagement.InputPane";
		void* classId = 0;
		HRESULT hr = p_createString(className, static_cast<UINT32>(sizeof(className) / sizeof(className[0]) - 1), &classId);
		if(FAILED(hr))
		{
			*p_why = withResult("no string for the InputPane's class", hr);
			return 0;
		}
		InputPaneInterop* p_interop = 0;
		hr = p_getFactory(classId, IID_INPUT_PANE_INTEROP, reinterpret_cast<void**>(&p_interop));
		p_deleteString(classId);
		if(FAILED(hr) || !p_interop)
		{
			*p_why = withResult("the InputPane class could not be activated", hr);
			return 0;
		}

		hr = p_interop->GetForWindow(info.window, IID_INPUT_PANE_2, reinterpret_cast<void**>(&p_inputPane));
		p_interop->Release();
		if(FAILED(hr) || !p_inputPane)
		{
			p_inputPane = 0;
			*p_why = withResult("no InputPane for the window", hr);
			return 0;
		}
		return p_inputPane;
	}

	// Whether this is a tablet in tablet posture, as Windows judges it for
	// its own fields: folded, or with the keyboard taken off, which the
	// metric reports as 0 - but it applies to convertibles only, and may read
	// 0 on a desktop with a touch screen, so a screen that cannot turn is no
	// tablet whatever it says, as Chromium has it. The reason, where not.
	bool tabletPosture(std::string* p_why)
	{
		if(GetSystemMetrics(SM_CONVERTIBLESLATEMODE) != 0)
		{
			*p_why = "not in tablet posture, so not shown";
			return false;
		}
		const GetAutoRotationStateFunction p_rotation = reinterpret_cast<GetAutoRotationStateFunction>(
			reinterpret_cast<void*>(GetProcAddress(GetModuleHandleA("user32.dll"), "GetAutoRotationState")));
		int state = 0;
		if(!p_rotation || !p_rotation(&state)) return true;
		if(state & ROTATION_LAPTOP)
		{
			*p_why = "in laptop posture by the screen's rotation, so not shown";
			return false;
		}
		if(state & (ROTATION_NO_SENSOR | ROTATION_NOT_SUPPORTED))
		{
			*p_why = "the screen cannot turn, so no tablet and not shown";
			return false;
		}
		return true;
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
	// Over a keyboard that is there to type on it would only cover the game.
	std::string why;
	if(!tabletPosture(&why))
	{
		logOutcome(why);
		return;
	}

	InputPane2* p_pane = inputPane(&why);
	if(!p_pane)
	{
		logOutcome(why);
		return;
	}

	unsigned char shown = 0;
	const HRESULT hr = p_pane->TryShow(&shown);
	if(FAILED(hr)) logOutcome(withResult("TryShow failed", hr));
	else logOutcome(shown ? "shown" : "Windows refused to show it - its own touch keyboard setting?");
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
