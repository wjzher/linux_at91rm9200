/*
 * Atmel DataFlash driver for Atmel AT91RM9200 (Thunder)
 *
 *  Copyright (C) SAN People (Pty) Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
*/

#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/at91_spi.h>

#include <asm/arch/AT91RM9200_SPI.h>
#include <asm/arch/pio.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/arch/board.h>
#include <asm/arch/gpio.h>
#include <linux/at91_spi.h>
#include <linux/blkdev.h>

#if 1
#define pr_debug(fmt,arg...) \
	printk(KERN_DEBUG fmt,##arg)
#else
#define pr_debug(fmt,arg...) \
	do { } while (0)
#endif

/*
 * 电路目前接法:
 * 1 CS: Host to card Chip Select signal.	SPI_CS3
 * 5 CLK : Host to card clock signal		SPI_CLK
 * 2 Data In: Host to card data signal.		SPI_DIN
 * 7 Data Out: Card to host data signal.	SPI_DOUT
 * 8,9 reserved, 100k pull-up 3v3
 * det_pin -> PB6
 */

#define SDSETCTSIZE 512
#define KERNEL_SECTOR_SIZE 512
#define SDCARD_MINORS 1
#define SDDETPIN	AT91_PIN_PB6
#define SDCARDCS	3

#define DUMMY_CHAR 0xFF
#define INITIAL_CRC 0x95
// commands: first bit 0 (start bit), second 1 (transmission bit); CMD-number + 0ffsett 0x40
#define MMC_GO_IDLE_STATE          0x40     //CMD0
#define MMC_SEND_OP_COND           0x41     //CMD1
#define MMC_READ_CSD               0x49     //CMD9
#define MMC_SEND_CID               0x4a     //CMD10
#define MMC_STOP_TRANSMISSION      0x4c     //CMD12
#define MMC_SEND_STATUS            0x4d     //CMD13
#define MMC_SET_BLOCKLEN           0x50     //CMD16 Set block length for next read/write
#define MMC_READ_SINGLE_BLOCK      0x51     //CMD17 Read block from memory
#define MMC_READ_MULTIPLE_BLOCK    0x52     //CMD18
#define MMC_CMD_WRITEBLOCK         0x54     //CMD20 Write block to memory
#define MMC_WRITE_BLOCK            0x58     //CMD24
#define MMC_WRITE_MULTIPLE_BLOCK   0x59     //CMD25
#define MMC_WRITE_CSD              0x5b     //CMD27 PROGRAM_CSD
#define MMC_SET_WRITE_PROT         0x5c     //CMD28
#define MMC_CLR_WRITE_PROT         0x5d     //CMD29
#define MMC_SEND_WRITE_PROT        0x5e     //CMD30
#define MMC_TAG_SECTOR_START       0x60     //CMD32
#define MMC_TAG_SECTOR_END         0x61     //CMD33
#define MMC_UNTAG_SECTOR           0x62     //CMD34
#define MMC_TAG_EREASE_GROUP_START 0x63     //CMD35
#define MMC_TAG_EREASE_GROUP_END   0x64     //CMD36
#define MMC_UNTAG_EREASE_GROUP     0x65     //CMD37
#define MMC_EREASE                 0x66     //CMD38
#define MMC_READ_OCR               0x67     //CMD39
#define MMC_CRC_ON_OFF             0x68     //CMD40

// status
#define IN_IDLE_STATE 				0x01
#define SUCCESS 					0x00
#define DATA_ACCEPTED 				0x05
#define CRC_ERROR 					0x0B
#define WRITE_ERROR 				0x0D
#define ERROR						0x01
#define CC_ERROR					0x02
#define CARD_ECC_FAILED				0x04
#define OUT_OF_RANGE				0x08  
#define ILLEGAL_COMMAND_IDLE_STATE  0x05 

// 定义以下变量
static int sdcard_major = 0;		// 主设备号
static const char *dev_name = "at91_sdcard";
/* SPI controller device */
static AT91PS_SPI spi_ctrl = (AT91PS_SPI) AT91C_VA_BASE_SPI;
#define FASH_SPI_BAUD	(at91_master_clock / (2 * 6000000))
// slow spi baud range 100k to 400k
#define SLOW_SPI_BAUD	(at91_master_clock / (2 * 100000))

/*
 * The internal representation of our device.
 * 驱动低级的类型
 */
struct sdcard_dev {
	int size;                       /* Device size in sectors */
	int hardsect_size;				/* SD Card sect size */
	/* u8 *data;                       The data array */
	short users;                    /* How many users */
	short media_change;             /* Flag a media change? */
	spinlock_t lock;                /* For mutual exclusion */
	struct request_queue *queue;    /* The device request queue */
	struct gendisk *gd;             /* The gendisk structure */
	struct timer_list timer;        /* For simulated media changes */
	int irq;						/* irq number */
	struct spi_transfer_list sd_spi_trans;
	int det_pin;					/* detect card pin */
	int nsectors;					/* how big the card is */
};

static struct sdcard_dev *sd_dev = NULL;

/* Detected DataFlash devices */

/* Allocate a single SPI transfer descriptor.  We're assuming that if multiple
   SPI transfers occur at the same time, spi_access_bus() will serialize them.
   If this is not valid, then either (i) each dataflash 'priv' structure
   needs it's own transfer descriptor, (ii) we lock this one, or (iii) use
   another mechanism.   */
//static struct spi_transfer_list* sd_spi_trans_desc;

static inline void AT91_CfgPIO_IO_CS3(void) {
	AT91_SYS->PIOA_PER = AT91C_PA6_NPCS3;	// 使能I/O
	AT91_SYS->PIOA_OER = AT91C_PA6_NPCS3;	// 配置为输出引脚
}

static inline void set_spi_baud(AT91PS_SPI ctrl, int num, int baud)
{
	AT91_REG *reg = &ctrl->SPI_CSR0 + num;
	*reg &= 0xFFFF00FF;
	*reg |= (baud << 8);
}

/*
 * Perform a SPI transfer, but not used interrupt
 */
static int do_spi_transfer_w(int nr, char* tx, int tx_len, char* rx, int rx_len,
		char* txnext, int txnext_len, char* rxnext, int rxnext_len)
{
	struct spi_transfer_list* list = &sd_dev->sd_spi_trans;

	list->tx[0] = tx;	list->txlen[0] = tx_len;
	list->rx[0] = rx;	list->rxlen[0] = rx_len;

	list->tx[1] = txnext; 	list->txlen[1] = txnext_len;
	list->rx[1] = rxnext;	list->rxlen[1] = rxnext_len;

	list->nr_transfers = nr;

	return spi_transfer_w(list);
}

/*
 * Perform a SPI transfer to access the DataFlash device.
 */
static int do_spi_transfer(int nr, char* tx, int tx_len, char* rx, int rx_len,
		char* txnext, int txnext_len, char* rxnext, int rxnext_len)
{
	struct spi_transfer_list* list = &sd_dev->sd_spi_trans;

	list->tx[0] = tx;	list->txlen[0] = tx_len;
	list->rx[0] = rx;	list->rxlen[0] = rx_len;

	list->tx[1] = txnext; 	list->txlen[1] = txnext_len;
	list->rx[1] = rxnext;	list->rxlen[1] = rxnext_len;

	list->nr_transfers = nr;

	return spi_transfer(list);
}

/* ......................................................................... */
static void at91_sdcadr_request(request_queue_t *q)
{
	return;
}

static u8 sd_crc7(u8 full_command[])
{
	int i, j;
	u8 command, crc = 0;
	for (i=0;i<5;i++) {
		command = full_command[i];		
		for (j=0;j<8;j++) {
			crc = crc << 1;					//Shift crc left by 1			
         	if ((command ^ crc) & 0x80)	//Test command XOR with crc and masked with 0x8000
         		crc = crc ^ 0x09;  		//XOR crc with 0x0900           
         	command = command << 1;			//Shift command left by 1
      	}

       	crc = crc & 0x7F;					//Mask CRC with 0x7F00	
   	}
   	crc = crc << 1;							//Shift crc left by 1
   	crc |= 0x01;							//CRC ORed with 0x0100
  	return(crc);							//Return CRC
}

/*
 * command consists of six bytes:
 * the first byte is the command,
 * the next four bytes are for status, address, or data
 * and the last byte is for CRC check.
 */
static int sdcard_xmit_command(u8 cmd, u8 *data, u16 crc)
{
	u8 command[6] = {0}, rx_cmd[6];
	command[0] = cmd;
	if (data) {
		memcpy(command, data, 4);
	}
	command[5] = crc;
	return do_spi_transfer(1, command, sizeof(command),
		rx_cmd, sizeof(rx_cmd), NULL, 0, NULL, 0);
}

static u8 sdcard_get_response(void)
{
	//This loop continously transmits 0xFF in order to receive response from card. 
	//The card is expected to return 00 or 01 stating that it is in idle state or 
	//it has finished it's initialization(SUCCESS). If anything is received besides
	//0x00, 0x01, or 0xFF then an error has occured. 
	//Response comes 1-8bytes after command
	u8 tx[1] = {0}, rx[1] = {0};
	int i;
	for (i = 0; i < 8; i++) {
		memset(tx, DUMMY_CHAR, sizeof(tx));
		memset(rx, 0, sizeof(rx));
		do_spi_transfer(1, tx, sizeof(tx), rx, sizeof(rx),
			NULL, 0, NULL, 0);
		if (rx[0] == SUCCESS || rx[0] == IN_IDLE_STATE) {
			break;
		} else if (rx[0] == DUMMY_CHAR) {
			continue;
		} else {
			printk("sdcard_get_response: recv %02x\n", rx[0]);
			rx[0] = DUMMY_CHAR;
			break;
		}
	}
	return rx[0];
}

static inline int send_spi_clk(int n)
{
	char *data;
	data = (char *)kmalloc(n, GFP_KERNEL);
	if (data == NULL)
		return -ENOMEM;
	memset(data, DUMMY_CHAR, n);
	do_spi_transfer(1, data, n, data, n, NULL, 0, NULL, 0);
	kfree(data);
	return 0;
}

/*
 * send 74 clk, cs is high
 */
static int sdcard_power_det(AT91PS_SPI ctrl)
{
#if 0
	char pre_tx[10], pre_rx[10] = {0};
	memset(pre_tx, DUMMY_CHAR, sizeof(pre_tx));
#endif
	// set spi baud
	set_spi_baud(ctrl, SDCARDCS, SLOW_SPI_BAUD);
	// access spi bus
	spi_access_bus(SDCARDCS);
	// npcs3为I/O模式
	AT91_CfgPIO_IO_CS3();
	// set npcs high
	at91_set_gpio_value(AT91C_PA6_NPCS3, 1);
	// 进行数据传输
	send_spi_clk(10);
#if 0
	do_spi_transfer_w(1, pre_tx, sizeof(pre_tx), pre_rx, sizeof(pre_rx),
		NULL, 0, NULL, 0);
#endif
	// release spi bus
	spi_release_bus(SDCARDCS);
	return 0;
}

/*
 * initialize sdcard and get some info
 */
static int sdcard_initialization(AT91PS_SPI ctrl)
{
	int i = 0, ret = 0;
	// set spi baud
	set_spi_baud(ctrl, SDCARDCS, SLOW_SPI_BAUD);
	// access spi bus
	spi_access_bus(SDCARDCS);
	// npcs3为I/O模式
	AT91_CfgPIO_IO_CS3();
	// set npcs low
	at91_set_gpio_value(AT91C_PA6_NPCS3, 0);
	// send command GO_IDLE_STATE
	sdcard_xmit_command(MMC_GO_IDLE_STATE, NULL, INITIAL_CRC);
	// get response
	while ((sdcard_get_response() != IN_IDLE_STATE) && (i < 10))
		i++;
	if (i == 10) {
		printk("sdcard initialization get response over 10\n");
		ret = -1;
		goto end;
	}
	//After receiving response clock must be active for 8 clock cycles
	send_spi_clk(1);
end:
	return ret;
}

static int at91_sdcard_probe(struct sdcard_dev *dev)
{
	int ret;
	if (dev->det_pin) {
		ret = at91_get_gpio_value(dev->det_pin);
		if (ret) {
			// 现在没有sdcard, 高电平时没有SD
			printk(KERN_WARNING "at91_sdcard: no SD Card detected\n");
			return 0;
		}
	}
	// 没有指定相应的察觉表或者SDCARD已经存在
	sdcard_power_det(spi_ctrl);
	set_capacity(dev->gd, dev->nsectors * (dev->hardsect_size / KERNEL_SECTOR_SIZE));
	add_disk(dev->gd);
	return 0;
}
static int at91_sdcard_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int at91_sdcard_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static int at91_sdcard_media_changed(struct gendisk *gd)
{
	return 0;
}

static int at91_sdcard_revalidate(struct gendisk *gd)
{
	return 0;
}
static int at91_sdcard_ioctl (struct inode *inode, struct file *filp,
                 unsigned int cmd, unsigned long arg)
{
	return 0;
}
/*
 * The device operations structure.
 */
static struct block_device_operations at91_sdcard_ops = {
	.owner           = THIS_MODULE,
	.open 	         = at91_sdcard_open,
	.release		 = at91_sdcard_release,
	.media_changed   = at91_sdcard_media_changed,
	.revalidate_disk = at91_sdcard_revalidate,
	.ioctl	         = at91_sdcard_ioctl
};

/*
 * Detect and initialize DataFlash device connected to specified SPI channel.
 */
static int at91_sdcard_setup(struct sdcard_dev *dev)
{
	unsigned short status;
	// SDCARD接在SPI_3上
	int channel = 3;
	// 并不需要告诉内核SDCARD是否已经接入
	// 但要进行设备的初始化
	memset(dev, 0, sizeof(struct sdcard_dev));
	dev->hardsect_size = SDSETCTSIZE;
	dev->det_pin = SDDETPIN;
	spin_lock_init(&dev->lock);
	/*
	 * The I/O queue, depending on whether we are using our own
	 * make_request function or not.
	 */
	dev->queue = blk_init_queue(at91_sdcadr_request, &dev->lock);
	if (dev->queue == NULL) {
		printk (KERN_WARNING "at91_sdcard_setup: blk init queue failure\n");
		return -ENOMEM;
	}
	// set hardware sector size for the queue
	blk_queue_hardsect_size(dev->queue, dev->hardsect_size);
	// save queuedata, ll_rw_blk doesn't touch it
	dev->queue->queuedata = dev;
	/*
	 * And the gendisk structure.
	 */
	dev->gd = alloc_disk(SDCARD_MINORS);
	if (! dev->gd) {
		printk (KERN_WARNING "at91_sdcard_setup: alloc_disk failure\n");
		blk_cleanup_queue(dev->queue);
		return -ENOMEM;
	}
	dev->gd->major = sdcard_major;
	dev->gd->first_minor = 0;
	dev->gd->fops = &at91_sdcard_ops;
	dev->gd->queue = dev->queue;
	dev->gd->private_data = dev;
	snprintf (dev->gd->disk_name, 32, "sdcard%c", 0 + 'a');
	// 现在还不知道SDCARD的存储容量, 所以需要初始化一下
	at91_sdcard_probe(dev);
	return 0;
}

static int __init at91_sdcard_init(void)
{
	// 获取主设备号
	sdcard_major = register_blkdev(sdcard_major, dev_name);
	if (sdcard_major <= 0) {
		printk(KERN_WARNING "at91_sdcard: unable to get major number\n");
		return -EBUSY;
	}

	/*
	 * Allocate the device struct, and initialize it.
	 * 由于资源有限, 所以只支持一个SD卡
	 */
	sd_dev = kmalloc(sizeof (struct sdcard_dev), GFP_KERNEL);
	if (sd_dev == NULL) {
		unregister_blkdev(sdcard_major, dev_name);
		return -ENOMEM;
	}

	pr_debug("at91_sdcard: begin setup\n");
	/* setup sdcard device */
	at91_sdcard_setup(sd_dev);

	return 0;
}

static void __exit at91_sdcard_exit(void)
{
/*	int i;

	for (i = 0; i < DATAFLASH_MAX_DEVICES; i++) {
		if (mtd_devices[i]) {
			del_mtd_device(mtd_devices[i]);
			kfree(mtd_devices[i]->priv);
			kfree(mtd_devices[i]);
		}
	}
	nr_devices = 0;
	kfree(spi_transfer_desc);
	*/
}


module_init(at91_sdcard_init);
module_exit(at91_sdcard_exit);

MODULE_LICENSE("GPL")
MODULE_AUTHOR("Andrew Victor")
MODULE_DESCRIPTION("DataFlash driver for Atmel AT91RM9200")
