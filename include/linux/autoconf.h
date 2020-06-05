/*
 * Automatically generated C config: don't edit
 * Linux kernel version: 2.6.12
 * Fri Jun 13 13:49:14 2014
 */
#define AUTOCONF_INCLUDED
#define CONFIG_ARM 1
#define CONFIG_MMU 1
#define CONFIG_UID16 1
#define CONFIG_RWSEM_GENERIC_SPINLOCK 1
#define CONFIG_GENERIC_CALIBRATE_DELAY 1
#define CONFIG_GENERIC_IOMAP 1

/*
 * Code maturity level options
 */
#define CONFIG_EXPERIMENTAL 1
#define CONFIG_CLEAN_COMPILE 1
#define CONFIG_BROKEN_ON_SMP 1
#define CONFIG_INIT_ENV_ARG_LIMIT 32

/*
 * General setup
 */
#define CONFIG_LOCALVERSION ""
#undef CONFIG_SWAP
#define CONFIG_SYSVIPC 1
#define CONFIG_POSIX_MQUEUE 1
#define CONFIG_BSD_PROCESS_ACCT 1
#undef CONFIG_BSD_PROCESS_ACCT_V3
#define CONFIG_SYSCTL 1
#undef CONFIG_AUDIT
#define CONFIG_HOTPLUG 1
#define CONFIG_KOBJECT_UEVENT 1
#undef CONFIG_IKCONFIG
#define CONFIG_EMBEDDED 1
#define CONFIG_KALLSYMS 1
#undef CONFIG_KALLSYMS_EXTRA_PASS
#define CONFIG_PRINTK 1
#define CONFIG_BUG 1
#define CONFIG_BASE_FULL 1
#define CONFIG_FUTEX 1
#define CONFIG_EPOLL 1
#define CONFIG_CC_OPTIMIZE_FOR_SIZE 1
#define CONFIG_SHMEM 1
#define CONFIG_CC_ALIGN_FUNCTIONS 0
#define CONFIG_CC_ALIGN_LABELS 0
#define CONFIG_CC_ALIGN_LOOPS 0
#define CONFIG_CC_ALIGN_JUMPS 0
#undef CONFIG_TINY_SHMEM
#define CONFIG_BASE_SMALL 0

/*
 * Loadable module support
 */
#undef CONFIG_MODULES

/*
 * System Type
 */
#undef CONFIG_ARCH_CLPS7500
#undef CONFIG_ARCH_CLPS711X
#undef CONFIG_ARCH_CO285
#undef CONFIG_ARCH_EBSA110
#undef CONFIG_ARCH_CAMELOT
#undef CONFIG_ARCH_FOOTBRIDGE
#undef CONFIG_ARCH_INTEGRATOR
#undef CONFIG_ARCH_IOP3XX
#undef CONFIG_ARCH_IXP4XX
#undef CONFIG_ARCH_IXP2000
#undef CONFIG_ARCH_L7200
#undef CONFIG_ARCH_PXA
#undef CONFIG_ARCH_RPC
#undef CONFIG_ARCH_SA1100
#undef CONFIG_ARCH_S3C2410
#undef CONFIG_ARCH_SHARK
#undef CONFIG_ARCH_LH7A40X
#undef CONFIG_ARCH_OMAP
#undef CONFIG_ARCH_VERSATILE
#undef CONFIG_ARCH_IMX
#undef CONFIG_ARCH_H720X
#define CONFIG_ARCH_AT91RM9200 1

/*
 * AT91RM9200 Implementations
 */
#define CONFIG_ARCH_AT91RM9200DK 1
#undef CONFIG_MACH_AT91RM9200EK
#undef CONFIG_MACH_CSB337
#undef CONFIG_MACH_CSB637
#undef CONFIG_MACH_CARMEVA

/*
 * Processor Type
 */
#define CONFIG_CPU_32 1
#define CONFIG_CPU_ARM920T 1
#define CONFIG_CPU_32v4 1
#define CONFIG_CPU_ABRT_EV4T 1
#define CONFIG_CPU_CACHE_V4WT 1
#define CONFIG_CPU_CACHE_VIVT 1
#define CONFIG_CPU_COPY_V4WB 1
#define CONFIG_CPU_TLB_V4WBI 1

/*
 * Processor Features
 */
#define CONFIG_ARM_THUMB 1
#undef CONFIG_CPU_ICACHE_DISABLE
#undef CONFIG_CPU_DCACHE_DISABLE
#undef CONFIG_CPU_DCACHE_WRITETHROUGH

/*
 * Bus support
 */
#define CONFIG_ISA_DMA_API 1

/*
 * PCCARD (PCMCIA/CardBus) support
 */
#undef CONFIG_PCCARD

/*
 * Kernel Features
 */
#undef CONFIG_SMP
#undef CONFIG_PREEMPT
#undef CONFIG_DISCONTIGMEM
#undef CONFIG_LEDS
#define CONFIG_ALIGNMENT_TRAP 1

/*
 * Boot options
 */
#define CONFIG_ZBOOT_ROM_TEXT 0x0
#define CONFIG_ZBOOT_ROM_BSS 0x0
#define CONFIG_CMDLINE ""
#undef CONFIG_XIP_KERNEL

/*
 * Floating point emulation
 */

/*
 * At least one emulation must be selected
 */
#define CONFIG_FPE_NWFPE 1
#define CONFIG_FPE_NWFPE_XP 1
#define CONFIG_FPE_FASTFPE 1

/*
 * Userspace binary formats
 */
#define CONFIG_BINFMT_ELF 1
#undef CONFIG_BINFMT_AOUT
#undef CONFIG_BINFMT_MISC
#undef CONFIG_ARTHUR

/*
 * Power management options
 */
#undef CONFIG_PM

/*
 * Device Drivers
 */

/*
 * Generic Driver Options
 */
#define CONFIG_STANDALONE 1
#undef CONFIG_PREVENT_FIRMWARE_BUILD
#define CONFIG_FW_LOADER 1

/*
 * Memory Technology Devices (MTD)
 */
#undef CONFIG_MTD

/*
 * Parallel port support
 */
#undef CONFIG_PARPORT

/*
 * Plug and Play support
 */

/*
 * Block devices
 */
#undef CONFIG_BLK_DEV_COW_COMMON
#define CONFIG_BLK_DEV_LOOP 1
#undef CONFIG_BLK_DEV_CRYPTOLOOP
#define CONFIG_BLK_DEV_NBD 1
#undef CONFIG_BLK_DEV_UB
#define CONFIG_BLK_DEV_RAM 1
#define CONFIG_BLK_DEV_RAM_COUNT 4
#define CONFIG_BLK_DEV_RAM_SIZE 6144
#define CONFIG_BLK_DEV_INITRD 1
#define CONFIG_INITRAMFS_SOURCE ""
#undef CONFIG_CDROM_PKTCDVD

/*
 * IO Schedulers
 */
#define CONFIG_IOSCHED_NOOP 1
#define CONFIG_IOSCHED_AS 1
#undef CONFIG_IOSCHED_DEADLINE
#undef CONFIG_IOSCHED_CFQ
#undef CONFIG_ATA_OVER_ETH
#undef CONFIG_AT91_SDCARD

/*
 * SCSI device support
 */
#define CONFIG_SCSI 1
#define CONFIG_SCSI_PROC_FS 1

/*
 * SCSI support type (disk, tape, CD-ROM)
 */
#define CONFIG_BLK_DEV_SD 1
#undef CONFIG_CHR_DEV_ST
#undef CONFIG_CHR_DEV_OSST
#undef CONFIG_BLK_DEV_SR
#define CONFIG_CHR_DEV_SG 1

/*
 * Some SCSI devices (e.g. CD jukebox) support multiple LUNs
 */
#define CONFIG_SCSI_MULTI_LUN 1
#undef CONFIG_SCSI_CONSTANTS
#undef CONFIG_SCSI_LOGGING

/*
 * SCSI Transport Attributes
 */
#define CONFIG_SCSI_SPI_ATTRS 1
#define CONFIG_SCSI_FC_ATTRS 1
#define CONFIG_SCSI_ISCSI_ATTRS 1

/*
 * SCSI low-level drivers
 */
#undef CONFIG_SCSI_SATA
#undef CONFIG_SCSI_DEBUG

/*
 * Multi-device support (RAID and LVM)
 */
#undef CONFIG_MD

/*
 * Fusion MPT device support
 */

/*
 * IEEE 1394 (FireWire) support
 */

/*
 * I2O device support
 */

/*
 * Networking support
 */
#define CONFIG_NET 1

/*
 * Networking options
 */
#define CONFIG_PACKET 1
#undef CONFIG_PACKET_MMAP
#define CONFIG_UNIX 1
#undef CONFIG_NET_KEY
#define CONFIG_INET 1
#undef CONFIG_IP_MULTICAST
#undef CONFIG_IP_ADVANCED_ROUTER
#define CONFIG_IP_PNP 1
#undef CONFIG_IP_PNP_DHCP
#undef CONFIG_IP_PNP_BOOTP
#undef CONFIG_IP_PNP_RARP
#undef CONFIG_NET_IPIP
#undef CONFIG_NET_IPGRE
#undef CONFIG_ARPD
#undef CONFIG_SYN_COOKIES
#undef CONFIG_INET_AH
#undef CONFIG_INET_ESP
#undef CONFIG_INET_IPCOMP
#undef CONFIG_INET_TUNNEL
#define CONFIG_IP_TCPDIAG 1
#undef CONFIG_IP_TCPDIAG_IPV6
#undef CONFIG_IPV6
#undef CONFIG_NETFILTER

/*
 * SCTP Configuration (EXPERIMENTAL)
 */
#undef CONFIG_IP_SCTP
#undef CONFIG_ATM
#undef CONFIG_BRIDGE
#undef CONFIG_VLAN_8021Q
#undef CONFIG_DECNET
#undef CONFIG_LLC2
#undef CONFIG_IPX
#undef CONFIG_ATALK
#undef CONFIG_X25
#undef CONFIG_LAPB
#undef CONFIG_NET_DIVERT
#undef CONFIG_ECONET
#undef CONFIG_WAN_ROUTER

/*
 * QoS and/or fair queueing
 */
#undef CONFIG_NET_SCHED
#undef CONFIG_NET_CLS_ROUTE

/*
 * Network testing
 */
#undef CONFIG_NET_PKTGEN
#undef CONFIG_NETPOLL
#undef CONFIG_NET_POLL_CONTROLLER
#undef CONFIG_HAMRADIO
#undef CONFIG_IRDA
#undef CONFIG_BT
#define CONFIG_NETDEVICES 1
#undef CONFIG_DUMMY
#undef CONFIG_BONDING
#undef CONFIG_EQUALIZER
#undef CONFIG_TUN

/*
 * Ethernet (10 or 100Mbit)
 */
#define CONFIG_NET_ETHERNET 1
#define CONFIG_MII 1
#define CONFIG_ARM_AT91_ETHER 1
#undef CONFIG_SMC91X

/*
 * Ethernet (1000 Mbit)
 */

/*
 * Ethernet (10000 Mbit)
 */

/*
 * Token Ring devices
 */

/*
 * Wireless LAN (non-hamradio)
 */
#undef CONFIG_NET_RADIO

/*
 * Wan interfaces
 */
#undef CONFIG_WAN
#undef CONFIG_PPP
#undef CONFIG_SLIP
#undef CONFIG_SHAPER
#undef CONFIG_NETCONSOLE

/*
 * ISDN subsystem
 */
#undef CONFIG_ISDN

/*
 * Input device support
 */
#define CONFIG_INPUT 1

/*
 * Userland interfaces
 */
#define CONFIG_INPUT_MOUSEDEV 1
#define CONFIG_INPUT_MOUSEDEV_PSAUX 1
#define CONFIG_INPUT_MOUSEDEV_SCREEN_X 1024
#define CONFIG_INPUT_MOUSEDEV_SCREEN_Y 768
#undef CONFIG_INPUT_JOYDEV
#undef CONFIG_INPUT_TSDEV
#undef CONFIG_INPUT_EVDEV
#undef CONFIG_INPUT_EVBUG

/*
 * Input Device Drivers
 */
#undef CONFIG_INPUT_KEYBOARD
#undef CONFIG_INPUT_MOUSE
#undef CONFIG_INPUT_JOYSTICK
#undef CONFIG_INPUT_TOUCHSCREEN
#undef CONFIG_INPUT_MISC

/*
 * Hardware I/O ports
 */
#undef CONFIG_SERIO
#undef CONFIG_GAMEPORT

/*
 * Character devices
 */
#define CONFIG_VT 1
#define CONFIG_VT_CONSOLE 1
#define CONFIG_HW_CONSOLE 1
#undef CONFIG_SERIAL_NONSTANDARD

/*
 * Serial drivers
 */
#undef CONFIG_SERIAL_8250

/*
 * Non-8250 serial port support
 */
#define CONFIG_SERIAL_AT91 1
#define CONFIG_SERIAL_AT91_CONSOLE 1
#define CONFIG_SERIAL_CORE 1
#define CONFIG_SERIAL_CORE_CONSOLE 1
#define CONFIG_UNIX98_PTYS 1
#define CONFIG_LEGACY_PTYS 1
#define CONFIG_LEGACY_PTY_COUNT 256

/*
 * IPMI
 */
#undef CONFIG_IPMI_HANDLER

/*
 * Watchdog Cards
 */
#define CONFIG_WATCHDOG 1
#define CONFIG_WATCHDOG_NOWAYOUT 1

/*
 * Watchdog Device Drivers
 */
#undef CONFIG_SOFT_WATCHDOG
#define CONFIG_AT91_WATCHDOG 1

/*
 * USB-based Watchdog Cards
 */
#undef CONFIG_USBPCWATCHDOG
#undef CONFIG_NVRAM
#undef CONFIG_RTC
#define CONFIG_AT91_RTC 1
#undef CONFIG_DTLK
#undef CONFIG_R3964

/*
 * Ftape, the floppy tape device driver
 */
#undef CONFIG_DRM
#undef CONFIG_RAW_DRIVER

/*
 * TPM devices
 */

/*
 * NKTY common device driver
 */
#define CONFIG_NKTYCOMMON 1
#define CONFIG_AT91_HASHTAB 1
#undef CONFIG_AT91_USRI2C_COMMON
#undef CONFIG_AT91_SED1335_COMMON
#undef CONFIG_AT91_GPIOB_COMMON
#undef CONFIG_AT91_DATAFLASH_COMMON
#undef CONFIG_AT91_FLASH_COMMON

/*
 * NKTY devices 21c
 */
#define CONFIG_NKTYDEV21C 1
#define CONFIG_AT91_UART 1
#undef CONFIG_NKTYCPUCARD
#define CONFIG_RECORD_CASHTERM 1
#define CONFIG_UART_V2 1
#undef CONFIG_GGGS
#undef CONFIG_AT91_USRI2C
#define CONFIG_AT91_USRI2C_GPIO 1
#define CONFIG_AT91_SED1335 1
#define CONFIG_AT91_GPIOB 1
#define CONFIG_AT91_DATAFLASH 1
#define CONFIG_AT91_SPI 1
#undef CONFIG_AT91_SPIDEV

/*
 * I2C support
 */
#undef CONFIG_I2C

/*
 * Misc devices
 */

/*
 * Multimedia devices
 */
#undef CONFIG_VIDEO_DEV

/*
 * Digital Video Broadcasting Devices
 */
#undef CONFIG_DVB

/*
 * Graphics support
 */
#undef CONFIG_FB

/*
 * Console display driver support
 */
#undef CONFIG_VGA_CONSOLE
#define CONFIG_DUMMY_CONSOLE 1

/*
 * Sound
 */
#undef CONFIG_SOUND

/*
 * USB support
 */
#define CONFIG_USB_ARCH_HAS_HCD 1
#define CONFIG_USB_ARCH_HAS_OHCI 1
#define CONFIG_USB 1
#undef CONFIG_USB_DEBUG

/*
 * Miscellaneous USB options
 */
#define CONFIG_USB_DEVICEFS 1
#undef CONFIG_USB_BANDWIDTH
#undef CONFIG_USB_DYNAMIC_MINORS
#undef CONFIG_USB_OTG

/*
 * USB Host Controller Drivers
 */
#define CONFIG_USB_OHCI_HCD 1
#undef CONFIG_USB_OHCI_BIG_ENDIAN
#define CONFIG_USB_OHCI_LITTLE_ENDIAN 1
#undef CONFIG_USB_SL811_HCD

/*
 * USB Device Class drivers
 */
#undef CONFIG_USB_BLUETOOTH_TTY
#undef CONFIG_USB_ACM
#undef CONFIG_USB_PRINTER

/*
 * NOTE: USB_STORAGE enables SCSI, and 'SCSI disk support' may also be needed; see USB_STORAGE Help for more information
 */
#define CONFIG_USB_STORAGE 1
#undef CONFIG_USB_STORAGE_DEBUG
#define CONFIG_USB_STORAGE_DATAFAB 1
#undef CONFIG_USB_STORAGE_FREECOM
#define CONFIG_USB_STORAGE_DPCM 1
#define CONFIG_USB_STORAGE_USBAT 1
#define CONFIG_USB_STORAGE_SDDR09 1
#define CONFIG_USB_STORAGE_SDDR55 1
#undef CONFIG_USB_STORAGE_JUMPSHOT

/*
 * USB Input Devices
 */
#undef CONFIG_USB_HID

/*
 * USB HID Boot Protocol drivers
 */
#undef CONFIG_USB_KBD
#undef CONFIG_USB_MOUSE
#undef CONFIG_USB_AIPTEK
#undef CONFIG_USB_WACOM
#undef CONFIG_USB_KBTAB
#undef CONFIG_USB_POWERMATE
#undef CONFIG_USB_MTOUCH
#undef CONFIG_USB_EGALAX
#undef CONFIG_USB_XPAD
#undef CONFIG_USB_ATI_REMOTE

/*
 * USB Imaging devices
 */
#undef CONFIG_USB_MDC800
#undef CONFIG_USB_MICROTEK

/*
 * USB Multimedia devices
 */
#undef CONFIG_USB_DABUSB

/*
 * Video4Linux support is needed for USB Multimedia device support
 */

/*
 * USB Network Adapters
 */
#undef CONFIG_USB_CATC
#undef CONFIG_USB_KAWETH
#undef CONFIG_USB_PEGASUS
#undef CONFIG_USB_RTL8150
#undef CONFIG_USB_USBNET
#undef CONFIG_USB_MON

/*
 * USB port drivers
 */

/*
 * USB Serial Converter support
 */
#undef CONFIG_USB_SERIAL

/*
 * USB Miscellaneous drivers
 */
#undef CONFIG_USB_EMI62
#undef CONFIG_USB_EMI26
#undef CONFIG_USB_AUERSWALD
#undef CONFIG_USB_RIO500
#undef CONFIG_USB_LEGOTOWER
#undef CONFIG_USB_LCD
#undef CONFIG_USB_LED
#undef CONFIG_USB_CYTHERM
#undef CONFIG_USB_PHIDGETKIT
#undef CONFIG_USB_PHIDGETSERVO
#undef CONFIG_USB_IDMOUSE
#undef CONFIG_USB_TEST

/*
 * USB ATM/DSL drivers
 */

/*
 * USB Gadget Support
 */
#undef CONFIG_USB_GADGET

/*
 * MMC/SD Card support
 */
#undef CONFIG_MMC

/*
 * File systems
 */
#define CONFIG_EXT2_FS 1
#undef CONFIG_EXT2_FS_XATTR
#undef CONFIG_EXT3_FS
#undef CONFIG_JBD
#undef CONFIG_REISERFS_FS
#undef CONFIG_JFS_FS

/*
 * XFS support
 */
#undef CONFIG_XFS_FS
#undef CONFIG_MINIX_FS
#undef CONFIG_ROMFS_FS
#undef CONFIG_QUOTA
#undef CONFIG_DNOTIFY
#undef CONFIG_AUTOFS_FS
#undef CONFIG_AUTOFS4_FS

/*
 * CD-ROM/DVD Filesystems
 */
#undef CONFIG_ISO9660_FS
#undef CONFIG_UDF_FS

/*
 * DOS/FAT/NT Filesystems
 */
#define CONFIG_FAT_FS 1
#undef CONFIG_MSDOS_FS
#define CONFIG_VFAT_FS 1
#define CONFIG_FAT_DEFAULT_CODEPAGE 437
#define CONFIG_FAT_DEFAULT_IOCHARSET "cp437"
#undef CONFIG_NTFS_FS

/*
 * Pseudo filesystems
 */
#define CONFIG_PROC_FS 1
#define CONFIG_SYSFS 1
#define CONFIG_DEVFS_FS 1
#define CONFIG_DEVFS_MOUNT 1
#undef CONFIG_DEVFS_DEBUG
#undef CONFIG_DEVPTS_FS_XATTR
#define CONFIG_TMPFS 1
#undef CONFIG_TMPFS_XATTR
#undef CONFIG_HUGETLB_PAGE
#define CONFIG_RAMFS 1

/*
 * Miscellaneous filesystems
 */
#undef CONFIG_ADFS_FS
#undef CONFIG_AFFS_FS
#undef CONFIG_HFS_FS
#undef CONFIG_HFSPLUS_FS
#undef CONFIG_BEFS_FS
#undef CONFIG_BFS_FS
#undef CONFIG_EFS_FS
#undef CONFIG_CRAMFS
#undef CONFIG_VXFS_FS
#undef CONFIG_HPFS_FS
#undef CONFIG_QNX4FS_FS
#undef CONFIG_SYSV_FS
#undef CONFIG_UFS_FS

/*
 * Network File Systems
 */
#define CONFIG_NFS_FS 1
#define CONFIG_NFS_V3 1
#undef CONFIG_NFS_V4
#undef CONFIG_NFS_DIRECTIO
#undef CONFIG_NFSD
#define CONFIG_ROOT_NFS 1
#define CONFIG_LOCKD 1
#define CONFIG_LOCKD_V4 1
#define CONFIG_SUNRPC 1
#undef CONFIG_RPCSEC_GSS_KRB5
#undef CONFIG_RPCSEC_GSS_SPKM3
#undef CONFIG_SMB_FS
#undef CONFIG_CIFS
#undef CONFIG_NCP_FS
#undef CONFIG_CODA_FS
#undef CONFIG_AFS_FS

/*
 * Partition Types
 */
#undef CONFIG_PARTITION_ADVANCED
#define CONFIG_MSDOS_PARTITION 1

/*
 * Native Language Support
 */
#define CONFIG_NLS 1
#define CONFIG_NLS_DEFAULT "iso8859-1"
#define CONFIG_NLS_CODEPAGE_437 1
#undef CONFIG_NLS_CODEPAGE_737
#undef CONFIG_NLS_CODEPAGE_775
#undef CONFIG_NLS_CODEPAGE_850
#undef CONFIG_NLS_CODEPAGE_852
#undef CONFIG_NLS_CODEPAGE_855
#undef CONFIG_NLS_CODEPAGE_857
#undef CONFIG_NLS_CODEPAGE_860
#undef CONFIG_NLS_CODEPAGE_861
#undef CONFIG_NLS_CODEPAGE_862
#undef CONFIG_NLS_CODEPAGE_863
#undef CONFIG_NLS_CODEPAGE_864
#undef CONFIG_NLS_CODEPAGE_865
#undef CONFIG_NLS_CODEPAGE_866
#undef CONFIG_NLS_CODEPAGE_869
#undef CONFIG_NLS_CODEPAGE_936
#undef CONFIG_NLS_CODEPAGE_950
#undef CONFIG_NLS_CODEPAGE_932
#undef CONFIG_NLS_CODEPAGE_949
#undef CONFIG_NLS_CODEPAGE_874
#undef CONFIG_NLS_ISO8859_8
#undef CONFIG_NLS_CODEPAGE_1250
#undef CONFIG_NLS_CODEPAGE_1251
#undef CONFIG_NLS_ASCII
#undef CONFIG_NLS_ISO8859_1
#undef CONFIG_NLS_ISO8859_2
#undef CONFIG_NLS_ISO8859_3
#undef CONFIG_NLS_ISO8859_4
#undef CONFIG_NLS_ISO8859_5
#undef CONFIG_NLS_ISO8859_6
#undef CONFIG_NLS_ISO8859_7
#undef CONFIG_NLS_ISO8859_9
#undef CONFIG_NLS_ISO8859_13
#undef CONFIG_NLS_ISO8859_14
#undef CONFIG_NLS_ISO8859_15
#undef CONFIG_NLS_KOI8_R
#undef CONFIG_NLS_KOI8_U
#undef CONFIG_NLS_UTF8

/*
 * Profiling support
 */
#undef CONFIG_PROFILING

/*
 * Kernel hacking
 */
#undef CONFIG_PRINTK_TIME
#undef CONFIG_DEBUG_KERNEL
#define CONFIG_LOG_BUF_SHIFT 14
#undef CONFIG_DEBUG_BUGVERBOSE
#define CONFIG_FRAME_POINTER 1
#define CONFIG_DEBUG_USER 1

/*
 * Security options
 */
#undef CONFIG_KEYS
#undef CONFIG_SECURITY

/*
 * Cryptographic options
 */
#undef CONFIG_CRYPTO

/*
 * Hardware crypto devices
 */

/*
 * Library routines
 */
#define CONFIG_CRC_CCITT 1
#define CONFIG_CRC32 1
#undef CONFIG_LIBCRC32C
