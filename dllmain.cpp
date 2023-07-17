// dllmain.cpp : Defines the entry point for the DLL application.
#include <windows.h>
#include <ocular.h>
#include "tether.h"
#include "celestial.h"

FILE* stream;

#define LOG_CONSOLE
//#define LOG_FILE

void ConsoleSetup()
{
#if defined(LOG_CONSOLE)
	AllocConsole();
	freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
	freopen_s((FILE**)stdout, "CONOUT$", "w", stderr);
	freopen_s((FILE**)stdout, "CONOUT$", "r", stdin);
#elif defined(LOG_FILE)
	freopen_s(&stream, "log.txt", "w", stdout);
	freopen_s(&stream, "log.txt", "w", stderr);
	freopen_s(&stream, "log.txt", "w", stdin);
#endif
}

DWORD WINAPI MainThread(LPVOID lpReserved)
{
	ConsoleSetup();

	celestial::Init();

	ocular::Init();
	ocular::CreateHook(celestial::hkPresent, ocular::VMT::Present);
	ocular::CreateHook(celestial::hkResizeBuffers, ocular::VMT::ResizeBuffers);
	ocular::CreateHook(celestial::hkOMSetRenderTargets, ocular::VMT::OMSetRenderTargets);

	celestial::MainLoop();

	return TRUE;
}

BOOL WINAPI DllMain(HMODULE hMod, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hMod);
		CreateThread(nullptr, 0, MainThread, hMod, 0, nullptr);
		Sleep(1000);
		break;
	case DLL_PROCESS_DETACH:
		fflush(stdout);
		if (stream) fclose(stream);
		break;
	}
	return TRUE;
}