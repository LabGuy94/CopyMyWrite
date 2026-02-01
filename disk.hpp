#include "utils.hpp"
#include <virtdisk.h>
#include <winioctl.h>
#pragma comment(lib, "virtdisk.lib")
#define VHD_PATH L"C:\\vhd.vhd"
namespace disk
{
    static HANDLE vhdHandle;
    static HANDLE deviceHandle;
    bool Cleanup()
    {
        if (vhdHandle == INVALID_HANDLE_VALUE) {
            Log("VHD handle is invalid.\n");
            return false;
        }
        HRESULT result = DetachVirtualDisk(vhdHandle, DETACH_VIRTUAL_DISK_FLAG_NONE, 0);
        if (result != ERROR_SUCCESS) {
            Log("Failed to detach VHD: %lu\n", result);
            return false;
        }
        else {
            Log("VHD detached.\n");
        }

        CloseHandle(vhdHandle);

        if (DeleteFileW(VHD_PATH)) {
            Log("VHD file deleted.\n");
        }
        else {
            Log("Failed to delete VHD file.\n");
            return false;
        }
        return true;
    }

    // https://github.com/cutecatsandvirtualmachines/DDMA
    bool ScsiIssueCommand(UCHAR operationCode, PVOID buffer, ULONG dataTransferLength = PAGE_SIZE)
    {
        struct SPTDBuffer
        {
            SCSI_PASS_THROUGH_DIRECT cmd;
            UINT8 sense[20];
        };
        if (!deviceHandle)
            return false;
        SPTDBuffer Srb{ };
        memset(&Srb, 0, sizeof(Srb));
        Srb.cmd.Length = sizeof(Srb.cmd);
        Srb.cmd.CdbLength = 10;
        Srb.cmd.SenseInfoLength = sizeof(Srb.sense);
        Srb.cmd.SenseInfoOffset = sizeof(Srb.cmd);
        Srb.cmd.DataTransferLength = dataTransferLength;
        Srb.cmd.TimeOutValue = 5;
        Srb.cmd.DataBuffer = buffer;

        Srb.cmd.Cdb[0] = operationCode;
        Srb.cmd.Cdb[1] = 0;

        SHORT CDBTLen = (SHORT)(dataTransferLength / SCSI_SECTOR_SIZE);
        Srb.cmd.Cdb[7] = ((LPCH)&CDBTLen)[1];
        Srb.cmd.Cdb[8] = ((LPCH)&CDBTLen)[0];
        Srb.cmd.Cdb[9] = 0x00;

        DWORD returned = 0;
        BOOL result = DeviceIoControl(
            deviceHandle,
            IOCTL_SCSI_PASS_THROUGH_DIRECT,
            &Srb,
            sizeof(Srb),
            0,
            0,
            &returned,
            NULL);
        if (!result) {
            Log("DeviceIoControl failed: " << GetLastError());
            return false;
        }
        return true;
    }

    bool DiskCopy(PVOID dest, PVOID src, SIZE_T size)
    {
        if (!deviceHandle)
            return false;

        bool status = ScsiIssueCommand(SCSIOP_WRITE, src, (ULONG)size);
        if (!status) {
            Log("Write command failed.\n");
            return status;
        }
        status = ScsiIssueCommand(SCSIOP_READ, dest, (ULONG)size);
        if (!status) {
            Log("Read command failed.\n");
            return status;
        }
        PVOID emptyBuffer = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        status = ScsiIssueCommand(SCSIOP_WRITE, emptyBuffer, (ULONG)size);
        if (!status) {
            Log("Zeroing command failed.\n");
            VirtualFree(emptyBuffer, 0, MEM_RELEASE);
            return status;
        }
        VirtualFree(emptyBuffer, 0, MEM_RELEASE);
        return status;

    }

    bool Initialize()
    {
        VIRTUAL_STORAGE_TYPE storageType = {
           VIRTUAL_STORAGE_TYPE_DEVICE_VHD,
           GUID_NULL
        };
        if (GetFileAttributesW(VHD_PATH) != INVALID_FILE_ATTRIBUTES) {
            HRESULT hr = OpenVirtualDisk(
                &storageType,
                VHD_PATH,
                VIRTUAL_DISK_ACCESS_DETACH,
                OPEN_VIRTUAL_DISK_FLAG_NONE,
                nullptr,
                &vhdHandle
            );

            DetachVirtualDisk(vhdHandle, DETACH_VIRTUAL_DISK_FLAG_NONE, 0);
            CloseHandle(vhdHandle);
            vhdHandle = INVALID_HANDLE_VALUE;

            if (!DeleteFileW(VHD_PATH)) {
                Log("Failed to delete existing VHD file.\n");
                return false;
            }
            else {
                Log("Existing VHD file deleted.\n");
            }
        }


        CREATE_VIRTUAL_DISK_PARAMETERS params{ };
        params.Version = CREATE_VIRTUAL_DISK_VERSION_1;
        params.Version1.MaximumSize = 3ull * 1024 * 1024; // 3MB
        params.Version1.BlockSizeInBytes = CREATE_VIRTUAL_DISK_PARAMETERS_DEFAULT_BLOCK_SIZE;
        params.Version1.SectorSizeInBytes = CREATE_VIRTUAL_DISK_PARAMETERS_DEFAULT_SECTOR_SIZE;
        params.Version1.ParentPath = NULL;

        HRESULT hr = CreateVirtualDisk(
            &storageType,
            VHD_PATH,
            VIRTUAL_DISK_ACCESS_ALL,
            NULL,
            CREATE_VIRTUAL_DISK_FLAG_NONE,
            0,
            &params,
            NULL,
            &vhdHandle
        );

        if (hr != 0) return false;
        ATTACH_VIRTUAL_DISK_PARAMETERS attachParams{ };
        attachParams.Version = ATTACH_VIRTUAL_DISK_VERSION_1;

        hr = AttachVirtualDisk(
            vhdHandle,
            NULL,
            ATTACH_VIRTUAL_DISK_FLAG_PERMANENT_LIFETIME,
            0,
            &attachParams,
            NULL
        );

        WCHAR physicalPath[MAX_PATH];
        ULONG pathSize = ARRAYSIZE(physicalPath);

        hr = GetVirtualDiskPhysicalPath(vhdHandle, &pathSize, physicalPath);

        if (hr != 0) return false;

        deviceHandle = CreateFileW(
            physicalPath,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr);

        if (deviceHandle == INVALID_HANDLE_VALUE) return false;

        return true;
    }
}