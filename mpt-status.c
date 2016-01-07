/*

This is a program to print the status of an LSI 1030 RAID or LSI
SAS1064 SCSI controller.

Copyright (C) 2004 CNET Networks, Inc.
Copyright (C) 2005-2006 Roberto Nibali
Copyright (C) 2006 by LSI Logic

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or (at
your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
USA.

*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <netinet/in.h>
#include "mpt-status.h"

#define ARG_M_A 0x0001
#define ARG_M_S 0x0002

static int m = 0;
static int quiet_mode = 0;
static int verbose_mode = 0;
static int debug = 0;
static int debug_level = 0;
static int auto_load = 0;
static int probe_id = 0;
static int mpt_exit_mask = MPT_EXIT_OKAY;
static int print_status_only = PRINT_STATUS_ONLY;
static int id_of_primary_device = 0;
static int ioc_unit = 0;
static int newstyle = 0;
static int sync_state[2] = { 0, 0 };
static int sync_info = 0;

static int sel;
static const struct option long_options[] = {
	{ "autoload",		no_argument,       &sel,  ARG_M_A },
	{ "controller",		required_argument, NULL, 'u' },
	{ "debug",		required_argument, NULL, 'd' },
	{ "help",		no_argument,       NULL, 'h' },
	{ "newstyle",		no_argument,       NULL, 'n' },
	{ "probe_id",		no_argument,	   NULL, 'p' },
	{ "quiet",		no_argument,       NULL, 'q' },
	{ "set_id",		required_argument, NULL, 'i' },
	{ "status_only",	no_argument,       NULL, 's' },
	{ "sync_info",		no_argument,	   &sel, ARG_M_S },
	{ "verbose",            no_argument,       NULL, 'v' },
	{ "version",            no_argument,       NULL, 'V' },
	{ 0,                    no_argument,       NULL,  0  },
};
static const char* const short_options  = "d:hi:npqsu:vV";
static const char* const usage_template =
  "Usage: %s [ options ]\n"
  "\n"
  "      --autoload             This will try to automatically load the \n"
  "                             mptctl kernel module\n"
  "  -d, --debug <int>          Enable debugging and set level\n"
  "                             NOTE: This is not fully implemented yet\n"
  "  -h, --help                 Print this help information page\n"
  "  -n, --newstyle             Use the new style output. This parameter was\n"
  "                             introduced to retain backwards compatibility\n"
  "                             If not set, you get the old style output\n"
  "  -p, --probe_id             Use this to probe SCSI id's when not on id 0\n"
  "  -q, --quiet                Do not display any warnings or boring info\n"
  "  -i, --set_id <int>         Set id of primary device (check README)\n"
  "  -s, --status_only          Only print the status information. This can\n"
  "                             be used for easy scripting\n"
  "      --sync_info            Show RAID (re)synchronization information\n"
  "                             (subject to quiet mode setting)\n"
  "  -u, --controller <int>     Set the IOC unit (controller)\n"
  "  -v, --verbose              Print verbose information, such as warnings\n"
  "  -V, --version              Print version information\n"
  "\n"
  "   You can write ``-o arg'' or ``--option arg'' or ``--option=arg''\n"
  "   Note that for some parameters only the long format is supported.\n"
  "\n"
  "   For more information, please refer to mpt-status(8).\n"
  "\n"
  "\n";


static char *VolumeTypes[] = { "IS", "IME", "IM" };
static char *VolumeTypesHuman[] = { "RAID-0", "RAID-1E", "RAID-1" };
struct mpt_ioctl_command *mpiBlkPtr = NULL;
unsigned char g_command[BIG];
char  g_data[4*BIG];
char  g_reply[BIG];
int   g_bigEndian = 0;
static int resync_on = 0;

/* function declaration */
static unsigned int cpu_to_le32 (unsigned int);
static unsigned short le16_to_cpu(unsigned short);
static void freeMem(void);
static int allocDataFrame(int);
static int allocReplyFrame(void);
static void mpt_exit(int);
static int mpt_printf(const char *format, ...);
static int mpt_fprintf(FILE *, const char *, ...);
static void print_usage(const char *);
static void print_version(void);
static int read_page2(uint);
//static int hasVolume(void);
static void GetVolumeInfo(void);
static void GetPhysDiskInfo(RaidVol0PhysDisk_t *, int);
static void GetHotSpareInfo(void);
static void GetResyncPercentageSilent(RaidVol0PhysDisk_t *, unsigned char *, int);
static void GetResyncPercentage(RaidVol0PhysDisk_t *, unsigned char *, int);
static void GetSyncInfo(void);
static void do_init(void);
/* internal-functions declaration */
static void __check_endianess(void);
static void __print_volume_advanced(RaidVolumePage0_t *);
static void __print_volume_classic(RaidVolumePage0_t *);
static void __print_physdisk_advanced(RaidPhysDiskPage0_t *, int);
static void __print_physdisk_classic(RaidPhysDiskPage0_t *);
static SyncInfoData __get_resync_data(void);

static void __check_endianess(void) {
	int i = 1;

	char *p = (char *) &i;
	if (p[0] != 1) { // Lowest address contains the least significant byte
		g_bigEndian = 1;
	}
}

static unsigned int cpu_to_le32(unsigned int x) {
	if (g_bigEndian) {
		unsigned int y;
		y = (x & 0xFF00) << 8;
		y |= (x & 0xFF) << 24;
		y |= (x & 0xFF0000) >> 8;
		y |= (x & 0xFF000000) >> 24;
		return y;
	}
	return x;
}

static unsigned short le16_to_cpu(unsigned short x) {
	if (g_bigEndian) {
		unsigned short y = (((x & 0xFF00) >> 8) | ((x & 0xFF) << 8));
		return y;
	}
	return x;
}

static void freeMem(void) {
	if (mpiBlkPtr->replyFrameBufPtr)
		mpiBlkPtr->replyFrameBufPtr = NULL;
	if (mpiBlkPtr->dataOutBufPtr)
		mpiBlkPtr->dataOutBufPtr = NULL;
	if (mpiBlkPtr->dataInBufPtr)
		mpiBlkPtr->dataInBufPtr = NULL;
	mpiBlkPtr = NULL;
}

static int allocDataFrame(int dir) {
	if (dir == DATA_DIR_OUT) {
		if (mpiBlkPtr->dataOutSize > (4*BIG))
			return 1;
		mpiBlkPtr->dataOutBufPtr =  (char *)&g_data;
		memset(mpiBlkPtr->dataOutBufPtr, 0, mpiBlkPtr->dataOutSize);
	} else if (dir == DATA_DIR_IN) {
		if (mpiBlkPtr->dataInSize > (4*BIG))
			return 1;
		mpiBlkPtr->dataInBufPtr = (char *)&g_data;
		memset(mpiBlkPtr->dataInBufPtr, 0, mpiBlkPtr->dataInSize);
	}
	return 0;
}

static int allocReplyFrame(void) {
	mpiBlkPtr->replyFrameBufPtr = (char *) &g_reply;
	memset (mpiBlkPtr->replyFrameBufPtr, 0, REPLY_SIZE);
	mpiBlkPtr->maxReplyBytes = REPLY_SIZE;
	return 0;
}

struct mpt_ioctl_command *allocIoctlBlk(uint numBytes) {
	int blksize = sizeof(struct mpt_ioctl_command) + numBytes;

	if (blksize >= BIG) {
		return NULL;
	}
	mpiBlkPtr = (struct mpt_ioctl_command *) &g_command;
	memset(mpiBlkPtr, 0, blksize);
	if (allocReplyFrame()) {
		printf("allocReplyFrame call failed\n");
		freeMem();
		return NULL;
	}
	return mpiBlkPtr;
}

static void mpt_exit(int status) {
	/* this function is suboptimal in that it masks too many
	   semantic information into one int
	*/
	if (status & MPT_EXIT_FREEMEM) {
		freeMem();
	}
	if (status & MPT_EXIT_NOCLOSE) {
		close(m);
	}
	exit(mpt_exit_mask |= status);
}

static int mpt_printf(const char *format, ...) {
	int result;
	va_list ap;

	if (quiet_mode)
		return 0;
	va_start(ap, format);
	result = vprintf(format, ap);
	va_end(ap);
	return result;
}

static int mpt_fprintf(FILE *fp, const char *format, ...) {
	int result;
	va_list ap;

	if (quiet_mode)
		return 0;
	va_start(ap, format);
	result = vfprintf(fp, format, ap);
	va_end(ap);
	return result;
}

static int mpt_debug(const char *format, ...) {
	int result;
	va_list ap;

	if (!debug)
		return 0;
	/* debug_level is also set at this point */
	va_start(ap, format);
	result = vprintf(format, ap);
	va_end(ap);
	return result;
}

static void print_usage(const char *progname) {
        mpt_printf(usage_template, progname);
}

static void print_version(void) {
	mpt_printf("mpt-status version   : %s\n", VERSION);
	/* Next version (needs mptbase.h)
	mpt_printf("Driver header version: %s\n", MPT_LINUX_VERSION_COMMON);
	 */
}

static int __probe_scsi_id2(void) {
	Config_t 		*ConfigRequest;
	ConfigReply_t 		*pReply = NULL;
	RaidVolumePage0_t 	*pRVP0 = NULL;
	uint numBytes;
	int status;
	int id;
	int scsi_id;

	for (scsi_id = 0; scsi_id < 16; scsi_id++) {
		mpt_printf("Checking for SCSI ID:%d\n", scsi_id);
		numBytes = (sizeof(Config_t) - sizeof(SGE_IO_UNION)) + 
			sizeof(SGESimple64_t);
		if ((mpiBlkPtr = allocIoctlBlk(numBytes)) == NULL) {
			return -1;
		}

		ConfigRequest = (Config_t *) mpiBlkPtr->MF;
		mpiBlkPtr->dataInSize = mpiBlkPtr->dataOutSize = 0;
		mpiBlkPtr->dataInBufPtr = mpiBlkPtr->dataOutBufPtr = NULL;
		mpiBlkPtr->dataSgeOffset = (sizeof (Config_t) - sizeof(SGE_IO_UNION))/4;

		pReply = (ConfigReply_t *)mpiBlkPtr->replyFrameBufPtr;

		ConfigRequest->Action       = MPI_CONFIG_ACTION_PAGE_HEADER;
		ConfigRequest->Function     = MPI_FUNCTION_CONFIG;
		ConfigRequest->MsgContext   = -1;
		ConfigRequest->Header.PageType = MPI_CONFIG_PAGETYPE_RAID_VOLUME;
		ConfigRequest->Header.PageNumber = ioc_unit;
		ConfigRequest->PageAddress = scsi_id;

		status = read_page2(MPT_FLAGS_KEEP_MEM);
		if (status != 0 || pReply->Header.PageLength == 0) {
			freeMem();
			return -1;
		}
		mpiBlkPtr->dataInSize = pReply->Header.PageLength * 4;
		if (allocDataFrame(DATA_DIR_IN)) {
			mpt_printf("Increase data buffer size");
			freeMem();
			return -1;
		}
		ConfigRequest->Action = MPI_CONFIG_ACTION_PAGE_READ_CURRENT;
		ConfigRequest->Header.PageVersion = pReply->Header.PageVersion;
		ConfigRequest->Header.PageLength = pReply->Header.PageLength;
		id = 0; // volume id: only one volume for now (vol_ids)
		/* The following PageAddress does not make too much sense */
		ConfigRequest->PageAddress = scsi_id | id;
		status = read_page2(MPT_FLAGS_KEEP_MEM);
		pRVP0 = (RaidVolumePage0_t *) mpiBlkPtr->dataInBufPtr;
		if ((status == 0) && (pRVP0->NumPhysDisks > 0)) {
			/* We have found a Volume with some physical disks */
			freeMem();
			return scsi_id;
		}
	}
	freeMem();
	return -1;
}

static int read_page2(uint flags) {
	MPIDefaultReply_t *pReply = NULL;
	int status = -1;

	mpiBlkPtr->hdr.iocnum = ioc_unit;
	mpiBlkPtr->hdr.port = 0;
	if (ioctl(m, (unsigned long) MPTCOMMAND, (char *) mpiBlkPtr) != 0) {
		perror("ioctl");
		mpt_exit(MPT_EXIT_UNKNOWN);
	} else {
		pReply = (MPIDefaultReply_t *) mpiBlkPtr->replyFrameBufPtr;
		if ((pReply) && (pReply->MsgLength > 0)) {
			pReply->IOCStatus = le16_to_cpu(pReply->IOCStatus);
			status = pReply->IOCStatus & MPI_IOCSTATUS_MASK;
		} else {
			status = 0;
		}
	}
	if ((flags & MPT_FLAGS_KEEP_MEM) == 0) {
		freeMem();
	}
	return status;
}

/* Will be needed for detecting if a HBA has its SCSI disks configured as RAID
static int hasVolume(void) {
	Config_t *ConfigRequest;
	ConfigReply_t *pReply = NULL;
	IOCPage2_t *pIOC2 = NULL;
	uint numBytes;
	uint numVolumes = 0;
	int status;
	unsigned bus = id_of_primary_device;

	numBytes = (sizeof(Config_t) - sizeof(SGE_IO_UNION)) + 
		sizeof(SGESimple64_t);
	if ((mpiBlkPtr = allocIoctlBlk(numBytes)) == NULL) {
		return numVolumes;
	}

	ConfigRequest = (Config_t *) mpiBlkPtr->MF;
	mpiBlkPtr->dataInSize = mpiBlkPtr->dataOutSize = 0;
	mpiBlkPtr->dataInBufPtr = mpiBlkPtr->dataOutBufPtr = NULL;
	mpiBlkPtr->dataSgeOffset = (sizeof (Config_t) - sizeof(SGE_IO_UNION))/4;

	pReply = (ConfigReply_t *)mpiBlkPtr->replyFrameBufPtr;

	ConfigRequest->Action       = MPI_CONFIG_ACTION_PAGE_HEADER;
	ConfigRequest->Function     = MPI_FUNCTION_CONFIG;
	ConfigRequest->MsgContext   = -1;
	ConfigRequest->Header.PageType = MPI_CONFIG_PAGETYPE_IOC;
	ConfigRequest->Header.PageNumber = ioc_unit;
	ConfigRequest->PageAddress = bus;

	status = read_page2(MPT_FLAGS_KEEP_MEM);
	if (status != 0 || pReply->Header.PageLength == 0) {
		freeMem();
		return numVolumes;
	}
	mpiBlkPtr->dataInSize = pReply->Header.PageLength * 4;
	if (allocDataFrame(DATA_DIR_IN)) {
		mpt_printf("Increase data buffer size");
		freeMem();
		return numVolumes;
	}
	ConfigRequest->Action = MPI_CONFIG_ACTION_PAGE_READ_CURRENT;
	ConfigRequest->Header.PageVersion = pReply->Header.PageVersion;
	ConfigRequest->Header.PageLength = pReply->Header.PageLength;
	ConfigRequest->PageAddress = bus;
	status = read_page2(MPT_FLAGS_KEEP_MEM);

	pIOC2 = (IOCPage2_t *) mpiBlkPtr->dataInBufPtr;
	if (status == 0){
		numVolumes = pIOC2->NumActiveVolumes;
	}
	freeMem();
	return numVolumes;
}
*/

/* This function is only written to get the information of Volume 0 */
static void GetVolumeInfo(void) {
	Config_t 		*ConfigRequest;
	ConfigReply_t 		*pReply = NULL;
	RaidVolumePage0_t 	*pRVP0 = NULL;
	RaidVol0PhysDisk_t	disk_num[16];
	unsigned char		pdisk_vol[16];
	uint numBytes;
	int status;
	int i, id;
	int pdisk_cnt = 0;
	unsigned bus = id_of_primary_device;

	numBytes = (sizeof(Config_t) - sizeof(SGE_IO_UNION)) + 
		sizeof(SGESimple64_t);
	if ((mpiBlkPtr = allocIoctlBlk(numBytes)) == NULL) {
		return;
	}

	ConfigRequest = (Config_t *) mpiBlkPtr->MF;
	mpiBlkPtr->dataInSize = mpiBlkPtr->dataOutSize = 0;
	mpiBlkPtr->dataInBufPtr = mpiBlkPtr->dataOutBufPtr = NULL;
	mpiBlkPtr->dataSgeOffset = (sizeof (Config_t) - sizeof(SGE_IO_UNION))/4;

	pReply = (ConfigReply_t *)mpiBlkPtr->replyFrameBufPtr;

	ConfigRequest->Action       = MPI_CONFIG_ACTION_PAGE_HEADER;
	ConfigRequest->Function     = MPI_FUNCTION_CONFIG;
	ConfigRequest->MsgContext   = -1;
	ConfigRequest->Header.PageType = MPI_CONFIG_PAGETYPE_RAID_VOLUME;
	ConfigRequest->Header.PageNumber = ioc_unit;
	//orig: ConfigRequest->PageAddress = cpu_to_le32((bus << 8) | 0);
	mpt_debug("DEBUG: cpu_to_le32=%d htonl=%d ntohl=%d\n",
			cpu_to_le32((bus << 8) | 0),
			htonl((bus << 8) | 0),
			ntohl((bus << 8) | 0));
	ConfigRequest->PageAddress = bus;

	status = read_page2(MPT_FLAGS_KEEP_MEM);
	if (status != 0 || pReply->Header.PageLength == 0) {
		freeMem();
		return;
	}
	mpiBlkPtr->dataInSize = pReply->Header.PageLength * 4;
	if (allocDataFrame(DATA_DIR_IN)) {
		mpt_printf("Increase data buffer size");
		freeMem();
		return;
	}
	ConfigRequest->Action = MPI_CONFIG_ACTION_PAGE_READ_CURRENT;
	ConfigRequest->Header.PageVersion = pReply->Header.PageVersion;
	ConfigRequest->Header.PageLength = pReply->Header.PageLength;

	id = 0; // volume id: only one volume for now (vol_ids)
	//orig: ConfigRequest->PageAddress = cpu_to_le32((bus << 8) | id);
	/* Something is fishy here */
	mpt_debug("DEBUG: cpu_to_le32=%d htonl=%d ntohl=%d\n",
			cpu_to_le32(bus | id),
			htonl(bus | id),
			ntohl(bus | id));
	ConfigRequest->PageAddress = bus | id;
	status = read_page2(MPT_FLAGS_KEEP_MEM);
	pRVP0 = (RaidVolumePage0_t *) mpiBlkPtr->dataInBufPtr;
	if ((status == 0) && (pRVP0->NumPhysDisks > 0)){
		if (newstyle == 0) {
			__print_volume_classic(pRVP0);
		} else {
			__print_volume_advanced(pRVP0);
		}
		for (i = pdisk_cnt; i < pdisk_cnt + pRVP0->NumPhysDisks; i++) {
			disk_num[i].PhysDiskNum = pRVP0->PhysDisk[i].PhysDiskNum;
			disk_num[i].PhysDiskMap = pRVP0->PhysDisk[i].PhysDiskMap;
			pdisk_vol[i] = pRVP0->VolumeID;
		}
		pdisk_cnt += pRVP0->NumPhysDisks;
	} else if (pRVP0->NumPhysDisks == 0) {
		mpt_printf("%s\n", wrong_scsi_id);
		mpt_exit(MPT_EXIT_UNKNOWN);
	}
	freeMem();
	/* this should be woven into the GetPhysDiskInfo part correctly */
	if (newstyle) /* && resync_on) */ {
		GetResyncPercentageSilent((RaidVol0PhysDisk_t *) &disk_num, 
			(unsigned char *) &pdisk_vol, pdisk_cnt);
	}
	if (pdisk_cnt > 0) {
		GetPhysDiskInfo((RaidVol0PhysDisk_t *) &disk_num, pdisk_cnt);
	}
	/* this should be woven into the GetPhysDiskInfo part correctly */
	if (newstyle) {
		GetHotSpareInfo();
	}
	/* this should be woven into the GetPhysDiskInfo part correctly */
	if (newstyle) /* && resync_on) */ {
		GetResyncPercentage((RaidVol0PhysDisk_t *) &disk_num, 
			(unsigned char *) &pdisk_vol, pdisk_cnt);
	}
	return;
}

static void GetPhysDiskInfo(RaidVol0PhysDisk_t *pDisk, int count) {
	Config_t *ConfigRequest;
	ConfigReply_t *pReply = NULL;
	uint numBytes;
	int  status;
	int  i;

	numBytes = (sizeof(Config_t) - sizeof(SGE_IO_UNION)) + sizeof (SGESimple64_t);
	if ((mpiBlkPtr = allocIoctlBlk(numBytes)) == NULL)
		return;

	ConfigRequest = (Config_t *) mpiBlkPtr->MF;
	mpiBlkPtr->dataInSize = mpiBlkPtr->dataOutSize = 0;
	mpiBlkPtr->dataInBufPtr = mpiBlkPtr->dataOutBufPtr = NULL;
	mpiBlkPtr->dataSgeOffset = (sizeof (Config_t) - sizeof(SGE_IO_UNION))/4;
	pReply = (ConfigReply_t *)mpiBlkPtr->replyFrameBufPtr;

	ConfigRequest->Action       = MPI_CONFIG_ACTION_PAGE_HEADER;
	ConfigRequest->Function     = MPI_FUNCTION_CONFIG;
	ConfigRequest->MsgContext   = -1;
	ConfigRequest->Header.PageType = MPI_CONFIG_PAGETYPE_RAID_PHYSDISK;
	ConfigRequest->Header.PageNumber = ioc_unit;
	ConfigRequest->PageAddress = cpu_to_le32((uint)pDisk[0].PhysDiskNum);

	status = read_page2(MPT_FLAGS_KEEP_MEM);
	if ((status == 0) && (pReply->Header.PageLength > 0)) {
		mpiBlkPtr->dataInSize = pReply->Header.PageLength * 4;
		if (allocDataFrame(DATA_DIR_IN)) {
			mpt_printf("Increase data buffer size");
			freeMem();
			return;
		}
	}

	ConfigRequest->Action = MPI_CONFIG_ACTION_PAGE_READ_CURRENT;
	ConfigRequest->Header.PageVersion = pReply->Header.PageVersion;
	ConfigRequest->Header.PageLength = pReply->Header.PageLength;
	for (i = 0; i < count; i++){
		ConfigRequest->PageAddress = cpu_to_le32((uint)pDisk[i].PhysDiskNum);

		status = read_page2(MPT_FLAGS_KEEP_MEM);
		if (status == 0) {
			RaidPhysDiskPage0_t *pRPD0 = (RaidPhysDiskPage0_t *)
						mpiBlkPtr->dataInBufPtr;
			if (newstyle == 0) {
				__print_physdisk_classic(pRPD0);
			} else {
				__print_physdisk_advanced(pRPD0, 0);
			}
		} else {
			//mpt_printf("\t\tNot Available.\n");
		}
	}
	freeMem();
	return;
}

static void GetHotSpareInfo(void) {
	Config_t *ConfigRequest;
	ConfigReply_t *pReply = NULL;
	IOCPage5_t *pPg5 = NULL;
	uint numBytes;
	int  status;
	uint num_spares = 0;

	numBytes = (sizeof(Config_t) - sizeof(SGE_IO_UNION)) + sizeof (SGESimple64_t);
	if ((mpiBlkPtr = allocIoctlBlk(numBytes)) == NULL)
		return;

	ConfigRequest = (Config_t *) mpiBlkPtr->MF;
	mpiBlkPtr->dataInSize = mpiBlkPtr->dataOutSize = 0;
	mpiBlkPtr->dataInBufPtr = mpiBlkPtr->dataOutBufPtr = NULL;
	mpiBlkPtr->dataSgeOffset = (sizeof (Config_t) - sizeof(SGE_IO_UNION))/4;

	pReply = (ConfigReply_t *)mpiBlkPtr->replyFrameBufPtr;

	ConfigRequest->Action       = MPI_CONFIG_ACTION_PAGE_HEADER;
	ConfigRequest->Function     = MPI_FUNCTION_CONFIG;
	ConfigRequest->MsgContext   = -1;
	ConfigRequest->Header.PageNumber = 5;
	ConfigRequest->Header.PageType = MPI_CONFIG_PAGETYPE_IOC;
	ConfigRequest->PageAddress = 0;

	status = read_page2(MPT_FLAGS_KEEP_MEM);
	if (status != 0 || pReply->Header.PageLength == 0) {
		freeMem();
		return;
	}
	mpiBlkPtr->dataInSize = pReply->Header.PageLength * 4;
	if (allocDataFrame(DATA_DIR_IN)) {
		mpt_printf("Increase data buffer size");
		freeMem();
		return;
	}
	ConfigRequest->Action = MPI_CONFIG_ACTION_PAGE_READ_CURRENT;
	ConfigRequest->Header.PageVersion = pReply->Header.PageVersion;
	ConfigRequest->Header.PageLength = pReply->Header.PageLength;
	status = read_page2(MPT_FLAGS_KEEP_MEM);
	pPg5 = (IOCPage5_t *) mpiBlkPtr->dataInBufPtr;

	if ((status == 0) && (pPg5->NumHotSpares > 0)){
		num_spares = pPg5->NumHotSpares;
		{
			// Get Phys Disk Information
			Ioc5HotSpare_t	disk_num[num_spares];
			unsigned int i;

			for (i = 0; i < num_spares; i++) {
				disk_num[i].PhysDiskNum = pPg5->HotSpare[i].PhysDiskNum;
				disk_num[i].HotSparePool = pPg5->HotSpare[i].HotSparePool;
			}
			freeMem(); /* Do not reference pPg5 any more */
			numBytes = (sizeof(Config_t) - sizeof(SGE_IO_UNION)) 
				+ sizeof (SGESimple64_t);
			if ((mpiBlkPtr = allocIoctlBlk(numBytes)) == NULL)
				return;
			ConfigRequest = (Config_t *) mpiBlkPtr->MF;
			mpiBlkPtr->dataInSize = mpiBlkPtr->dataOutSize = 0;
			mpiBlkPtr->dataInBufPtr = mpiBlkPtr->dataOutBufPtr = NULL;
			mpiBlkPtr->dataSgeOffset = (sizeof (Config_t) 
				- sizeof(SGE_IO_UNION))/4;
			pReply = (ConfigReply_t *)mpiBlkPtr->replyFrameBufPtr;

			ConfigRequest->Action = MPI_CONFIG_ACTION_PAGE_HEADER;
			ConfigRequest->Function     = MPI_FUNCTION_CONFIG;
			ConfigRequest->MsgContext   = -1;
			ConfigRequest->Header.PageType = MPI_CONFIG_PAGETYPE_RAID_PHYSDISK;
			ConfigRequest->PageAddress = cpu_to_le32((uint)disk_num[0].PhysDiskNum);

			status = read_page2(MPT_FLAGS_KEEP_MEM);
			if ((status == 0) && (pReply->Header.PageLength > 0)) {
				mpiBlkPtr->dataInSize = pReply->Header.PageLength * 4;
				if (allocDataFrame(DATA_DIR_IN)) {
					mpt_printf("Increase data buffer size");
					freeMem();
					return;
				}
			}

			ConfigRequest->Action = MPI_CONFIG_ACTION_PAGE_READ_CURRENT;
			ConfigRequest->Header.PageVersion = pReply->Header.PageVersion;
			ConfigRequest->Header.PageLength = pReply->Header.PageLength;
			for (i = 0; i < num_spares; i++){
				ConfigRequest->PageAddress = cpu_to_le32((uint)disk_num[i].PhysDiskNum);

				status = read_page2(MPT_FLAGS_KEEP_MEM);
				if (status == 0) {
					RaidPhysDiskPage0_t *pRPD0 = (RaidPhysDiskPage0_t *) mpiBlkPtr->dataInBufPtr;
					__print_physdisk_advanced(pRPD0, 1);
				}
			}
		}
	}
	freeMem();
	return;
}

static void GetResyncPercentageSilent(RaidVol0PhysDisk_t *pDisk, unsigned char *pVol, int count) {
	MpiRaidActionRequest_t	*pRequest;
	uint			blks_done;
	uint 			numBytes;
	int			i;
	uint			tot_blks, blks_left;
	int  			status;

	numBytes = (sizeof(MpiRaidActionRequest_t) - sizeof(SGE_IO_UNION))
		+ sizeof (SGESimple64_t);
	if ((mpiBlkPtr = allocIoctlBlk(numBytes)) == NULL)
		return;
	pRequest = (MpiRaidActionRequest_t *) mpiBlkPtr->MF;
	mpiBlkPtr->dataSgeOffset = (sizeof (MpiRaidActionRequest_t)
		- sizeof(SGE_IO_UNION))/4;
	mpiBlkPtr->dataInSize = mpiBlkPtr->dataOutSize = 0;
	pRequest->Action       = MPI_RAID_ACTION_INDICATOR_STRUCT;
	pRequest->Function     = MPI_FUNCTION_RAID_ACTION;
	pRequest->MsgContext   = -1;
	pRequest->ActionDataWord  = 0; /* action data is 0 */
	for (i = 0; i < count; i++) {
		pRequest->VolumeID     = (u8) pVol[i];
		pRequest->PhysDiskNum  = pDisk[i].PhysDiskNum;
		status = read_page2(MPT_FLAGS_KEEP_MEM);
		if (status == 0) {
			// pDisk[i].PhysDiskNum == scsi_id
			uint *pdata = (uint *) mpiBlkPtr->replyFrameBufPtr;
			pdata += 6;
			tot_blks = *pdata;
			pdata++;
			pdata++;
			blks_left = *pdata;
			pdata++;
			blks_done = tot_blks - blks_left;
			//mpt_printf("scsi_id:%d", pDisk[i].PhysDiskNum);
			if (blks_left == 0) {
				sync_state[pDisk[i].PhysDiskNum%2] = 100;
			} else {
				sync_state[pDisk[i].PhysDiskNum%2] =
					((blks_done >> 6)*100)/(tot_blks >> 6);
			}
		}
	}
	freeMem();
	return;
}

static void GetResyncPercentage(RaidVol0PhysDisk_t *pDisk, unsigned char *pVol, int count) {
	MpiRaidActionRequest_t	*pRequest;
	uint			blks_done;
	uint 			numBytes;
	int			i;
	uint			tot_blks, blks_left;
	int  			status;

	numBytes = (sizeof(MpiRaidActionRequest_t) - sizeof(SGE_IO_UNION))
		+ sizeof (SGESimple64_t);
	if ((mpiBlkPtr = allocIoctlBlk(numBytes)) == NULL)
		return;
	pRequest = (MpiRaidActionRequest_t *) mpiBlkPtr->MF;
	mpiBlkPtr->dataSgeOffset = (sizeof (MpiRaidActionRequest_t)
		- sizeof(SGE_IO_UNION))/4;

	/* Initialize data in/data out sizes: Change below if need to */
	mpiBlkPtr->dataInSize = mpiBlkPtr->dataOutSize = 0;

	pRequest->Action       = MPI_RAID_ACTION_INDICATOR_STRUCT;
	pRequest->Function     = MPI_FUNCTION_RAID_ACTION;
	pRequest->MsgContext   = -1;
	pRequest->ActionDataWord  = 0; /* action data is 0 */

	for (i = 0; i < count; i++ ) {
		pRequest->VolumeID     = (u8) pVol[i];
		pRequest->PhysDiskNum  = pDisk[i].PhysDiskNum;
		status = read_page2(MPT_FLAGS_KEEP_MEM);
		if (status == 0) {
			// pDisk[i].PhysDiskNum == scsi_id
			uint *pdata = (uint *) mpiBlkPtr->replyFrameBufPtr;
			mpt_debug("DEBUG: *pdata=%d\n", *pdata);
			pdata += 6;
			tot_blks = *pdata;
			mpt_debug("DEBUG: tot_blks=%d\n", tot_blks);
			pdata++;
			pdata++;
			blks_left = *pdata;
			mpt_debug("DEBUG: blks_left=%d\n", blks_left);
			pdata++;
			blks_done = tot_blks - blks_left;
			mpt_printf("scsi_id:%d", pDisk[i].PhysDiskNum);
			if (blks_left == 0) {
				mpt_printf(" 100%%\n");
			} else {
				mpt_printf(" %d%%\n",
					((blks_done >> 6)*100)/(tot_blks >> 6));
			}
		}
	}
	freeMem();
	return;
}

/* get resync data for volume 0 only */
static SyncInfoData __get_resync_data(void) {
	SyncInfoData data = { -1, -1, -1 };
	MpiRaidActionRequest_t	*pRequest;
	unsigned int numBytes;

	// get size for structure
	numBytes = (sizeof(Config_t) - sizeof(SGE_IO_UNION)) + sizeof(SGESimple64_t);

	// get mpi block pointer
	if ((mpiBlkPtr = allocIoctlBlk(numBytes)) == NULL ) return data;

	// set Sge offset (dunno)
	mpiBlkPtr->dataSgeOffset = (sizeof (MpiRaidActionRequest_t) - sizeof(SGE_IO_UNION))/4;

	/* Initialize data in/data out sizes: Change below if need to */
	mpiBlkPtr->dataInSize = mpiBlkPtr->dataOutSize = 0;

	// prepare request call
	pRequest = (MpiRaidActionRequest_t *) mpiBlkPtr->MF;
	pRequest->Action       = MPI_RAID_ACTION_INDICATOR_STRUCT;
	pRequest->Function     = MPI_FUNCTION_RAID_ACTION;
	pRequest->MsgContext   = -1;
	pRequest->ActionDataWord  = 0; /* action data is 0 */

	// if status is ok - read total and remaining blocks
	if(read_page2(MPT_FLAGS_KEEP_MEM)==0) {
		uint *pdata = (uint *) mpiBlkPtr->replyFrameBufPtr;

		// populate data structure - total blocks
		pdata += 6;
		data.blocks_total = *pdata;

		// populate data structure - left blocks
		pdata += 2;
		data.blocks_left = *pdata;

		// populate data structure - done blocks
		data.blocks_done = data.blocks_total - data.blocks_left;
	}

	// free unused memory
	freeMem();

	// return populated structure
	return data;
}

static void GetSyncInfo(void) {
	// data holder for rate count
	SyncInfoData	data[2];

	// get first data read
	data[0] = __get_resync_data();

	// if no blocks left to synchronize - we're synchronized, we can finish
	if( 0 == data[0].blocks_left ) {
		printf("STATUS: no resync in progress (synchronized or degraded: status unhandled).\n");

	// we're synchronizing now... count rates, times, etc...
	} else if( 0 < data[0].blocks_left ) {
		char	progress[52]	= "[                                                 ]";
		int	percent		= 0,
			diff		= 0,
			i		= 0,
			time[4];
		double	size_total	= 0,
			size_left	= 0,
			size_done	= 0,
			rate		= 0;

		// get second data probe after 0.1 sec wait
		usleep(100000);
		data[1] = __get_resync_data();

		// get basic stats (percent done and synchronization rate)
		diff		= (data[0].blocks_left - data[1].blocks_left);				// blocks done in 0.1sec
		percent		= ((data[1].blocks_done >> 6)*100)/(data[1].blocks_total >> 6);		// percent done
		rate		= ((double)diff/1048576)*5120;						// MiB/s

		size_total	= (((double)data[1].blocks_total/1048576)*512)/1024;			// total array size in GiB
		size_left	= (((double)data[1].blocks_left/1048576)*512)/1024;			// size left to synchronize in GiB
		size_done	= (((double)data[1].blocks_done/1048576)*512)/1024;			// size already synchronized in GiB

		time[3] = data[1].blocks_left/diff/10;							// total seconds left
		time[0] = time[3]/3600;									// H
		time[1] = (time[3]-(3600*time[0]))/60;							// i
		time[2] = time[3]-(3600*time[0])-(60*time[1]);						// s

		// set progress bar...
		for(i = 1; i < percent/2; i++)
		    progress[i] = '=';

		if(i==1) progress[i] = '>';
		else progress[--i] = '>';

		// if in quite_mode: only print resync status
		if(quiet_mode>0) {
			printf("STATUS: RESYNC_IN_PROGRESS %u%% (%01.0f/%01.0f GiB) @ %01.1f MiB/s, %uh %02um left\n",
				percent,
				size_done,
				size_total,
				rate,
				time[0],
				time[1]
			);

		// othewise be more verbose
		} else {
			printf("STATUS: Volume 0 array is being resynchronized\n");

			printf("STATUS: Data done: %01.3f GiB / %01.3f GiB (%01.3f GiB left)\n",
				size_done,
				size_total,
				size_left
			);

			printf("STATUS: Aproximatly time left: %02uh %02um %02us.\n",
				time[0],
				time[1],
				time[2]
			);

			printf("%u%% %s %01.2f MiB/s\n", percent, progress, rate);
		}

	} else {
		printf("STATUS: Error obtaining resync info.\n");
		mpt_exit(MPT_EXIT_UNKNOWN);
	}

	return;
}

static void __print_volume_advanced(RaidVolumePage0_t *page) {
	if (1 == print_status_only) {
		mpt_printf("vol_id:%d", page->VolumeID);
	} else {
		mpt_printf("ioc:%d vol_id:%d type:%s raidlevel:%s num_disks:%d size(GB):%d",
			page->VolumeIOC,
			page->VolumeID,
			page->VolumeType < sizeof(VolumeTypes) ?
				VolumeTypes[page->VolumeType] : "unknown",
			page->VolumeType < sizeof(VolumeTypes) ?
				VolumeTypesHuman[page->VolumeType] : "",
			page->NumPhysDisks,
			page->MaxLBA / (2 * 1024 * 1024));
		mpt_printf(" state:");
	}
	if (page->VolumeStatus.State == MPI_RAIDVOL0_STATUS_STATE_OPTIMAL) {
		mpt_exit_mask |= MPT_EXIT_VOL_OPTIMAL;
		mpt_printf(" OPTIMAL");
	} else if (page->VolumeStatus.State == 
	    MPI_RAIDVOL0_STATUS_STATE_DEGRADED) {
		mpt_exit_mask |= MPT_EXIT_VOL_DEGRADED;
		mpt_printf(" DEGRADED");
	} else if (page->VolumeStatus.State == 
	    MPI_RAIDVOL0_STATUS_STATE_FAILED) {
		mpt_exit_mask |= MPT_EXIT_VOL_FAILED;
		mpt_printf(" FAILED");
	} else {
		mpt_exit_mask |= MPT_EXIT_UNKNOWN;
		mpt_printf(" UNKNOWN");
	}
	if (1 != print_status_only) {
		mpt_printf(" flags:");
		if (page->VolumeStatus.Flags != 0) {
			if (page->VolumeStatus.Flags &
			    MPI_RAIDVOL0_STATUS_FLAG_ENABLED)
				mpt_printf(" ENABLED");
			if (page->VolumeStatus.Flags &
			    MPI_RAIDVOL0_STATUS_FLAG_QUIESCED)
				mpt_printf(" QUIESCED");
			if (page->VolumeStatus.Flags &
			    MPI_RAIDVOL0_STATUS_FLAG_RESYNC_IN_PROGRESS) {
				mpt_printf(" RESYNC_IN_PROGRESS");
				resync_on = 1;
			}
			if (page->VolumeStatus.Flags &
			    MPI_RAIDVOL0_STATUS_FLAG_VOLUME_INACTIVE)
				mpt_printf(" VOLUME_INACTIVE");
		} else {
			mpt_printf(" NONE");
		}
	}
	/* since mpt_printf() returns without output when quiet mode is enabled
	   we need to explicitly write that CR
	 */
	printf("\n");
	/*
	    CONFIG_PAGE_HEADER      Header;         // 00h
	    U8                      VolumeID;       // 04h
	    U8                      VolumeBus;      // 05h
	    U8                      VolumeIOC;      // 06h
	    U8                      VolumeType;     // 07h
	    RAID_VOL0_STATUS        VolumeStatus;   // 08h
	    RAID_VOL0_SETTINGS      VolumeSettings; // 0Ch
	    U32                     MaxLBA;         // 10h
	    U32                     Reserved1;      // 14h
	    U32                     StripeSize;     // 18h
	    U32                     Reserved2;      // 1Ch
	    U32                     Reserved3;      // 20h
	    U8                      NumPhysDisks;   // 24h
	    U8                      DataScrubRate;  // 25h
	    U8                      ResyncRate;     // 26h
	    U8                      InactiveStatus; // 27h
	    RAID_VOL0_PHYS_DISK     PhysDisk[MPI_RAID_VOL_PAGE_0_PHYSDISK_MAX];
	*/
	//mpt_debug(VolumeID);
}

static void __print_volume_classic(RaidVolumePage0_t *page) {
	/* This function will go away for the next big release */
	if (1 == print_status_only) {
		mpt_printf("log_id %d", page->VolumeID);
	} else {
		mpt_printf("ioc%d vol_id %d type %s, %d phy, %d GB",
			page->VolumeIOC,
			page->VolumeID,
			sizeof(VolumeTypes) ? VolumeTypes[page->VolumeType] :
				"unknown",
			page->NumPhysDisks,
			page->MaxLBA / (2 * 1024 * 1024));
		mpt_printf(", state");
	}
	if (page->VolumeStatus.State == MPI_RAIDVOL0_STATUS_STATE_OPTIMAL) {
		mpt_exit_mask |= MPT_EXIT_VOL_OPTIMAL;
		mpt_printf(" OPTIMAL");
	} else if (page->VolumeStatus.State == 
	    MPI_RAIDVOL0_STATUS_STATE_DEGRADED) {
		mpt_exit_mask |= MPT_EXIT_VOL_DEGRADED;
		mpt_printf(" DEGRADED");
	} else if (page->VolumeStatus.State == 
	    MPI_RAIDVOL0_STATUS_STATE_FAILED) {
		mpt_exit_mask |= MPT_EXIT_VOL_FAILED;
		mpt_printf(" FAILED");
	} else {
		mpt_exit_mask |= MPT_EXIT_UNKNOWN;
		mpt_printf(" UNKNOWN");
	}
	if (1 != print_status_only) {
		mpt_printf(", flags");
		if (page->VolumeStatus.Flags != 0) {
			if (page->VolumeStatus.Flags &
			    MPI_RAIDVOL0_STATUS_FLAG_ENABLED)
				mpt_printf(" ENABLED");
			if (page->VolumeStatus.Flags &
			    MPI_RAIDVOL0_STATUS_FLAG_QUIESCED)
				mpt_printf(" QUIESCED");
			if (page->VolumeStatus.Flags &
			    MPI_RAIDVOL0_STATUS_FLAG_RESYNC_IN_PROGRESS) {
				mpt_printf(" RESYNC_IN_PROGRESS");
				resync_on = 1;
			}
			if (page->VolumeStatus.Flags &
			    MPI_RAIDVOL0_STATUS_FLAG_VOLUME_INACTIVE)
				mpt_printf(" VOLUME_INACTIVE");
		} else {
			mpt_printf(" NONE");
		}
	}
	/* since mpt_printf() returns without output when quiet mode is enabled
	   we need to explicitly write that CR
	 */
	printf("\n");
}

static void __print_physdisk_advanced(RaidPhysDiskPage0_t *phys, int spare) {
	char vendor[BIG];
	char productid[BIG];
	char rev[BIG];

	memset(vendor, 0, sizeof(vendor));
	memset(productid, 0, sizeof(productid));
	memset(rev, 0, sizeof(rev));

	strncpy(vendor, (void *) phys->InquiryData.VendorID,
		sizeof(phys->InquiryData.VendorID));
	strncpy(productid, (void *) phys->InquiryData.ProductID,
		sizeof(phys->InquiryData.ProductID));
	strncpy(rev, (void *) phys->InquiryData.ProductRevLevel,
		sizeof(phys->InquiryData.ProductRevLevel));

	if (1 == print_status_only) {
		printf("%s:%d", spare == 1 ? "spare_id" : "phys_id",
			phys->PhysDiskNum);
	} else {
		printf("ioc:%d %s:%d scsi_id:%d vendor:%s product_id:%s revision:%s size(GB):%d",
			phys->PhysDiskIOC,
			spare == 1 ? "spare_id" : "phys_id",
			phys->PhysDiskNum,
			phys->PhysDiskID,
			vendor,
			productid,
			rev,
			phys->MaxLBA / (2 * 1024 * 1024));
		printf(" state:");
	}
	if (phys->PhysDiskStatus.State ==
	    MPI_PHYSDISK0_STATUS_ONLINE) {
		mpt_exit_mask |= MPT_EXIT_OKAY;
		printf(" ONLINE");
	} else if (phys->PhysDiskStatus.State ==
	    MPI_PHYSDISK0_STATUS_MISSING) {
		mpt_exit_mask |= MPT_EXIT_PHYSDISK_ERROR;
		printf(" MISSING");
	} else if (phys->PhysDiskStatus.State ==
	    MPI_PHYSDISK0_STATUS_NOT_COMPATIBLE) {
		mpt_exit_mask |= MPT_EXIT_PHYSDISK_ERROR;
		printf(" NOT_COMPATIBLE");
	} else if (phys->PhysDiskStatus.State ==
	    MPI_PHYSDISK0_STATUS_FAILED) {
		mpt_exit_mask |= MPT_EXIT_PHYSDISK_ERROR;
		printf(" FAILED");
	} else if (phys->PhysDiskStatus.State ==
	    MPI_PHYSDISK0_STATUS_INITIALIZING) {
		mpt_exit_mask |= MPT_EXIT_PHYSDISK_WARN;
		printf(" INITIALIZING");
	} else if (phys->PhysDiskStatus.State ==
	    MPI_PHYSDISK0_STATUS_OFFLINE_REQUESTED) {
		mpt_exit_mask |= MPT_EXIT_PHYSDISK_WARN;
		printf(" OFFLINE_REQUESTED");
	} else if (phys->PhysDiskStatus.State ==
	    MPI_PHYSDISK0_STATUS_FAILED_REQUESTED) {
		mpt_exit_mask |= MPT_EXIT_PHYSDISK_ERROR;
		printf(" FAILED_REQUESTED");
	} else if (phys->PhysDiskStatus.State ==
	    MPI_PHYSDISK0_STATUS_OTHER_OFFLINE) {
		mpt_exit_mask |= MPT_EXIT_PHYSDISK_WARN;
		printf(" OTHER_OFFLINE");
	} else {
		mpt_exit_mask |= MPT_EXIT_UNKNOWN;
		mpt_printf(" UNKNOWN");
	}
	if (1 != print_status_only) {
		mpt_printf(" flags:");
		if (phys->PhysDiskStatus.Flags) {
			if (phys->PhysDiskStatus.Flags &
			    MPI_PHYSDISK0_STATUS_FLAG_OUT_OF_SYNC)
				mpt_printf(" OUT_OF_SYNC");
			if (phys->PhysDiskStatus.Flags &
			    MPI_PHYSDISK0_STATUS_FLAG_QUIESCED)
				mpt_printf(" QUIESCED");
		} else {
			mpt_printf(" NONE");
		}
		/* SYNC RATE */
		//mpt_printf(" sync_state: %d%%", GetResyncPercentageDisk(phys));
		/* this is such a gross hack it's almost unbelieveable ... 
		   it will be fixed in the next release */
		if (spare != 1) {
			mpt_printf(" sync_state: %d",
				sync_state[phys->PhysDiskID%2]);
		} else {
			mpt_printf(" sync_state: n/a");
		}
		/* ASC/ASCQ information */
		mpt_printf(" ASC/ASCQ:0x%02x/0x%02x SMART ASC/ASCQ:0x%02x/0x%02x",
			phys->ErrorData.ErrorASC,
			phys->ErrorData.ErrorASCQ,
			phys->ErrorData.SmartASC,
			phys->ErrorData.SmartASCQ);
	}
	/* since mpt_printf() returns without output when quiet mode is enabled
	   we need to explicitly write that CR
	 */
	printf("\n");
}

static void __print_physdisk_classic(RaidPhysDiskPage0_t *phys) {
	/* This function will go away for the next big release */
	char vendor[BIG];
	char productid[BIG];
	char rev[BIG];

	memset(vendor, 0, sizeof(vendor));
	memset(productid, 0, sizeof(productid));
	memset(rev, 0, sizeof(rev));

	strncpy(vendor, (void *) phys->InquiryData.VendorID,
		sizeof(phys->InquiryData.VendorID));
	strncpy(productid, (void *) phys->InquiryData.ProductID,
		sizeof(phys->InquiryData.ProductID));
	strncpy(rev, (void *) phys->InquiryData.ProductRevLevel,
		sizeof(phys->InquiryData.ProductRevLevel));

	if (1 == print_status_only) {
		printf("phys_id %d", phys->PhysDiskNum);
	} else {
		printf("ioc%d phy %d scsi_id %d %s %s %s, %d GB",
		       phys->PhysDiskIOC,
		       phys->PhysDiskNum,
		       phys->PhysDiskID,
		       vendor,
		       productid,
		       rev,
		       phys->MaxLBA / (2 * 1024 * 1024));
		printf(", state");
	}
	if (phys->PhysDiskStatus.State ==
	    MPI_PHYSDISK0_STATUS_ONLINE) {
		mpt_exit_mask |= MPT_EXIT_OKAY;
		printf(" ONLINE");
	} else if (phys->PhysDiskStatus.State ==
	    MPI_PHYSDISK0_STATUS_MISSING) {
		mpt_exit_mask |= MPT_EXIT_PHYSDISK_ERROR;
		printf(" MISSING");
	} else if (phys->PhysDiskStatus.State ==
	    MPI_PHYSDISK0_STATUS_NOT_COMPATIBLE) {
		mpt_exit_mask |= MPT_EXIT_PHYSDISK_ERROR;
		printf(" NOT_COMPATIBLE");
	} else if (phys->PhysDiskStatus.State ==
	    MPI_PHYSDISK0_STATUS_FAILED) {
		mpt_exit_mask |= MPT_EXIT_PHYSDISK_ERROR;
		printf(" FAILED");
	} else if (phys->PhysDiskStatus.State ==
	    MPI_PHYSDISK0_STATUS_INITIALIZING) {
		mpt_exit_mask |= MPT_EXIT_PHYSDISK_WARN;
		printf(" INITIALIZING");
	} else if (phys->PhysDiskStatus.State ==
	    MPI_PHYSDISK0_STATUS_OFFLINE_REQUESTED) {
		mpt_exit_mask |= MPT_EXIT_PHYSDISK_WARN;
		printf(" OFFLINE_REQUESTED");
	} else if (phys->PhysDiskStatus.State ==
	    MPI_PHYSDISK0_STATUS_FAILED_REQUESTED) {
		mpt_exit_mask |= MPT_EXIT_PHYSDISK_ERROR;
		printf(" FAILED_REQUESTED");
	} else if (phys->PhysDiskStatus.State ==
	    MPI_PHYSDISK0_STATUS_OTHER_OFFLINE) {
		mpt_exit_mask |= MPT_EXIT_PHYSDISK_WARN;
		printf(" OTHER_OFFLINE");
	} else {
		mpt_exit_mask |= MPT_EXIT_UNKNOWN;
		mpt_printf(" UNKNOWN");
	}
	if (1 != print_status_only) {
		mpt_printf(", flags");
		if (phys->PhysDiskStatus.Flags) {
			if (phys->PhysDiskStatus.Flags &
			    MPI_PHYSDISK0_STATUS_FLAG_OUT_OF_SYNC)
				mpt_printf(" OUT_OF_SYNC");
			if (phys->PhysDiskStatus.Flags &
			    MPI_PHYSDISK0_STATUS_FLAG_QUIESCED)
				mpt_printf(" QUIESCED");
		} else {
			mpt_printf(" NONE");
		}
	}
	/* since mpt_printf() returns without output when quiet mode is enabled
	   we need to explicitly write that CR
	 */
	printf("\n");
}

static void do_init(void) {
	static const char* devnames[] = {
		"/dev/mptctl",
		"/dev/mpt2ctl",
		"/dev/mpt3ctl",
		NULL
	};
	int save_errno = 0;
	const char *save_devname = NULL;
	int status;
	int dev_idx;

	if (auto_load > 0) {
		status = system("/sbin/modprobe mptctl");
		if (status < 0 || 0 != WEXITSTATUS(status)) {
			mpt_fprintf(stderr, "Failed to load mptctl\n");
			mpt_exit(MPT_EXIT_UNKNOWN|MPT_EXIT_NOCLOSE);
		}
	}

	for (dev_idx = 0; devnames[dev_idx] != NULL; dev_idx++) {
		m = open("/dev/mptctl", O_RDWR);
		if (m > -1)
			return m;

		// Set priorities among the error codes for the most interesting by the order:
		// EACCESS, ENODEV, ENOENT
		if (errno == EACCES ||
			(save_errno != EACCES && errno == ENODEV) ||
			save_errno = 0)
		{
			save_errno = errno;
			save_devname = devnames[dev_idx];
		}
	}

	perror("Failed to open an mpt ctl device");
	if (save_errno == EACCES) {
		mpt_fprintf(stderr, " Need root to run this program\n");
	} else if (save_errno == ENOENT) {
		mpt_fprintf(stderr, 
				"  There is no mptctl, mpt2ctl or mpt3ctl device, do you have an LSI MPT device at all?\n");
	} else if (save_errno == ENODEV) {
		mpt_fprintf(stderr,
				"  Are you sure your controller"
				" is supported by mptlinux?\n");
	}
	mpt_fprintf(stderr,
			"Make sure mptctl is loaded into the kernel\n");
	mpt_exit(MPT_EXIT_UNKNOWN|MPT_EXIT_NOCLOSE);
}

int main(int argc, char *argv[]) {
	int next_option;
	int option_index;
	char *progname = argv[0];

	do {
		next_option = getopt_long(argc, argv,
			short_options, long_options, &option_index);
		switch (next_option) {
		case 'd':
			debug = 1;
			debug_level = strtoul(optarg, NULL, 10);
			break;
		case 'h':
			print_usage(progname);
			mpt_exit(MPT_EXIT_OKAY);
			break;
		case 'i':
			id_of_primary_device = strtoul(optarg, NULL, 10);
			break;
		case 'n':
			newstyle = 1;
			break;
		case 'p':
			probe_id = 1;
			break;
		case 'q':
			quiet_mode = 1;
			break;
		case 's':
			print_status_only = 1;
			break;
		case 'u':
			ioc_unit = strtoul(optarg, NULL, 10);
			break;
		case 'v':
			verbose_mode = 1;
			break;
		case 'V':
			print_version();
			mpt_exit(MPT_EXIT_OKAY);
			break;
		case  0:
			if (sel == ARG_M_A) auto_load++;
			if (sel == ARG_M_S) sync_info = 1;

			break;
		case -1:
			// Done with options
			break;
		default:
			print_usage(progname);
			mpt_exit(MPT_EXIT_UNKNOWN);
			break;
		}
	} while (next_option != -1);

	do_init();
	if (newstyle > 0) {
		__check_endianess();
		if (probe_id > 0) {
			id_of_primary_device = __probe_scsi_id2();
		}
		/* This will be enabled and properly coded after the release
		if (hasVolume() > 0) {
			GetVolumeInfo();
		} else {
			mpt_printf("No RAID is configured\n");
			mpt_exit(MPT_EXIT_UNKNOWN);
		}
		*/
	} else {
		/* this is the old style setup */
		if (probe_id > 0) {
			id_of_primary_device = __probe_scsi_id2();
			if (-1 == id_of_primary_device) {
				mpt_printf("Nothing found, contact the author\n");
				mpt_exit(MPT_EXIT_UNKNOWN);
			} else {
				printf("Found SCSI id=%d, use ''mpt-status "
				       "-i %d`` to get more information.\n",
					id_of_primary_device,
					id_of_primary_device);
				mpt_exit(MPT_EXIT_OKAY);
			}
		}
	}

	if (sync_info)
		GetSyncInfo();
	else
		GetVolumeInfo();

	return mpt_exit_mask;
}
