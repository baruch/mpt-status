# About

The mpt-status software is a query tool to access the running configuration and
status of LSI SCSI HBAs. This is a completely rewritten version of the original
mpt-status-1.0 tool written by Matt Braithwaite. mpt-status allows you to
monitor the health and status of your RAID setup.

Currently supported and tested HBAs are:

LSI 1030 SCSI RAID storage controller
LSI SAS1064 SCSI RAID storage controller
LSI SAS1068 SCSI RAID storage controller
LSI SAS 3442-R SCSI RAID storage controller

Since the tool is using the MPI (message passing interface) chances are high
that the basic information regarding RAID status will be available for all
LSI based controllers. Just give it a try and report back.


# Requirements

You should have mptbase and mptctl loaded or compiled into the kernel.


# Reported working hardware configuration

Sun Fire X4100
Sun Fire X4200
Sun Fire V20z
Sun Fire V40z
Dell PE2600
Intel Server with SE7520BD2S boards
HP ProLiant DL320 G4
IBM eServer BladeCenter LS20


# Where can I get this fine piece of software?

Homepage of maintained version: http://www.drugphish.ch/~ratz/mpt-status/

The original version of this tool is not maintained anymore by its author. He
was kind enough to put a link to the current mpt-status home. Thanks, Matt.


# Distro related support

To my avail following distributions have mpt-status included:

| Distro | Status    | URL |
| ------ | --------- | --- |
| Debian | OK        | http://packages.debian.org/unstable/admin/mpt-status |
| Suse	 | NOK (old) | http://www.novell.com/products/linuxpackages/professional/mpt-status.html |
|	 | NOK (old) | http://www.novell.com/products/linuxpackages/enterpriseserver/SP3/ia64/mpt-status.html |

Support for following distros can be found through google and rpm searches 
(status unknown):

    PLD 
    Turbolinux

I have provided spec (for RPM) files for OpenSuse and Red Hat, however they are
mostly untested. Have a look into the contrib sub directory.


# How can I compile, install or uninstall mpt-status?

Read doc/INSTALL for further information on building mpt-status. Basically you
should be fine invoking

    make

A simple (as root)

    make install install_doc

should install the binary to $PREFIX/sbin, and can be uninstalled as follows:

    make uninstall


# What does this software provide you?

For now it should be improved regarding compilation on different systems, also
different kernels. But the output has changed as well. Example:

    SR2400:~# ./mpt-status -i 1
    ioc0 vol_id 1 type IM, 2 phy, 68 GB, state OPTIMAL, flags ENABLED
    ioc0 phy 0 scsi_id 1 SEAGATE  ST373207LC       0003, 68 GB, state ONLINE, flags NONE
    ioc0 phy 1 scsi_id 3 SEAGATE  ST373207LC       0003, 68 GB, state ONLINE, flags NONE

    [root@redhatAS-3-U4-X86_64 mpt-status-1.1.5-new]# ./mpt-status

    You seem to have no SCSI disks attached to your HBA or you have
    them on a different scsi_id. To get your SCSI id, run:

    mpt-status -p

    [root@redhatAS-3-U4-X86_64 mpt-status-1.1.5-new]# ./mpt-status -p
    Found SCSI id=2, use ''mpt-status -i 2`` to get more information.

    [root@redhatAS-3-U4-X86_64 mpt-status-1.1.5-new]# ./mpt-status -i 2
    ioc0 vol_id 2 type IM, 2 phy, 67 GB, state OPTIMAL, flags ENABLED
    ioc0 phy 1 scsi_id 4 FUJITSU  MAV2073RCSUN72G  0301, 68 GB, state ONLINE, flags NONE
    ioc0 phy 0 scsi_id 3 FUJITSU  MAV2073RCSUN72G  0301, 68 GB, state ONLINE, flags NONE

    [root@redhatAS-3-U4-X86_64 mpt-status-1.1.5-new]# ./mpt-status -i 2 -s
    vol_id 2 OPTIMAL
    phys_id 1 ONLINE
    phys_id 0 ONLINE

A new style output has made it into the sources, however due to backwards
compatibility reasons it's not on per default. You can enable the new style
output by invoking mpt-status as follows:

    mpt-status --newstyle

There is also a debug mode since the mpt-status-1.2.0 release.


# Hardware, Software and Distribution Compatibility

The mpt-status software is known and reported to compile and work on following
Linux distributions:

* SuSE		: 9.0, 9.1, 9.2, 9.3, 10.0, 10.1, 10.2, SLES8, SLES9, SLES10
* Redhat	: 7.3, 9.0, RHEL3, RHEL4, FC1, FC2, FC3, FC4, FC5
* Debian	: all so far
* Ubuntu	: edgy
* Gentoo	: 2005.x
* ulibc-based	: all

Kernels:
Linux: 2.4.x, 2.6.x

Architectures:
* i386
* x86\_64


# List of Hardware configuration

This list should eventually be exported somewhere so decision makers and 
hardware buyers can easily find out if their hardware is a) supported and
b) can be monitored.

If you have successfully running mpt-status on a platform not mentioned below,
please send me following output:

* dmesg -s 1000000 (best after having rebooted your machine)
* lspci -v
* dmidecode
