/*
 * Copyright (c) 2007, NKTY Company
 * All rights reserved.
 * �ļ����ƣ�at91_flash.c
 * ժ    Ҫ��NOR FLASH��������, ��Ҫ֧��JS28F640
 * 
 * ��ǰ�汾��1.0
 * ��    �ߣ�wjzhe
 * ������ڣ�2007��6��8��
 *
 * ȡ���汾��1.0 
 * ԭ����  ��wjzhe
 * ������ڣ�2007��6��8��
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/in6.h>
#include <linux/syscalls.h>
#include <linux/vmalloc.h>
#include <asm/checksum.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm/types.h>

#include <asm/uaccess.h>
#include <asm/arch/pio.h>
 
#include "flash.h"

/* ������FLASH�����ַ�Ķ���, ����FLASH�ڲ���ַ�Ķ��� */
#define PHYS_FLASH_1 0x10000000
#define PHYS_FLASH_SIZE 0x200000  /* 2 megs main flash */
#define CFG_FLASH_BASE		PHYS_FLASH_1
/* �����ַ */
#define MEM_FLASH_ADDR1		((0x00005555<<1))
#define MEM_FLASH_ADDR2		((0x00002AAA<<1))
#define IDENT_FLASH_ADDR1	((0x0000555<<1))
#define IDENT_FLASH_ADDR2	((0x0000AAA<<1))

flash_info_t    *flash_info;	// ����FLASH INFO STUCTURE
#define DEBUG
#undef DEBUG

/*
 * ʶ��FLASH����, ���������ڴ�ӳ��
 * Ŀǰֻʶ��INTEL����
 */
static int at91_flash_mem(flash_info_t *info)
{
	unsigned short manuf_code, device_code, add_device_code, data;
	void *ptr;
	void *start;
	unsigned char cfi[4];
	unsigned long s_addr = 0;
	int i;
	start = ioremap(CFG_FLASH_BASE, SZ_2M);// ������2M��FLASH
	// read and store data from CFG_FLASH_BASE
	data = 0;
	// issue CFI Query, write 0x98 to 0x55
	ptr = start + (FLASH_CODE2 << 1);
	__raw_writew(QRY_IN_CODE, ptr);
	// data = "QRY"?
	ptr = start + QRY_OUT_CODE;
	for (i = 0; i < 3; i += 1) {
		data = __raw_readw(ptr);
		memcpy(&cfi[i], &data, sizeof(cfi[i]));
		ptr += 2;
	}
	if (cfi[0] == 'Q' && cfi[1] == 'R' && cfi[2] == 'Y') {
#ifdef DEBUG
		printk("FLASH is CFI\n");
#endif
	} else {
#ifdef DEBUG
		printk("FLASH is not CFI, %02x, %02x, %02x\n", cfi[0], cfi[1], cfi[2]);
#endif
		iounmap(start);
		return -1;
	}
	// �ж�FLASH�ͺ�
	ptr = start + IDENT_FLASH_ADDR1;
	__raw_writew(FLASH_CODE1, ptr);
	ptr = start + IDENT_FLASH_ADDR2;
	__raw_writew(FLASH_CODE2, ptr);
	ptr = start + IDENT_FLASH_ADDR1;
	__raw_writew(ID_IN_CODE, ptr);
	ptr = start;
	manuf_code = __raw_readw(ptr);
	ptr += 2;
	device_code = __raw_readw(ptr);
	ptr = start + (3 << 1);
	add_device_code = __raw_readw(ptr);
#ifdef DEBUG
#if 0
	ptr = start + IDENT_FLASH_ADDR1;
	*ptr = FLASH_CODE1;
	ptr = start + IDENT_FLASH_ADDR2;
	*ptr = FLASH_CODE2;
	ptr = start + IDENT_FLASH_ADDR1;
	*ptr = ID_OUT_CODE;
#endif/* 0 */
	printk("manuf_code %04x \n", manuf_code);
	printk("device_code %04x \n", device_code);
	printk("add_device_code %04x \n", add_device_code);
#endif
	if ((manuf_code & FLASH_TYPEMASK) == (INTEL_MANUFACT & FLASH_TYPEMASK)) {
		info->flash_id = INTEL_MANUFACT & FLASH_TYPEMASK;
		switch (device_code & FLASH_TYPEMASK) {
			case INTEL_ID_28F320J3A & FLASH_TYPEMASK:
				printk("32M bit FLASH 28F320\n");
				info->size = SZ_4M;
				break;
			case INTEL_ID_28F128J3A & FLASH_TYPEMASK:
				printk("128M bit FLASH 28F128\n");
				info->size = SZ_16M;
				break;
			case INTEL_ID_28F640J3A & FLASH_TYPEMASK:
				printk("32M bit FLASH 28F640\n");
				info->size = SZ_8M;
				break;
			default:
				printk("Unknown FLASH\n");
				return -1;
		}
	}
	// ����Ҫ����ӳ���ڴ�
	iounmap(start);
	start = ioremap(CFG_FLASH_BASE, info->size);
	info->sector_count = info->size / SZ_128K;
	for (i = 0; i < (info->sector_count); i++) {
		info->start[i] = CFG_FLASH_BASE + s_addr;
		info->sect_addr[i] = start + s_addr;
		s_addr += SZ_128K;
	}
	return 0;
}
/*
 * ��FLASH״̬, ����FLASH״̬��Ϣ
 */
static unsigned short read_flash_stauts(flash_info_t *info)
{
	void *ptr;
	unsigned short status;
	// issue status register command, any address 0x70
	ptr = info->sect_addr[0];
	__raw_writew(SRCMD, ptr);
	status = 0;
	status = __raw_readw(ptr);
	return (status & 0xFF);
}
/*
 * ��дFLASH�жϳ�ʱ����, bit��Ҫ�жϵ�λ, loop��ѭ������
 * ����0˵���ж�bitλ��ʱ, ��������ֵ, �򲻳�ʱ
 */
static short flash_poll(flash_info_t *info, unsigned long bit, int loop)
{
	int loop_cntr = loop;
	unsigned short status;
	do {
		udelay(10);
		status = read_flash_stauts(info);
#if 0
		printk("poll status: %04x, bit: %08x, loopcnt: %d\n",
			status, bit, loop_cntr);
#endif
		status &= bit;
	} while ((status == 0) && (--loop_cntr > 0));
	return (loop_cntr > 0);
}

/*
 * ��ȡFLASH״̬, ���жϸ���״̬λ, ���д�ӡ���
 * ����ʱ������, �������󽫷���-1, ���򷵻�0
 */
int flash_sterr(flash_info_t *info)
{
	unsigned short status = read_flash_stauts(info);
	status &= 0xFF;
	if (!(status & WSMSTATUS)) {
		printk("Flash is busy now\n");
		//goto FERR;
	} else if (status & ERASESUSPEND) {
		printk("Erase Flash suspend\n");
		goto FERR;
	} else if (status & PROGSUSPEND) {
		printk("Program Flash suspend\n");
		goto FERR;
	} else if ((status & ECLSTATUS)) {
		if ((status & COMSEQERR))
			printk("Error Command Sequence\n");
		else
			printk("Erase Flash failed\n");
		goto FERR;
	} else if (status & COMSEQERR) {
		printk("Program Flash failed\n");
		goto FERR;
	} else if (status & PROGVOLTAGE) {
		printk("Error: Vpen below Vpenlk\n");
		goto FERR;
	} else if (status & DEVPROTECT) {
		printk("Error: block locked\n");
		goto FERR;
	} else {
#ifdef DEBUG
		printk("Flash work normal\n");
#endif
		return 0;
	}
FERR:
	return -1;
}

/*
 * ������д��FLASH�����������
 * count must less than or equal to buffer size(info->buffer_size)
 */
static int write_flash_buffer(void *buf, size_t count, flash_info_t *info, unsigned long addr)
{
	void *ptr;
	int loop = info->buffer_write_tout;
	size_t wcnt;
	unsigned short cmd, status, *data = buf;
	if (count > info->buffer_size) {
		printk("write flash buffer count > buffer_size\n");
		return -1;
	}
	if (addr & 1) {
		printk("unaligned destination not supported\n");
		return -1;
	};
	if (count == 0) {
		return 0;
	}
	// ����wcnt
	if (count % 2) {
		wcnt = (count + 1) / 2;
	} else {
		wcnt = count / 2;
	}
	wcnt -= 1;
	ptr = info->sect_addr[0];
	// wait flash buffer available
	do {
		// check ready status
		status = read_flash_stauts(info);
		status &= 0x80;
		udelay(3);
	} while ((status == 0) && (--loop > 0));
	if (loop <= 0) {
		printk("write_flash_buffer(wait flash buffer available): flash is not ready\n");
		return -1;
	}
#ifdef DEBUG
	printk("write_flash_buffer: wcnt: %d\n", wcnt);
#endif
	// command cycle, issue write to buffer command
	cmd = WBCMD;
	__raw_writew(cmd, ptr);
	//cmd = 0;
	// write buffer count 0 ~ F, with block addr
	ptr = info->sect_addr[(addr >> 17)];
#ifdef DEBUG
	printk("write buffer count: sector addr %p : %d\n", ptr, (addr >> 17));
#endif
	__raw_writew(wcnt, ptr);
	// write data to buffer
	ptr = info->sect_addr[0] + addr;
	while (count > 1) {
		__raw_writew(*data, ptr);
		data++;
		count -= 2;
		ptr += 2;
	}
	if (count == 1) {
		// ��ʣһ���ֽ�
		cmd = (unsigned short)*(unsigned char *)data;
		cmd |= 0xFF00;
		__raw_writew(cmd, ptr);
	}
	// over, and confirm command should be issue
	__raw_writew(CONFIRMCMD, ptr);
	// read status register, any error?
	if (!flash_poll(info, WSMSTATUS, info->buffer_write_tout)) {
		printk("write flash buffer: timeout\n");
		return -1;
	}
	return (flash_sterr(info));
}

/*
 * ���ֽ���FLASH���
 */
static int write_flash_word(unsigned short word, unsigned long addr, flash_info_t *info)
{
	void *ptr = info->sect_addr[0];
	
	if (addr & 1) {
		printk ("unaligned destination not supported\n");
		return -1;
	}
	ptr += addr;
	// write 0x40, address
	__raw_writew(PROGWORD, ptr);
	// write data and address
	if (!flash_poll(info, WSMSTATUS, info->write_tout)) {
		printk("Write Flash Word timeout\n");
		return -1;
	}
	// read status register, any error?
	return (flash_sterr(info));
}

/*
 * ����ĳһ��lock-bit
 */
static int set_lock_bit(int sect, flash_info_t *info)
{
	void *ptr;
	if ((sect < 0) || (sect >= info->sector_count)) {
		return -EINVAL;
	}
	ptr = info->sect_addr[sect];
	if (ptr == NULL)
		return -EINVAL;
	// write 60H, block addr
	__raw_writew(CMD_LOCK, ptr);
	// write 01H, block addr
	__raw_writew(1, ptr);
	// read status register, wait WSM
	if (!flash_poll(info, WSMSTATUS, info->write_tout)) {
		printk("Set sector %d lock bit timeout\n", sect);
		return -1;
	}
	// read status register, and ckeck procedure
	return (flash_sterr(info));
}

/*
 * ���sect lock-bit
 */
static int clear_lock_bit(int sect, flash_info_t *info)
{
	void *ptr;
	if ((sect < 0) || (sect >= info->sector_count)) {
		return -EINVAL;
	}
	ptr = info->sect_addr[sect];
	if (ptr == NULL)
		return -EINVAL;
	// write 60H, block addr
	__raw_writew(CMD_LOCK, ptr);
	// write D0H, block addr
	__raw_writew(CONFIRMCMD, ptr);
	// read status register, wait WSM
	if (!flash_poll(info, WSMSTATUS, info->erase_blk_tout)) {
		printk("Clear sector %d lock bit timeout\n", sect);
		return -1;
	}
	// read status register, and check procedure
	return (flash_sterr(info));
}

/*
 * ����FLASH����
 */
static int erase_flash(flash_info_t * info, int s_first, int s_last)
{
	int sect, ret;
	void *ptr;
#if 0
	int prot;
#endif
	/* first look for protection bits */

	if ((s_first < 0) || (s_first > s_last)) {
		return -EINVAL;
	}

	if ((info->flash_id & FLASH_TYPEMASK) !=
		(INTEL_MANUFACT & FLASH_TYPEMASK)) {
		printk("Unknown device: %08x\n", (unsigned int)info->flash_id);
		return -1;
	}
#if 0
	prot = 0;
	for (sect = s_first; sect <= s_last; ++sect) {
		if (info->protect[sect]) {
			prot++;
		}
	}
	if (prot) {
		printk("sectors %d not erased\n", prot);
		return -1;
	}
#endif
	/* Start erase on unprotected sectors */
	for (sect = s_first; sect <= s_last; sect++) {
		printk ("Erasing sector %2d ... ", sect);
		ptr = info->sect_addr[sect];
		__raw_writew(ERASEFLASH, ptr);
		__raw_writew(CONFIRMCMD, ptr);
		if (!flash_poll(info, WSMSTATUS, info->erase_blk_tout)) {
			printk("Erase sector %2d timeout\n", sect);
			return -1;
		}
		ret = flash_sterr(info);
		if (ret < 0) {
			break;
		}
		printk("\r");
	}
	printk ("Erase Flash ok.       \n");
	return ret;
}

/*
 * read flash opration
 */
int read_flash_data(flash_info_t *info, void *data, size_t a_first, size_t cnt)
{
	int ret, i, step = info->chipwidth / 8;
	void *ptr = info->sect_addr[0];
	ushort temp;
	if (a_first & 1) {
		printk("Unknown addr %08x\n", a_first);
		ret = -EFAULT;
		goto dout;
	}
	if (cnt & 1) {
		printk("count must be double, cnt: %d\n", cnt);
		ret = -EINVAL;
		goto dout;
	}
	if (!flash_poll(info, WSMSTATUS, info->erase_blk_tout)) {
		printk("read flash data: flash is busy\n");
		ret = -1;
		goto dout;
	}
	// enter array mode, write 0xFF
	__raw_writew(CMD_READ_ARRAY, ptr);
	udelay(10);
	// read data
	ptr += a_first;
#ifdef DEBUG
	printk("start addr is %p: phy addr is %08x\n", ptr, 0x10000000 + a_first);
	printk("step is %d, cnt is %d\n", step, cnt);
#endif
	for (i = 0; i < (cnt / 2); i++) {
		temp = __raw_readw(ptr);
		memcpy(data, &temp, step);
		data += step;
		ptr += step;
	}
dout:
	return cnt;
}

static loff_t at91_flash_llseek(struct file *filp, loff_t off, int whence)
{
    flash_info_t *info = filp->private_data;
    loff_t newpos;

    switch(whence) {
      case 0: /* SEEK_SET */
        newpos = off;
        break;

      case 1: /* SEEK_CUR */
        newpos = filp->f_pos + off;
        break;

      case 2: /* SEEK_END */
        newpos = info->size + off;
        break;

      default: /* can't happen */
        return -EINVAL;
    }
    if (newpos<0)
		return -EINVAL;
    filp->f_pos = newpos;
    return newpos;
}
/*
 * ���豸����, ���豸�Ų���Ϊ>=1
 */
static int at91_flash_open(struct inode *inode, struct file *filp)
{
    flash_info_t *info; /* device information */
    int num = MINOR(inode->i_rdev);
    /*
     * the type and num values are only valid if we are not using devfs.
     * However, since we use them to retrieve the device pointer, we
     * don't need them with devfs as filp->private_data is already
     * initialized
     */
#if 0
    int type = TYPE(inode->i_rdev);


    /*
     * If private data is not valid, we are not using devfs
     * so use the type (from minor nr.) to select a new f_op
     */
    if (!filp->private_data && type) {
        if (type > SCULL_MAX_TYPE) return -ENODEV;
        filp->f_op = scull_fop_array[type];
        return filp->f_op->open(inode, filp); /* dispatch to specific open */
    }
#endif
    /* type 0, check the device number (unless private_data valid) */
    info = (flash_info_t *)filp->private_data;
    if (!info) {
        if (num >= 1)	// only open 0
			return -ENODEV;
        info = flash_info;
        filp->private_data = info; /* for other methods */
    }

    //MOD_INC_USE_COUNT;  /* Before we maybe sleep */
    /* now trim to 0 the length of the device if open was write-only */
#if 0
	if ( (filp->f_flags & O_ACCMODE) == O_WRONLY) {
        if (down_interruptible(&dev->sem)) {
            MOD_DEC_USE_COUNT;
            return -ERESTARTSYS;
        }
        scull_trim(dev); /* ignore errors */
        up(&dev->sem);
    }
#endif
    return 0;          /* success */
}

/*
 * IO���ƺ���, ��Ҫ�в�дFLASH, set lock-bit, clear lock-bit, ��FLASH�����
 */
static int at91_flash_ioctl(struct inode* inode,struct file* filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	int sect;
	flash_info_t *info = (flash_info_t *)filp->private_data;
	if (info == NULL) {
		return -ENODEV;
	}
	// check parameter
    if (_IOC_TYPE(cmd) != FLASH_IOCTL_BASE)
		return -ENOTTY;
    if (_IOC_NR(cmd) > FLASH_IOCTL_BASE)
		return -ENOTTY;
	if (_IOC_DIR(cmd) & _IOC_READ) {
		ret = verify_area(VERIFY_WRITE, (void *)arg, _IOC_SIZE(cmd));
	} else if (_IOC_DIR(cmd) & _IOC_WRITE) {
		ret =  verify_area(VERIFY_READ, (void *)arg, _IOC_SIZE(cmd));
	}
	if (ret)
		return -EFAULT;
	switch (cmd) {
		case ERASEBLOCK:	/* �����FLASH, Ҫ��һ��ֻ�ܲ�һ�� */
			ret = get_user(sect, (int *)arg);
			if (ret < 0)
				return ret;
			ret = erase_flash(info, sect, sect);
			break;
		case SETLOCKSECT:	/* ���ÿ�lock-bit */
			ret = get_user(sect, (int *)arg);
			if (ret < 0) return ret;
			ret = set_lock_bit(sect, info);
			break;
		case CLEARLOCKSECT:	/* �����lock-bit */
			ret = get_user(sect, (int *)arg);
			if (ret < 0) return ret;
			ret = clear_lock_bit(sect, info);
			break;
		default:
			ret = -ENOIOCTLCMD;
	}
	return ret;
}

/*
 * �ڵ�ǰ�α괦��ȡFLASHcount���ֽ�
 */
static int at91_flash_read(struct file *filp, char* buf, size_t count, loff_t *offp)
{
#if 0
#endif
	flash_info_t *info = (flash_info_t *)filp->private_data;
    int realcnt = count, fnum, lnum;
	void *ptr;
	size_t newpos, f_pos = (size_t)*offp, a_first = f_pos;
    int ret = 0;
	if (f_pos >= info->size)
        goto rout;
    if (f_pos + count > info->size)
        count = info->size - f_pos;

	// �׵�ַ����2��������
	fnum = f_pos % (info->portwidth / sizeof(unsigned char));
	if (fnum) {
		realcnt += info->portwidth / sizeof(unsigned char) - fnum;
		a_first = f_pos - fnum;
	}
	// ĩ��ַ������������2��������
	newpos = a_first + realcnt;
	lnum = newpos % (info->portwidth / sizeof(unsigned char));
	if (lnum) {
		realcnt += info->portwidth / sizeof(unsigned char) - lnum;
		newpos += info->portwidth / sizeof(unsigned char) - lnum;
	}
	// memory alloc
	if (realcnt >= SZ_128K) {
		ptr = vmalloc(realcnt);
	} else {
		ptr = kmalloc(realcnt, GFP_KERNEL);
	}
	if (ptr == NULL) {
		ret = -ENOMEM;
		goto rout;
	}
	// read flash
	ret = read_flash_data(info, ptr, a_first, realcnt);
	if (ret < 0) {
		goto rfout;
	}
	// copy data
	ret = copy_to_user(buf, ptr + fnum, count);
	if (ret) {
		ret = -EFAULT;
		goto rfout;
	}
	f_pos += count;
	*offp = f_pos;
	ret = count;

rfout:
	if (realcnt >= SZ_128K) {
		vfree(ptr);
	}else {
		kfree(ptr);
	}
rout:
	return ret;
}

/*
 * write data to current f_pos, flash address must not odd address
 */
static int at91_flash_write(struct file *filp, const char* buf, size_t count, loff_t *offp)
{
	flash_info_t *info = filp->private_data;
	int ret = -ENOMEM; /* value used in "goto out" statements */
	// write to flash, use write buffer command
	// write number of data is count, flash offset address is *offp
	size_t f_pos = *offp;
	int wcnt, rcnt = 0;
	char wbuf[32];
	if (f_pos & 1) {
		// odd address
		printk("not support write word to odd address\n");
		ret = -EFAULT;
		goto out;
	}
    /* do with count */
	while (count > 0) {
		if (count >= info->buffer_size) {
			wcnt = info->buffer_size;
		} else {
			wcnt = count;
		}
#ifdef DEBUG
		printk("rcnt: %d, f_pos: %d, wcnt: %d, count: %d\n",
			rcnt, f_pos, wcnt, count);
#endif
		ret = copy_from_user(wbuf, buf, wcnt);
		if (ret < 0) goto out;
		// write flash buffer wcnt data
		ret = write_flash_buffer(wbuf, wcnt, info, f_pos);
		if (ret < 0) goto out;
		// adjust pointer
		count -= wcnt;
		buf += wcnt;
		f_pos += wcnt;
		rcnt += wcnt;
	}
	return rcnt;
out:
	*offp = f_pos;
    return ret;
}

/*
 * release flash device
 */
static int at91_flash_release(struct inode *inode, struct file *filp)
{
    return 0;
}

struct file_operations at91_flash_fops = {
	.owner = THIS_MODULE,
	.open = at91_flash_open,
	.ioctl = at91_flash_ioctl,
	.write = at91_flash_write,
	.read = at91_flash_read,
	.llseek = at91_flash_llseek,
	.release = at91_flash_release,
};

/*
 * init flash device
 */
static __init int at91_flash_init(void)
{
	int ret;
	// ����ռ�
	flash_info = kmalloc(sizeof(flash_info_t), GFP_KERNEL);
	if (flash_info == NULL) {
		printk ("Unable to allocate AT91 FLASH device structure.\n");
		ret = -ENOMEM;
		goto out;
	}
	memset(flash_info, 0, sizeof(flash_info_t));
	// ����SMC�Ĵ���
	AT91_SYS->EBI_SMC2_CSR[0] = AT91C_SMC2_BAT | AT91C_SMC2_WSEN | (AT91C_SMC2_NWS & 0xA)
	| (AT91C_SMC2_TDF & 0xF00) | AT91C_SMC2_DBW_16 | (AT91C_SMC2_RWSETUP & 0x6);

	// ����Ҫӳ�������ַ
	ret = at91_flash_mem(flash_info);
	if (ret < 0) {
		printk ("Unable to flash MEM.\n");
		goto mmout;
	}
	// �������flash info
	flash_info->buffer_write_tout = FLASH_WRITE_TIMEOUT;
	flash_info->erase_blk_tout = FLASH_ERASE_TIMEOUT;
	flash_info->write_tout = FLASH_WRITE_TIMEOUT;
	flash_info->chipwidth = 16;
	flash_info->buffer_size = 32;
	flash_info->portwidth = 16;
	// ע���豸, �豸��Ϊ140
	ret = register_chrdev(FLASH_MAJOR, "NOR FLASH", &at91_flash_fops);				//ע���豸
	if (ret < 0) {
		printk(KERN_ERR "at91_flash: couldn't get a major %d for NOR FLASH.\n", FLASH_MAJOR);
		goto mmout;
	}
	printk("AT91 NOR FLASH driver v1.2\n");
	return ret;
mmout:
	kfree(flash_info);
out:
	return ret;
}

/*
 * exit flash device
 */
static void __exit at91_flash_exit(void)
{
	if (flash_info == NULL) {
		goto last;
	}
	if (flash_info->sect_addr[0]) {
		iounmap(flash_info->sect_addr[0]);
	}
	kfree(flash_info);
last:
	unregister_chrdev(FLASH_MAJOR, "NOR FLASH");
}

module_init(at91_flash_init);
module_exit(at91_flash_exit);
MODULE_LICENSE("Proprietary")
MODULE_AUTHOR("wjzhe")
MODULE_DESCRIPTION("FLASH driver for NKTY AT91RM9200")

