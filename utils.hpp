#include "ntdef.h"
#include <iostream>
#include <psapi.h>
#include <string>  
#include <tlhelp32.h>


#define Log(msg) std::cout << std::dec << GetNameFromFullPath(__FILE__) << "(" << __LINE__ << ") " << __FUNCTION__ << "() " << msg << std::endl;
std::string GetNameFromFullPath(const std::string& dir)
{
    auto slashPos = dir.rfind('\\');
    if (slashPos == std::string::npos) {
        slashPos = dir.rfind('/');
    }
    if (slashPos == std::string::npos) {
        return dir;
    }
    return dir.substr(slashPos + 1);
}

bool isPageShared(PVOID address) {
	bool result = false;
	ULONG bufferSize = sizeof(PSAPI_WORKING_SET_INFORMATION);
	PPSAPI_WORKING_SET_INFORMATION pwsi = (PPSAPI_WORKING_SET_INFORMATION)malloc(bufferSize);
	HANDLE process = GetCurrentProcess();

	BOOL status = QueryWorkingSet(process, pwsi, bufferSize);
	if (!status && GetLastError() == ERROR_BAD_LENGTH)
	{
		bufferSize = sizeof(PSAPI_WORKING_SET_BLOCK) * pwsi->NumberOfEntries * 2 + sizeof(ULONG_PTR);
		free(pwsi);
		pwsi = (PPSAPI_WORKING_SET_INFORMATION)malloc(bufferSize);
		memset(pwsi, 0, bufferSize);

		status = QueryWorkingSet(process, pwsi, bufferSize);
	}

	for (ULONG i = 0; i < pwsi->NumberOfEntries; i++) {
		PSAPI_WORKING_SET_BLOCK wsb = pwsi->WorkingSetInfo[i];
		if ((UINT64)address >> 12 == wsb.VirtualPage)
		{
			result = wsb.Shared;
			break;
		}
	}

	free(pwsi);
	return result;
}

bool ProcessExists(DWORD pid) {
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
	if (hProcess == NULL) {
		if (GetLastError() == ERROR_INVALID_PARAMETER) {
			// Process does not exist
			return false;
		}
		return true;
	}

	CloseHandle(hProcess);
	return true;
}

DWORD GetProcessByName(const wchar_t* processName) {
	DWORD pid = 0;

	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE)
		return 0;

	PROCESSENTRY32W pe;
	pe.dwSize = sizeof(PROCESSENTRY32W);

	if (Process32FirstW(hSnapshot, &pe)) {
		do {
			if (wcscmp(processName, pe.szExeFile) == 0) {
				pid = pe.th32ProcessID;
				break;
			}
		} while (Process32NextW(hSnapshot, &pe));
	}

	CloseHandle(hSnapshot);
	return pid;
}
