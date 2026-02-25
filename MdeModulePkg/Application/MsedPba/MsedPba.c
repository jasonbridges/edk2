/** @file
  Main entry point for MsedPba application.

  Copyright (c) 2014-2015 Michael Romeo <r0m30@r0m30.com>
  Copyright (c) 2024, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include <Protocol/StorageSecurityCommand.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DiskInfo.h>

#include "UnlockOpal.h"

//
// 426E7754-D81B-49EA-85AD-69EAA7B1539C
//
EFI_GUID gMsedPbaConfigGuid = { 0x426E7754, 0xD81B, 0x49EA, { 0x85, 0xAD, 0x69, 0xEA, 0xA7, 0xB1, 0x53, 0x9C } };

//
// NVMe Identify Controller Data Structure (partial)
//
typedef struct {
  UINT16  Vid;
  UINT16  Ssvid;
  CHAR8   Sn[20];
  CHAR8   Mn[40];
  CHAR8   Fr[8];
  // ...
} NVME_ADMIN_CONTROLLER_DATA;

//
// ATA Identify Data Structure (partial)
//
typedef struct {
  UINT16  GeneralConfiguration;
  UINT16  Reserved1[9];
  CHAR8   SerialNo[20];
  UINT16  Reserved2[3];
  CHAR8   FirmwareRevision[8];
  CHAR8   ModelName[40];
  // ...
} ATA_IDENTIFY_DATA;

VOID
SwapAtaString (
  IN CHAR8 *Buffer,
  IN UINTN Length
  )
{
  UINTN Index;
  CHAR8 Temp;

  for (Index = 0; Index < Length; Index += 2) {
    Temp = Buffer[Index];
    Buffer[Index] = Buffer[Index + 1];
    Buffer[Index + 1] = Temp;
  }
}

EFI_STATUS
GetDriveInfo (
  IN EFI_HANDLE          Handle,
  OUT OPAL_DiskInfo      *DiskInfo
  )
{
  EFI_STATUS                  Status;
  EFI_DISK_INFO_PROTOCOL      *DiskInfoProtocol;
  EFI_GUID                    Interface;
  UINT32                      BufferSize;
  VOID                        *Buffer;
  ATA_IDENTIFY_DATA           *AtaData;
  NVME_ADMIN_CONTROLLER_DATA  *NvmeData;

  Status = gBS->HandleProtocol (
                  Handle,
                  &gEfiDiskInfoProtocolGuid,
                  (VOID **)&DiskInfoProtocol
                  );
  if (EFI_ERROR(Status)) {
    return Status;
  }

  CopyGuid(&Interface, &DiskInfoProtocol->Interface);

  BufferSize = 4096;
  Buffer = AllocatePool(BufferSize);
  if (Buffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = DiskInfoProtocol->Identify(DiskInfoProtocol, &BufferSize, Buffer);
  if (EFI_ERROR(Status)) {
    FreePool(Buffer);
    return Status;
  }

  if (CompareGuid(&Interface, &gEfiDiskInfoIdeInterfaceGuid) ||
      CompareGuid(&Interface, &gEfiDiskInfoAhciInterfaceGuid)) {
    // ATA
    AtaData = (ATA_IDENTIFY_DATA *)Buffer;
    DiskInfo->devType = 0; // ATA

    CopyMem(DiskInfo->serialNum, AtaData->SerialNo, 20);
    SwapAtaString((CHAR8 *)DiskInfo->serialNum, 20);

    CopyMem(DiskInfo->modelNum, AtaData->ModelName, 40);
    SwapAtaString((CHAR8 *)DiskInfo->modelNum, 40);

    CopyMem(DiskInfo->firmwareRev, AtaData->FirmwareRevision, 8);
    SwapAtaString((CHAR8 *)DiskInfo->firmwareRev, 8);

  } else if (CompareGuid(&Interface, &gEfiDiskInfoNvmeInterfaceGuid)) {
    // NVMe
    NvmeData = (NVME_ADMIN_CONTROLLER_DATA *)Buffer;
    DiskInfo->devType = 1; // Not strictly ATA, but let's say 1

    // NVMe strings are not swapped.
    // Note: NVMe SN is space padded 20 bytes.
    // msedpba assumes the serial number bytes are used "as is" for the salt.
    // For ATA, msedpba swapped them.
    // For NVMe, they are already in correct order (BE/LE issues? No, char array).
    CopyMem(DiskInfo->serialNum, NvmeData->Sn, 20);
    CopyMem(DiskInfo->modelNum, NvmeData->Mn, 40);
    CopyMem(DiskInfo->firmwareRev, NvmeData->Fr, 8);
  } else {
    // Unknown
    FreePool(Buffer);
    return EFI_UNSUPPORTED;
  }

  FreePool(Buffer);
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                        Status;
  UINTN                             HandleCount;
  EFI_HANDLE                        *HandleBuffer;
  UINTN                             Index;
  EFI_STORAGE_SECURITY_COMMAND_PROTOCOL *Ssp;
  EFI_BLOCK_IO_PROTOCOL             *BlockIo;
  OPAL_DiskInfo                     DiskInfo;
  CHAR16                            InputBuffer[256];
  CHAR8                             PassBuffer[256];
  UINTN                             InputIndex;
  EFI_INPUT_KEY                     Key;

  Print(L"MsedPba Ported to UEFI\n");

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiStorageSecurityCommandProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );
  if (EFI_ERROR(Status)) {
    Print(L"No Storage Security Command Protocol handles found.\n");
    return Status;
  }

  Print(L"Found %d handles.\n", HandleCount);

  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol (
                    HandleBuffer[Index],
                    &gEfiStorageSecurityCommandProtocolGuid,
                    (VOID **)&Ssp
                    );
    if (EFI_ERROR(Status)) continue;

    // Get BlockIO for MediaId
    Status = gBS->HandleProtocol (
                    HandleBuffer[Index],
                    &gEfiBlockIoProtocolGuid,
                    (VOID **)&BlockIo
                    );
    if (EFI_ERROR(Status)) {
      Print(L"Handle %d has no BlockIo\n", Index);
      continue;
    }

    ZeroMem(&DiskInfo, sizeof(DiskInfo));

    // Attempt to get drive info (Serial Number)
    Status = GetDriveInfo(HandleBuffer[Index], &DiskInfo);
    if (EFI_ERROR(Status)) {
      Print(L"Handle %d: Failed to get drive info: %r. Skipping.\n", Index, Status);
      continue;
    }

    Print(L"Handle %d: Serial: %a Model: %a\n", Index, DiskInfo.serialNum, DiskInfo.modelNum);

    // Perform Discovery0
    Discovery0(Ssp, BlockIo->Media->MediaId, &DiskInfo);

    if (DiskInfo.OPAL20 || DiskInfo.OPAL10) {
      Print(L"  OPAL Drive detected!\n");
      if (!DiskInfo.Locking_locked) {
        Print(L"  Drive is NOT locked.\n");
        // continue; // Optional: still offer to unlock/revert? msedpba checks !locked && !MBREnabled and continues loop.
        if (!DiskInfo.Locking_MBREnabled) {
           Print(L"  Locking not enabled. Skipping.\n");
           continue;
        }
      }

      //
      // Check if password variable exists
      //
      UINTN PassSize = sizeof(PassBuffer) - 1;
      Status = gRT->GetVariable(
                      L"MsedPbaPassword",
                      &gMsedPbaConfigGuid,
                      NULL,
                      &PassSize,
                      PassBuffer
                      );

      if (!EFI_ERROR(Status) && PassSize > 0) {
        PassBuffer[PassSize] = 0; // Ensure null termination
        Print(L"  Using MsedPbaPassword variable.\n");
      } else {
        Print(L"Enter Password: ");
        // Simple input loop
        InputIndex = 0;
        ZeroMem(InputBuffer, sizeof(InputBuffer));
        ZeroMem(PassBuffer, sizeof(PassBuffer));

        while (InputIndex < 255) {
          gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &Index);
          Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);
          if (EFI_ERROR(Status)) continue;

          if (Key.UnicodeChar == '\r') {
            Print(L"\n");
            break;
          } else if (Key.UnicodeChar == '\b') {
            if (InputIndex > 0) {
              InputIndex--;
              InputBuffer[InputIndex] = 0;
              Print(L"\b \b");
            }
          } else if (Key.UnicodeChar >= 0x20 && Key.UnicodeChar < 0x7F) {
            InputBuffer[InputIndex++] = Key.UnicodeChar;
            Print(L"*"); // Echo *
          }
        }
        InputBuffer[InputIndex] = 0;

        // Convert to ASCII
        UnicodeStrToAsciiStrS(InputBuffer, PassBuffer, sizeof(PassBuffer));
      }

      Status = UnlockOpal(Ssp, BlockIo->Media->MediaId, PassBuffer, &DiskInfo);
      if (!EFI_ERROR(Status)) {
        Print(L"  Unlocked successfully!\n");
      } else {
        Print(L"  Unlock failed: %r\n", Status);
      }

      // Sanitize memory
      ZeroMem(InputBuffer, sizeof(InputBuffer));
      ZeroMem(PassBuffer, sizeof(PassBuffer));

    } else {
      Print(L"  Not an OPAL drive.\n");
    }
  }

  FreePool(HandleBuffer);
  return EFI_SUCCESS;
}
