//
// FFGLPluginInfoData.cpp
//
// Usually you do not need to edit this file!
//

#include "FFGLPluginInfo.h"
#include "NosuchDebug.h"

//////////////////////////////////////////////////////////////////
// Information about the plugin
//////////////////////////////////////////////////////////////////

CFFGLPluginInfo* g_CurrPluginInfo = NULL;

//////////////////////////////////////////////////////////////////
// Plugin dll entry point
//////////////////////////////////////////////////////////////////
#ifdef _WIN32
BOOL APIENTRY DllMain(HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	char dllpath[MAX_PATH];
	GetModuleFileNameA((HMODULE)hModule, dllpath, MAX_PATH);

	if (ul_reason_for_call == DLL_PROCESS_ATTACH ) {
		bool pyffle_setdll(std::string dllpath);
		// Initialize once for each new process.
		// Return FALSE if we fail to load DLL.
		if ( ! pyffle_setdll(std::string(dllpath)) ) {
			NosuchDebug("pyffle_setdll failed");
			return FALSE;
		}
		NosuchDebug("DLL_PROCESS_ATTACH dll=%s",dllpath);
	}
	if (ul_reason_for_call == DLL_PROCESS_DETACH ) {
		NosuchDebug("DLL_PROCESS_DETACH dll=%s",dllpath);
	}
	if (ul_reason_for_call == DLL_THREAD_ATTACH ) {
		NosuchDebug("DLL_THREAD_ATTACH dll=%s",dllpath);
	}
	if (ul_reason_for_call == DLL_THREAD_DETACH ) {
		NosuchDebug("DLL_THREAD_DETACH dll=%s",dllpath);
	}
    return TRUE;
}




#endif
