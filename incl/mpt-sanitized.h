#define MPI_FUNCTION_CONFIG				(0x04)
#define MPI_CONFIG_ACTION_PAGE_HEADER			(0x00)
#define MPI_CONFIG_PAGETYPE_RAID_VOLUME			(0x08)
#define MPI_CONFIG_ACTION_PAGE_READ_CURRENT		(0x01)
#define MPI_RAIDVOLPAGE0_PAGEVERSION			(0x01)
#define MPI_RAIDVOL0_STATUS_FLAG_ENABLED		(0x01)
#define MPI_RAIDVOL0_STATUS_FLAG_QUIESCED		(0x02)
#define MPI_RAIDVOL0_STATUS_FLAG_RESYNC_IN_PROGRESS	(0x04)
#define MPI_RAIDVOL0_STATUS_FLAG_VOLUME_INACTIVE	(0x08)
#define MPI_RAIDVOL0_STATUS_STATE_OPTIMAL		(0x00)
#define MPI_RAIDVOL0_STATUS_STATE_DEGRADED		(0x01)
#define MPI_RAIDVOL0_STATUS_STATE_FAILED		(0x02)
#define MPI_CONFIG_PAGETYPE_RAID_PHYSDISK		(0x0A)
#define MPI_RAIDPHYSDISKPAGE0_PAGEVERSION		(0x00)
#define MPI_PHYSDISK0_STATUS_FLAG_OUT_OF_SYNC		(0x01)
#define MPI_PHYSDISK0_STATUS_FLAG_QUIESCED		(0x02)
#define MPI_PHYSDISK0_STATUS_ONLINE			(0x00)
#define MPI_PHYSDISK0_STATUS_MISSING			(0x01)
#define MPI_PHYSDISK0_STATUS_NOT_COMPATIBLE		(0x02)
#define MPI_PHYSDISK0_STATUS_FAILED			(0x03)
#define MPI_PHYSDISK0_STATUS_FAILED_REQUESTED		(0x06)
#define MPI_PHYSDISK0_STATUS_INITIALIZING		(0x04)
#define MPI_PHYSDISK0_STATUS_OFFLINE_REQUESTED		(0x05)
#define MPI_PHYSDISK0_STATUS_FAILED_REQUESTED		(0x06)
#define MPI_PHYSDISK0_STATUS_OTHER_OFFLINE		(0xFF)
#define MPI_RAID_VOL_PAGE_0_PHYSDISK_MAX		(1)

#define MPT_MAGIC_NUMBER	'm'
#define MPTCOMMAND		_IOWR(MPT_MAGIC_NUMBER,20,struct mpt_ioctl_command)

typedef struct ConfigPageHeader_t {
	uint8_t PageVersion;	/* 00h */
	uint8_t PageLength;	/* 01h */
	uint8_t PageNumber;	/* 02h */
	uint8_t PageType;	/* 03h */
} ConfigPageHeader_t;

typedef struct SGESimpleUnion_t {
	uint32_t FlagsLength;
	union {
		uint32_t Address32;
		uint64_t Address64;
	} u;
} SGESimpleUnion_t;

typedef struct SGEChainUnion_t {
	uint16_t Length;
	uint8_t NextChainOffset;
	uint8_t Flags;
	union {
		uint32_t Address32;
		uint64_t Address64;
	} u;
} SGEChainUnion_t;

typedef struct SGEIOUnion_t {
	union {
		SGESimpleUnion_t Simple;
		SGEChainUnion_t Chain;
	} u;
} SGEIOUnion_t;

typedef struct Config_t {
	uint8_t Action;		/* 00h */
	uint8_t Reserved;	/* 01h */
	uint8_t ChainOffset;	/* 02h */
	uint8_t Function;	/* 03h */
	uint16_t ExtPageLength;	/* 04h */
	uint8_t ExtPageType;	/* 06h */
	uint8_t MsgFlags;	/* 07h */
	uint32_t MsgContext;	/* 08h */
	uint8_t Reserved2[8];	/* 0Ch */
	ConfigPageHeader_t Header;	/* 14h */
	uint32_t PageAddress;	/* 18h */
	SGEIOUnion_t PageBufferSGE;	/* 1Ch */
} Config_t;

typedef struct ConfigReply_t {
	uint8_t Action;		/* 00h */
	uint8_t Reserved;	/* 01h */
	uint8_t MsgLength;	/* 02h */
	uint8_t Function;	/* 03h */
	uint16_t ExtPageLength;	/* 04h */
	uint8_t ExtPageType;	/* 06h */
	uint8_t MsgFlags;	/* 07h */
	uint32_t MsgContext;	/* 08h */
	uint8_t Reserved2[2];	/* 0Ch */
	uint16_t IOCStatus;	/* 0Eh */
	uint32_t IOCLogInfo;	/* 10h */
	ConfigPageHeader_t Header;	/* 14h */
} ConfigReply_t;

typedef struct RaidVol0Status_t {
	uint8_t Flags;		/* 00h */
	uint8_t State;		/* 01h */
	uint16_t Reserved;	/* 02h */
} RaidVol0Status_t;

typedef struct RaidVol0Settings {
	uint16_t Settings;	/* 00h */
	uint8_t HotSparePool;	/* 01h *//* MPI_RAID_HOT_SPARE_POOL_ */
	uint8_t Reserved;	/* 02h */
} RaidVol0Settings;

typedef struct RaidVol0PhysDisk_t {
	uint16_t Reserved;	/* 00h */
	uint8_t PhysDiskMap;	/* 02h */
	uint8_t PhysDiskNum;	/* 03h */
} RaidVol0PhysDisk_t;

typedef struct RaidVolumePage0_t {
	ConfigPageHeader_t Header;	/* 00h */
	uint8_t VolumeID;	/* 04h */
	uint8_t VolumeBus;	/* 05h */
	uint8_t VolumeIOC;	/* 06h */
	uint8_t VolumeType;	/* 07h *//* MPI_RAID_VOL_TYPE_ */
	RaidVol0Status_t VolumeStatus;	/* 08h */
	RaidVol0Settings VolumeSettings;	/* 0Ch */
	uint32_t MaxLBA;	/* 10h */
	uint32_t Reserved1;	/* 14h */
	uint32_t StripeSize;	/* 18h */
	uint32_t Reserved2;	/* 1Ch */
	uint32_t Reserved3;	/* 20h */
	uint8_t NumPhysDisks;	/* 24h */
	uint8_t Reserved4;	/* 25h */
	uint16_t Reserved5;	/* 26h */
	RaidVol0PhysDisk_t PhysDisk[MPI_RAID_VOL_PAGE_0_PHYSDISK_MAX];	/* 28h */
} RaidVolumePage0_t;

typedef struct RaidPhysDiskSettings_t {
	uint8_t SepID;		/* 00h */
	uint8_t SepBus;		/* 01h */
	uint8_t HotSparePool;	/* 02h *//* MPI_RAID_HOT_SPARE_POOL_ */
	uint8_t PhysDiskSettings;	/* 03h */
} RaidPhysDiskSettings_t;

typedef struct RaidPhysDisk0InquiryData {
	uint8_t VendorID[8];	/* 00h */
	uint8_t ProductID[16];	/* 08h */
	uint8_t ProductRevLevel[4];	/* 18h */
	uint8_t Info[32];	/* 1Ch */
} RaidPhysDisk0InquiryData;

typedef struct RaidPhysDiskStatus_t {
	uint8_t Flags;		/* 00h */
	uint8_t State;		/* 01h */
	uint16_t Reserved;	/* 02h */
} RaidPhysDiskStatus_t;

typedef struct RaidPhysDisk0ErrorData_t {
	uint8_t ErrorCdbByte;	/* 00h */
	uint8_t ErrorSenseKey;	/* 01h */
	uint16_t Reserved;	/* 02h */
	uint16_t ErrorCount;	/* 04h */
	uint8_t ErrorASC;	/* 06h */
	uint8_t ErrorASCQ;	/* 07h */
	uint16_t SmartCount;	/* 08h */
	uint8_t SmartASC;	/* 0Ah */
	uint8_t SmartASCQ;	/* 0Bh */
} RaidPhysDisk0ErrorData_t;

typedef struct RaidPhysDiskPage0_t {
	ConfigPageHeader_t Header;	/* 00h */
	uint8_t PhysDiskID;	/* 04h */
	uint8_t PhysDiskBus;	/* 05h */
	uint8_t PhysDiskIOC;	/* 06h */
	uint8_t PhysDiskNum;	/* 07h */
	RaidPhysDiskSettings_t PhysDiskSettings;	/* 08h */
	uint32_t Reserved1;	/* 0Ch */
	uint32_t Reserved2;	/* 10h */
	uint32_t Reserved3;	/* 14h */
	uint8_t DiskIdentifier[16];	/* 18h */
	RaidPhysDisk0InquiryData InquiryData;	/* 28h */
	RaidPhysDiskStatus_t PhysDiskStatus;	/* 64h */
	uint32_t MaxLBA;	/* 68h */
	RaidPhysDisk0ErrorData_t ErrorData;	/* 6Ch */
} RaidPhysDiskPage0_t;

typedef struct mpt_ioctl_header {
	unsigned int iocnum;	/* IOC unit number */
	unsigned int port;	/* IOC port number */
	int maxDataSize;	/* Maximum Num. bytes to transfer on read */
} mpt_ioctl_header;

struct mpt_ioctl_command {
	mpt_ioctl_header hdr;
	int timeout;		/* optional (seconds) */
	char *replyFrameBufPtr;
	char *dataInBufPtr;
	char *dataOutBufPtr;
	char *senseDataPtr;
	int maxReplyBytes;
	int dataInSize;
	int dataOutSize;
	int maxSenseBytes;
	int dataSgeOffset;
	char MF[1];
};
