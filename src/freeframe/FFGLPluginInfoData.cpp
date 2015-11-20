//
// FFGLPluginInfoData.cpp
//
// Usually you do not need to edit this file!
//

#include "FFGLPluginInfo.h"
#include "NosuchDebug.h"
#include <direct.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

//////////////////////////////////////////////////////////////////
// Information about the plugin
//////////////////////////////////////////////////////////////////

CFFGLPluginInfo* g_CurrPluginInfo = NULL;


//////////////////////////////////////////////////////////////////
// Plugin dll entry point
//////////////////////////////////////////////////////////////////

#ifdef _WIN32

extern "C" { bool ffgl_setdll(std::string dllpath); }

BOOL APIENTRY DllMain(HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	char dllpath[MAX_PATH];
	GetModuleFileNameA((HMODULE)hModule, dllpath, MAX_PATH);

	if (ul_reason_for_call == DLL_PROCESS_ATTACH ) {
		// Initialize once for each new process.
		// Return FALSE if we fail to load DLL.
		if ( ! ffgl_setdll(std::string(dllpath)) ) {
			NosuchDebug("ffgl_setdll failed");
			return FALSE;
		}
		NosuchDebug(1,"DLLPROCESS_ATTACH dll=%s",dllpath);
	}
	if (ul_reason_for_call == DLL_PROCESS_DETACH ) {
		NosuchDebug(1,"DLLPROCESS_DETACH dll=%s",dllpath);
	}
	if (ul_reason_for_call == DLL_THREAD_ATTACH ) {
		NosuchDebug(1,"DLLTHREAD_ATTACH dll=%s",dllpath);
	}
	if (ul_reason_for_call == DLL_THREAD_DETACH ) {
		NosuchDebug(1,"DLLTHREAD_DETACH dll=%s",dllpath);
	}
    return TRUE;
}
#endif
