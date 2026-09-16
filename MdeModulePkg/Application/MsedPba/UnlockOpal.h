/** @file
  Function prototypes for Opal unlocking.

  Copyright (c) 2014-2015 Michael Romeo <r0m30@r0m30.com>
  Copyright (c) 2024, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

#include <Uefi.h>
#include <Protocol/StorageSecurityCommand.h>
#include "MsedStructures.h"

#define IO_BUFFER_LENGTH 2048

VOID
Discovery0 (
  IN EFI_STORAGE_SECURITY_COMMAND_PROTOCOL *Ssp,
  IN UINT32                                MediaId,
  IN OUT OPAL_DiskInfo                     *DiskInfo
  );

EFI_STATUS
UnlockOpal (
  IN EFI_STORAGE_SECURITY_COMMAND_PROTOCOL *Ssp,
  IN UINT32                                MediaId,
  IN CHAR8                                 *Pass,
  IN OPAL_DiskInfo                         *DiskInfo
  );
