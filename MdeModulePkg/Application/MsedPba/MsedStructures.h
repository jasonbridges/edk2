/** @file
  Opal structures.

  Copyright (c) 2014-2015 Michael Romeo <r0m30@r0m30.com>
  Copyright (c) 2024, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once
#pragma pack(push, 1)

#include <Uefi.h>

#define FC_TPER		  0x0001
#define FC_LOCKING    0x0002
#define FC_GEOMETRY   0x0003
#define FC_ENTERPRISE 0x0100
#define FC_DATASTORE  0x0202
#define FC_SINGLEUSER 0x0201
#define FC_OPALV100   0x0200
#define FC_OPALV200   0x0203

typedef struct _Discovery0Header {
    UINT32 length;
    UINT32 revision;
    UINT32 reserved01;
    UINT32 reserved02;
} Discovery0Header;

typedef struct _Discovery0TPerFeatures {
    UINT16 featureCode;
    UINT8 reserved_v : 4;
    UINT8 version : 4;
    UINT8 length;
    UINT8 sync : 1;
    UINT8 async : 1;
    UINT8 acknack : 1;
    UINT8 bufferManagement : 1;
    UINT8 streaming : 1;
    UINT8 reserved02 : 1;
    UINT8 comIDManagement : 1;
    UINT8 reserved01 : 1;

    UINT32 reserved03;
    UINT32 reserved04;
    UINT32 reserved05;
} Discovery0TPerFeatures;

typedef struct _Discovery0LockingFeatures {
    UINT16 featureCode;
    UINT8 reserved_v : 4;
    UINT8 version : 4;
    UINT8 length;
    UINT8 lockingSupported : 1;
    UINT8 lockingEnabled : 1;
    UINT8 locked : 1;
    UINT8 mediaEncryption : 1;
    UINT8 MBREnabled : 1;
    UINT8 MBRDone : 1;
    UINT8 reserved01 : 1;
    UINT8 reserved02 : 1;

    UINT32 reserved03;
    UINT32 reserved04;
    UINT32 reserved05;
} Discovery0LockingFeatures;

typedef struct _Discovery0GeometryFeatures {
    UINT16 featureCode;
    UINT8 reserved_v : 4;
    UINT8 version : 4;
    UINT8 length;
    UINT8 align : 1;
    UINT8 reserved01 : 7;
    UINT8 reserved02[7];
    UINT32 logicalBlockSize;
    UINT64 alignmentGranularity;
    UINT64 lowestAlighedLBA;
} Discovery0GeometryFeatures;

typedef struct _Discovery0EnterpriseSSC {
    UINT16 featureCode;
    UINT8 reserved_v : 4;
    UINT8 version : 4;
    UINT8 length;
    UINT16 baseComID;
    UINT16 numberComIDs;
    UINT8 rangeCrossing : 1;
    UINT8 reserved01 : 7;

    UINT8 reserved02;
    UINT16 reserved03;
    UINT32 reserved04;
    UINT32 reserved05;
} Discovery0EnterpriseSSC;

typedef struct _Discovery0OpalV100 {
	UINT16 featureCode;
	UINT8 reserved_v : 4;
	UINT8 version : 4;
	UINT8 length;
	UINT16 baseComID;
	UINT16 numberComIDs;
} Discovery0OpalV100;

typedef struct _Discovery0SingleUserMode {
    UINT16 featureCode;
    UINT8 reserved_v : 4;
    UINT8 version : 4;
    UINT8 length;
    UINT32 numberLockingObjects;
    UINT8 any : 1;
    UINT8 all : 1;
    UINT8 policy : 1;
    UINT8 reserved01 : 5;

    UINT8 reserved02;
    UINT16 reserved03;
    UINT32 reserved04;
} Discovery0SingleUserMode;

typedef struct _Discovery0DatastoreTable {
    UINT16 featureCode;
    UINT8 reserved_v : 4;
    UINT8 version : 4;
    UINT8 length;
    UINT16 reserved01;
    UINT16 maxTables;
    UINT32 maxSizeTables;
    UINT32 tableSizeAlignment;
} Discovery0DatastoreTable;

typedef struct _Discovery0OPALV200 {
    UINT16 featureCode;
    UINT8 reserved_v : 4;
    UINT8 version : 4;
    UINT8 length;
    UINT16 baseCommID;
    UINT16 numCommIDs;
    UINT8 rangeCrossing : 1;
    UINT8 reserved01 : 7;

    UINT16 numlockingAdminAuth;
    UINT16 numlockingUserAuth;
    UINT8 initialPIN;
    UINT8 revertedPIN;
    UINT8 reserved02;
    UINT32 reserved03;
} Discovery0OPALV200;

typedef union _Discovery0Features {
    Discovery0TPerFeatures TPer;
    Discovery0LockingFeatures locking;
    Discovery0GeometryFeatures geometry;
    Discovery0EnterpriseSSC enterpriseSSC;
    Discovery0SingleUserMode singleUserMode;
    Discovery0OPALV200 opalv200;
	Discovery0OpalV100 opalv100;
    Discovery0DatastoreTable datastore;
} Discovery0Features;

typedef struct _OPALComPacket {
    UINT32 reserved0;
    UINT8 extendedComID[4];
    UINT32 outstandingData;
    UINT32 minTransfer;
    UINT32 length;
} OPALComPacket;

typedef struct _OPALPacket {
    UINT32 TSN;
    UINT32 HSN;
    UINT32 seqNumber;
    UINT16 reserved0;
    UINT16 ackType;
    UINT32 aknowledgement;
    UINT32 length;
} OPALPacket;

typedef struct _OPALDataSubPacket {
    UINT8 reserved0[6];
    UINT16 kind;
    UINT32 length;
} OPALDataSubPacket;

typedef struct _OPALHeader {
    OPALComPacket cp;
    OPALPacket pkt;
    OPALDataSubPacket subpkt;
} OPALHeader;

typedef enum _ATACOMMAND {
    IF_RECV = 0x5c,
    IF_SEND = 0x5e,
    IDENTIFY = 0xec,
} ATACOMMAND;

typedef struct _OPAL_DiskInfo {
	UINT8 Unknown;
	UINT8 VendorSpecific;
    UINT8 TPer : 1;
    UINT8 Locking : 1;
    UINT8 Geometry : 1;
    UINT8 Enterprise : 1;
    UINT8 SingleUser : 1;
    UINT8 DataStore : 1;
    UINT8 OPAL20 : 1;
	UINT8 OPAL10 : 1;
	UINT8 Properties : 1;
	UINT8 ANY_OPAL_SSC : 1;
	UINT8 SupportedSSC : 1;
    UINT8 TPer_ACKNACK : 1;
    UINT8 TPer_async : 1;
    UINT8 TPer_bufferMgt : 1;
    UINT8 TPer_comIDMgt : 1;
    UINT8 TPer_streaming : 1;
    UINT8 TPer_sync : 1;
    UINT8 Locking_locked : 1;
    UINT8 Locking_lockingEnabled : 1;
    UINT8 Locking_lockingSupported : 1;
    UINT8 Locking_MBRDone : 1;
    UINT8 Locking_MBREnabled : 1;
    UINT8 Locking_mediaEncrypt : 1;
    UINT8 Geometry_align : 1;
    UINT64 Geometry_alignmentGranularity;
    UINT32 Geometry_logicalBlockSize;
    UINT64 Geometry_lowestAlignedLBA;
    UINT8 Enterprise_rangeCrossing : 1;
    UINT16 Enterprise_basecomID;
    UINT16 Enterprise_numcomID;
    UINT8 SingleUser_any : 1;
    UINT8 SingleUser_all : 1;
    UINT8 SingleUser_policy : 1;
    UINT32 SingleUser_lockingObjects;
    UINT16 DataStore_maxTables;
    UINT32 DataStore_maxTableSize;
    UINT32 DataStore_alignment;
	UINT16 OPAL10_basecomID;
	UINT16 OPAL10_numcomIDs;
    UINT16 OPAL20_basecomID;
    UINT16 OPAL20_numcomIDs;
    UINT8 OPAL20_initialPIN;
    UINT8 OPAL20_revertedPIN;
    UINT16 OPAL20_numAdmins;
    UINT16 OPAL20_numUsers;
    UINT8 OPAL20_rangeCrossing;
    UINT8 devType : 1;
    UINT8 serialNum[20];
	UINT8 null0;
    UINT8 firmwareRev[8];
	UINT8 null1;
    UINT8 modelNum[40];
	UINT8 null2;
} OPAL_DiskInfo;

typedef struct _IDENTIFY_RESPONSE {
    UINT8 reserved0;
    UINT8 reserved1 : 7;
    UINT8 devType : 1;
    UINT8 reserved2[18];
    UINT8 serialNum[20];
    UINT8 reserved3[6];
    UINT8 firmwareRev[8];
    UINT8 modelNum[40];
    UINT8 reserved4[2];
    UINT16 TCGSupport;
} IDENTIFY_RESPONSE;

#pragma pack(pop)
