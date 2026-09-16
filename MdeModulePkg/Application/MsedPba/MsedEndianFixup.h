/** @file
  Endianness fixups.

  Copyright (c) 2014-2015 Michael Romeo <r0m30@r0m30.com>
  Copyright (c) 2024, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

#include <Library/BaseLib.h>

#define SWAP16(x) SwapBytes16(x)
#define SWAP32(x) SwapBytes32(x)
#define SWAP64(x) SwapBytes64(x)
