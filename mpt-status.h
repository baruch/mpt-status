#ifndef _MPT_STATUS_H
#define _MPT_STATUS_H

#include <sys/ioctl.h>
#ifdef __linux__
#include <linux/compiler.h>
#endif

#ifdef SANITIZED_KERNEL_HEADERS
#include "mpt-sanitized.h"
#else
#ifndef __user
#define __user
#endif
#ifndef __kernel
#define __kernel
#endif
#include "pci.h"	// config.h and header.h from pciutils package
#include "lsi/mpi_type.h"
#include "lsi/mpi.h"
#include "lsi/mpi_ioc.h"
#include "lsi/mpi_cnfg.h"
#include "lsi/mpi_raid.h"
#include "mptctl.h"
//#include "mptbase.h"
#endif // SANITIZED_KERNEL_HEADERS

#define VERSION "1.2.0"

#define BIG 1024
#define REALLYBIG 10240
#define PRINT_STATUS_ONLY 0

#define REPLY_SIZE 128
#define MPT_EXIT_OKAY		(0)
#define MPT_EXIT_VOL_OPTIMAL	(0)
#define MPT_EXIT_VOL_FAILED	(1 << 1)
#define MPT_EXIT_VOL_DEGRADED	(1 << 2)
#define MPT_EXIT_VOL_RESYNC	(1 << 3)
#define MPT_EXIT_PHYSDISK_ERROR	(1 << 4)
#define MPT_EXIT_PHYSDISK_WARN	(1 << 5)
#define MPT_EXIT_UNKNOWN	(1 << 0)

#define MPT_EXIT_NOCLOSE	(1 << 10)
#define MPT_EXIT_FREEMEM	(1 << 11)

#define DATA_DIR_NONE           0
#define DATA_DIR_IN             1
#define DATA_DIR_OUT            2

#define MPT_FLAGS_FREE_MEM      0x00
#define MPT_FLAGS_KEEP_MEM      0x01
#define MPT_FLAGS_DUMP_REPLY    0x02
#define MPT_FLAGS_DUMP_DATA     0x04

static char *wrong_scsi_id =
"\nYou seem to have no SCSI disks attached to your HBA or you have\n"
"them on a different scsi_id. To get your SCSI id, run:\n\n"
"    mpt-status -p\n";

typedef struct mpt_ioctl_command mpiIoctlBlk_t;

#endif /* End of mpt-status.h */
