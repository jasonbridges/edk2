/** @file
  Opal unlocking logic.

  Copyright (c) 2014-2015 Michael Romeo <r0m30@r0m30.com>
  Copyright (c) 2024, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseCryptLib.h>
#include <Library/PrintLib.h>

#include "UnlockOpal.h"
#include "MsedEndianFixup.h"

//
// Command Templates
//
STATIC UINT8 StartSession[] = {
    0x00, 0x00, 0x00, 0x00, 0x07, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x58, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x69, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x34,
    0xf8,
    0xa8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff,
    0xa8, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x01,
    0xf0, 0xf2,
    0x04, 0x00, 0x00, 0x00, 0x09,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xf3,
    0xf2, 0x03, 0xa8,
    0x00, 0x00, 0x00, 0x09, 0x00, 0x01, 0x00, 0x01,
    0xf3,
    0xf1,
    0xf9,
    0xf0, 0x00, 0x00, 0x00, 0xf1,
};

STATIC UINT8 Unlock[] = {
    0x00, 0x00, 0x00, 0x00, 0x07, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x4c, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x69, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x34, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28,
    0xf8,
    0xa8, 0x00, 0x00, 0x08, 0x02, 0x00, 0x00, 0x00, 0x01,
    0xa8, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x17,
    0xf0, 0xf2, 0x01, 0xf0,
    0xf2, 0x07, 0x00, 0xf3,
    0xf2, 0x08, 0x00, 0xf3,
    0xf1, 0xf3, 0xf1,
    0xf9,
    0xf0, 0x00, 0x00, 0x00, 0xf1,
};

STATIC UINT8 MbrDone[] = {
    0x00, 0x00, 0x00, 0x00, 0x07, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x48, 0xff, 0xff, 0xfc, 0x52, 0x00, 0x00, 0x00, 0x69, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24,
    0xf8,
    0xa8, 0x00, 0x00, 0x08, 0x03, 0x00, 0x00, 0x00, 0x01,
    0xa8, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x17,
    0xf0, 0xf2, 0x01, 0xf0,
    0xf2, 0x02, 0x01, 0xf3,
    0xf1, 0xf3, 0xf1,
    0xf9,
    0xf0, 0x00, 0x00, 0x00, 0xf1,
};

STATIC UINT8 EndSession[] = {
    0x00, 0x00, 0x00, 0x00, 0x07, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x28, 0xff, 0xff, 0xfc, 0x01, 0x00, 0x00, 0x00, 0x69, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xfa, 0x00, 0x00, 0x00,
};

#pragma pack(push, 1)

typedef union _TCGINTDATA {
    UINT8 TCGUINT8;
    UINT16 TCGUINT16;
    UINT32 TCGUINT32;
} TCGINTDATA;

typedef struct _TCGUINT {
    UINT8 header;
    TCGINTDATA d;
} TCGUINT;

typedef struct _TCGSHORTCHAR {
    UINT8 header;
    CHAR8 bytes[];
} TCGSHORTCHAR;

typedef struct _TCGSMEDCHAR {
    UINT16 header;
    CHAR8 bytes[];
} TCGMEDCHAR;

typedef struct _TCGTOKEN {
    UINT8 header;
    CHAR8 bytes[];
} TCGTOKEN;

typedef struct _RESPONSE {
    union {
        TCGUINT i;
        TCGSHORTCHAR sc;
        TCGMEDCHAR mc;
        TCGTOKEN token;
    } data;
} RESPONSE;
#pragma pack(pop)

STATIC
EFI_STATUS
SendCmd (
  IN EFI_STORAGE_SECURITY_COMMAND_PROTOCOL *Ssp,
  IN UINT32                                MediaId,
  IN UINT8                                 AtaCommand,
  IN UINT16                                ComId,
  IN OUT VOID                              *Buffer,
  IN UINTN                                 BufSize
  )
{
  EFI_STATUS Status;

  if (AtaCommand == IF_RECV) {
    Status = Ssp->ReceiveData (
                    Ssp,
                    MediaId,
                    100000000, // 10 seconds timeout
                    0x01,      // Security Protocol 1 (TCG)
                    ComId,
                    BufSize,
                    Buffer,
                    &BufSize
                    );
  } else if (AtaCommand == IF_SEND) {
    Status = Ssp->SendData (
                    Ssp,
                    MediaId,
                    100000000, // 10 seconds timeout
                    0x01,      // Security Protocol 1 (TCG)
                    ComId,
                    BufSize,
                    Buffer
                    );
  } else {
    return EFI_INVALID_PARAMETER;
  }
  return Status;
}

STATIC
EFI_STATUS
Exec (
  IN EFI_STORAGE_SECURITY_COMMAND_PROTOCOL *Ssp,
  IN UINT32                                MediaId,
  IN VOID                                  *Cmd,
  IN OUT VOID                              *Response,
  IN UINT16                                ComId
  )
{
  EFI_STATUS Status;
  OPALHeader *Hdr;

  Status = SendCmd(Ssp, MediaId, IF_SEND, ComId, Cmd, IO_BUFFER_LENGTH);
  if (EFI_ERROR(Status)) {
    DEBUG((DEBUG_ERROR, "Command failed on send %r\n", Status));
    return Status;
  }

  Hdr = (OPALHeader *) Response;
  do {
    gBS->Stall(25000); // 25ms
    ZeroMem(Response, IO_BUFFER_LENGTH);
    Status = SendCmd(Ssp, MediaId, IF_RECV, ComId, Response, IO_BUFFER_LENGTH);
    if (EFI_ERROR(Status)) {
      DEBUG((DEBUG_ERROR, "Command failed on recv %r\n", Status));
      return Status;
    }
    // Check if we need to poll more
    // Note: Checking outstandingData (bit 0 set usually indicates more data available or processing)
    // The original code checks (0 != hdr->cp.outstandingData) && (0 == hdr->cp.minTransfer)
    // Note: outstandingData is BE
  } while ((0 != Hdr->cp.outstandingData) && (0 == Hdr->cp.minTransfer));

  return EFI_SUCCESS;
}

VOID
Discovery0 (
  IN EFI_STORAGE_SECURITY_COMMAND_PROTOCOL *Ssp,
  IN UINT32                                MediaId,
  IN OUT OPAL_DiskInfo                     *DiskInfo
  )
{
    VOID *D0Response;
    UINT8 *Epos, *Cpos;
    Discovery0Header *Hdr;
    Discovery0Features *Body;
    EFI_STATUS Status;

    D0Response = AllocatePages(EFI_SIZE_TO_PAGES(IO_BUFFER_LENGTH));
    if (D0Response == NULL) return;

    ZeroMem(D0Response, IO_BUFFER_LENGTH);
    Status = SendCmd(Ssp, MediaId, IF_RECV, 0x0001, D0Response, IO_BUFFER_LENGTH);
    if (EFI_ERROR(Status)) {
        DEBUG((DEBUG_ERROR, "Discovery0 failed %r\n", Status));
        FreePages(D0Response, EFI_SIZE_TO_PAGES(IO_BUFFER_LENGTH));
        return;
    }

    // Dump logic skipped for brevity, relies on DEBUG if needed

    Epos = Cpos = (UINT8 *) D0Response;
    Hdr = (Discovery0Header *) D0Response;
    Epos = Epos + SWAP32(Hdr->length);
    Cpos = Cpos + 48; // Skip header

    do {
        Body = (Discovery0Features *) Cpos;
        switch (SWAP16(Body->TPer.featureCode)) {
        case FC_TPER:
            DiskInfo->TPer = 1;
            DiskInfo->TPer_ACKNACK = Body->TPer.acknack;
            DiskInfo->TPer_async = Body->TPer.async;
            DiskInfo->TPer_bufferMgt = Body->TPer.bufferManagement;
            DiskInfo->TPer_comIDMgt = Body->TPer.comIDManagement;
            DiskInfo->TPer_streaming = Body->TPer.streaming;
            DiskInfo->TPer_sync = Body->TPer.sync;
            break;
        case FC_LOCKING:
            DiskInfo->Locking = 1;
            DiskInfo->Locking_locked = Body->locking.locked;
            DiskInfo->Locking_lockingEnabled = Body->locking.lockingEnabled;
            DiskInfo->Locking_lockingSupported = Body->locking.lockingSupported;
            DiskInfo->Locking_MBRDone = Body->locking.MBRDone;
            DiskInfo->Locking_MBREnabled = Body->locking.MBREnabled;
            DiskInfo->Locking_mediaEncrypt = Body->locking.mediaEncryption;
            break;
        case FC_GEOMETRY:
            DiskInfo->Geometry = 1;
            DiskInfo->Geometry_align = Body->geometry.align;
            DiskInfo->Geometry_alignmentGranularity = SWAP64(Body->geometry.alignmentGranularity);
            DiskInfo->Geometry_logicalBlockSize = SWAP32(Body->geometry.logicalBlockSize);
            DiskInfo->Geometry_lowestAlignedLBA = SWAP64(Body->geometry.lowestAlighedLBA);
            break;
        case FC_ENTERPRISE:
            DiskInfo->Enterprise = 1;
            DiskInfo->ANY_OPAL_SSC = 1;
            DiskInfo->Enterprise_rangeCrossing = Body->enterpriseSSC.rangeCrossing;
            DiskInfo->Enterprise_basecomID = SWAP16(Body->enterpriseSSC.baseComID);
            DiskInfo->Enterprise_numcomID = SWAP16(Body->enterpriseSSC.numberComIDs);
            break;
        case FC_OPALV100:
            DiskInfo->OPAL10 = 1;
            DiskInfo->ANY_OPAL_SSC = 1;
            DiskInfo->OPAL10_basecomID = SWAP16(Body->opalv100.baseComID);
            DiskInfo->OPAL10_numcomIDs = SWAP16(Body->opalv100.numberComIDs);
            break;
        case FC_SINGLEUSER:
            DiskInfo->SingleUser = 1;
            DiskInfo->SingleUser_all = Body->singleUserMode.all;
            DiskInfo->SingleUser_any = Body->singleUserMode.any;
            DiskInfo->SingleUser_policy = Body->singleUserMode.policy;
            DiskInfo->SingleUser_lockingObjects = SWAP32(Body->singleUserMode.numberLockingObjects);
            break;
        case FC_DATASTORE:
            DiskInfo->DataStore = 1;
            DiskInfo->DataStore_maxTables = SWAP16(Body->datastore.maxTables);
            DiskInfo->DataStore_maxTableSize = SWAP32(Body->datastore.maxSizeTables);
            DiskInfo->DataStore_alignment = SWAP32(Body->datastore.tableSizeAlignment);
            break;
        case FC_OPALV200:
            DiskInfo->OPAL20 = 1;
            DiskInfo->ANY_OPAL_SSC = 1;
            DiskInfo->OPAL20_basecomID = SWAP16(Body->opalv200.baseCommID);
            DiskInfo->OPAL20_initialPIN = Body->opalv200.initialPIN;
            DiskInfo->OPAL20_revertedPIN = Body->opalv200.revertedPIN;
            DiskInfo->OPAL20_numcomIDs = SWAP16(Body->opalv200.numCommIDs);
            DiskInfo->OPAL20_numAdmins = SWAP16(Body->opalv200.numlockingAdminAuth);
            DiskInfo->OPAL20_numUsers = SWAP16(Body->opalv200.numlockingUserAuth);
            DiskInfo->OPAL20_rangeCrossing = Body->opalv200.rangeCrossing;
            break;
        default:
            DiskInfo->Unknown++;
            break;
        }
        Cpos = Cpos + (Body->TPer.length + 4);
    } while (Cpos < Epos);

    FreePages(D0Response, EFI_SIZE_TO_PAGES(IO_BUFFER_LENGTH));
}

EFI_STATUS
UnlockOpal (
  IN EFI_STORAGE_SECURITY_COMMAND_PROTOCOL *Ssp,
  IN UINT32                                MediaId,
  IN CHAR8                                 *Pass,
  IN OPAL_DiskInfo                         *DiskInfo
  )
{
    UINT8 *Cmd;
    UINT8 *Resp;
    UINT16 ComId;
    UINT32 Tsn;
    VOID *PassPos;
    OPALHeader *PCmd;
    VOID *RespPos;
    RESPONSE *PResp;
    EFI_STATUS Status;

    Cmd = AllocatePages(EFI_SIZE_TO_PAGES(IO_BUFFER_LENGTH));
    Resp = AllocatePages(EFI_SIZE_TO_PAGES(IO_BUFFER_LENGTH));

    if (Cmd == NULL || Resp == NULL) {
        if (Cmd) FreePages(Cmd, EFI_SIZE_TO_PAGES(IO_BUFFER_LENGTH));
        if (Resp) FreePages(Resp, EFI_SIZE_TO_PAGES(IO_BUFFER_LENGTH));
        return EFI_OUT_OF_RESOURCES;
    }

    PCmd = (OPALHeader *) Cmd;
    ComId = DiskInfo->OPAL20 ? DiskInfo->OPAL20_basecomID : DiskInfo->OPAL10_basecomID;

    // Start Session
    ZeroMem(Cmd, IO_BUFFER_LENGTH);
    CopyMem(Cmd, StartSession, sizeof(StartSession));

    PCmd->cp.extendedComID[0] = ((ComId & 0xff00) >> 8);
    PCmd->cp.extendedComID[1] = (ComId & 0x00ff);

    // Hash password
    PassPos = (VOID *) Cmd;
    PassPos += 0x5c; // Offset in StartSession command

    // gc_pbkdf2_sha1(pass, strnlen(pass, 256), (char *) disk_info->serialNum, 20, 75000, passpos, 32);
    if (!Pkcs5HashPassword(
        AsciiStrLen(Pass),
        Pass,
        20,
        (CONST UINT8 *)DiskInfo->serialNum,
        75000,
        SHA1_DIGEST_SIZE,
        32,
        (UINT8 *)PassPos
        )) {
      DEBUG((DEBUG_ERROR, "Pkcs5HashPassword failed\n"));
      Status = EFI_ABORTED;
      goto Done;
    }

    Status = Exec(Ssp, MediaId, Cmd, Resp, ComId);
    if (EFI_ERROR(Status)) {
        DEBUG((DEBUG_ERROR, "StartSession failed\n"));
        goto Done;
    }

    // Get TSN
    RespPos = (VOID *) Resp + sizeof(OPALHeader);
    PResp = (RESPONSE *) RespPos;
    if (PResp->data.token.header != 0xf8) {
        Status = EFI_DEVICE_ERROR;
        goto Done;
    }
    RespPos += (1 + 9 + 9);
    PResp = (RESPONSE *) RespPos;
    if (PResp->data.token.header != 0xf0) {
        Status = EFI_DEVICE_ERROR;
        goto Done;
    }
    RespPos += 1;
    PResp = (RESPONSE *) RespPos;
    if ((PResp->data.token.header & 0xf0) != 0x80) {
        Status = EFI_DEVICE_ERROR;
        goto Done;
    }
    RespPos += ((PResp->data.token.header & 0x0f) + 1);
    PResp = (RESPONSE *) RespPos;
    if ((PResp->data.token.header & 0xf0) != 0x80) {
        Status = EFI_DEVICE_ERROR;
        goto Done;
    }

    if ((PResp->data.token.header & 0x0f) == 1)
        Tsn = (UINT32)PResp->data.i.d.TCGUINT8;
    else if ((PResp->data.token.header & 0x0f) == 2)
        Tsn = (UINT32)SWAP16(PResp->data.i.d.TCGUINT16);
    else if ((PResp->data.token.header & 0x0f) == 4)
        Tsn = SWAP32(PResp->data.i.d.TCGUINT32);
    else {
        Status = EFI_DEVICE_ERROR;
        goto Done;
    }

    // Unlock
    ZeroMem(Cmd, IO_BUFFER_LENGTH);
    CopyMem(Cmd, Unlock, sizeof(Unlock));
    PCmd->cp.extendedComID[0] = ((ComId & 0xff00) >> 8);
    PCmd->cp.extendedComID[1] = (ComId & 0x00ff);
    PCmd->pkt.TSN = SWAP32(Tsn);

    Status = Exec(Ssp, MediaId, Cmd, Resp, ComId);
    if (EFI_ERROR(Status)) {
        DEBUG((DEBUG_ERROR, "Unlock failed\n"));
        goto Done;
    }

    RespPos = (VOID *) Resp + sizeof(OPALHeader);
    PResp = (RESPONSE *) RespPos;
    if (PResp->data.token.header != 0xf0) {
        Status = EFI_DEVICE_ERROR;
        goto Done;
    }
    if (PResp->data.token.bytes[3] != 0x00) {
        DEBUG((DEBUG_ERROR, "Unlock status: %x\n", PResp->data.token.bytes[3]));
        Status = EFI_ACCESS_DENIED;
        goto Done;
    }

    // MBR Done if enabled
    if (DiskInfo->Locking_MBREnabled) {
        ZeroMem(Cmd, IO_BUFFER_LENGTH);
        CopyMem(Cmd, MbrDone, sizeof(MbrDone));
        PCmd->cp.extendedComID[0] = ((ComId & 0xff00) >> 8);
        PCmd->cp.extendedComID[1] = (ComId & 0x00ff);
        PCmd->pkt.TSN = SWAP32(Tsn);

        Status = Exec(Ssp, MediaId, Cmd, Resp, ComId);
        if (EFI_ERROR(Status)) {
             DEBUG((DEBUG_ERROR, "MbrDone failed\n"));
             goto Done;
        }
    }

    // End Session
    ZeroMem(Cmd, IO_BUFFER_LENGTH);
    CopyMem(Cmd, EndSession, sizeof(EndSession));
    PCmd->cp.extendedComID[0] = ((ComId & 0xff00) >> 8);
    PCmd->cp.extendedComID[1] = (ComId & 0x00ff);
    PCmd->pkt.TSN = SWAP32(Tsn);

    Status = Exec(Ssp, MediaId, Cmd, Resp, ComId);
    if (EFI_ERROR(Status)) {
         DEBUG((DEBUG_ERROR, "EndSession failed\n"));
         goto Done;
    }

    Status = EFI_SUCCESS;

Done:
    if (Cmd) {
      ZeroMem(Cmd, IO_BUFFER_LENGTH); // Sanitize
      FreePages(Cmd, EFI_SIZE_TO_PAGES(IO_BUFFER_LENGTH));
    }
    if (Resp) {
      ZeroMem(Resp, IO_BUFFER_LENGTH);
      FreePages(Resp, EFI_SIZE_TO_PAGES(IO_BUFFER_LENGTH));
    }
    return Status;
}
