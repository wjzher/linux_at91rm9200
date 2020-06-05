/*
 * Copyright (c) 2007, NKTY Company
 * All rights reserved.
 * �ļ����ƣ�deal_purse.c
 * ժ    Ҫ�����崦����POS�Ľӿں���, ���е��õĵײ㺯������at91_uart.c, uart_dev.c
 *			 ���Ӷ�term_ram�ṹ����³�Ա����
 * 			 
 * ��ǰ�汾��1.1
 * ��    �ߣ�wjzhe
 * ������ڣ�2007��9��3��
 *
 * ȡ���汾��1.0
 * ԭ����  ��wjzhe
 * ������ڣ�2007��6��14��
 */
#include "pdccfg.h"
#include "deal_purse.h"
 
//#define DEBUG

/*
 * ����У��ֵ
 */
static int com_verify(term_ram *ptrm, unsigned char *buf, int cnt)
{
	int i;
	for (i = 0; i < cnt; i++, buf++) {
		ptrm->add_verify += *buf;
		ptrm->dif_verify ^= *buf;
	}
	return 0;
}
/*
 * ��ӡ���struct tradenum, ���ڴ������ʱ����, ��ǰ�ļ�ʹ��
 */
#if 1
static void out_tnum(struct tradenum *t_num)
{
	printk("%02x%02x %02x, %02x %02x: %d\n",
		t_num->s_tm.hyear, t_num->s_tm.lyear, t_num->s_tm.mon,
		t_num->s_tm.day, t_num->s_tm.hour, t_num->num);
}
#endif
/*
 * �����Ƿ��к�������������, ������Ҫ�·�������
 * write by wjzhe, June 12, 2007
 */
#define DEBUG
int compute_blk(term_ram *ptrm, struct black_info *pblkinfo)
{
	//int ret;
	// �ж��Ƿ������ȷ�Ľ��׺źͰ汾��
	// ֻ�ڽ��׺����д�����߽��׺�Ϊ0�����������ȡ���׺źͰ汾��
	if ((ptrm->blkinfo.trade_num.s_tm.hyear != 0x20)
		|| (ptrm->blkinfo.edition == 0)
		/* && (ptrm->blkinfo.trade_num.num == 0)*/) {
		//if (ptrm->blkinfo.trade_num.s_tm.hyear == 0x20) {
		//}
		// term has no black info, request it
#ifdef DEBUG
		printk("term has no black info: %02x, request it\n", ptrm->blkinfo.trade_num.s_tm.hyear);
		out_tnum(&ptrm->blkinfo.trade_num);
#endif
		if (purse_get_edition(ptrm) < 0) {
#ifdef DEBUG
			printk("purse_get_edition failed\n");
#endif
			return -1;
		}
	}
#if 0
	printk("globle blk info is: %d, %d\n", blkinfo->edition, blkinfo->trade_num.num);
	printk("term blk info is: %d, %d\n", ptrm->blkinfo.edition, ptrm->blkinfo.trade_num.num);
#endif
#ifdef DEBUG
	if (ptrm->blkinfo.edition != pblkinfo->edition) {
		printk("compute_blk term %d blk edition not match %d: %d\n",
			ptrm->term_no, ptrm->blkinfo.edition, pblkinfo->edition);
		//return -1;
	}
#endif
#if 0
	// �ȶԽ��׺��Ƿ�͵�ǰ��һ��
	ret = memcmp(&blkinfo->trade_num, &ptrm->blkinfo.trade_num, sizeof(ptrm->blkinfo.trade_num));
	printk("memcmp: %d\n", ret);
	if (ret == 0)
		return 0;
#endif
	// ���뱣֤���׺�����, �˷���ֵ����ȷ
#ifdef DEBUG
	//printk("LOCAL: %d, TERM %d, blk %d\n", pblkinfo->trade_num.num, ptrm->blkinfo.trade_num.num, pblkinfo->trade_num.num - ptrm->blkinfo.trade_num.num);
#endif
	return pblkinfo->trade_num.num - ptrm->blkinfo.trade_num.num;
}
#undef DEBUG
/*
 * �۰���Һ�����, �ȶԽ��׺Ŵ�С
 * write by wjzhe, June 12, 2007
 */
static black_acc *search_blk2(const unsigned long num, black_acc *pbacc, int cnt)
{
	int head, tail, mid;
	if (cnt <= 0 || !pbacc)		// cnt < 0, there is no account
		return NULL;
	head = 0;
	tail = cnt - 1;
	mid = (tail + head) / 2;
//	printk("%d %d pbacc[%d] = %d pbacc[%d] = %d\n", num, cnt, 
//		tail, pbacc[tail].t_num.num, 0, pbacc[0].t_num.num);
	do {
		if (pbacc[mid].t_num.num == num) {
			pbacc += mid;
			goto end;
		} else if (pbacc[mid].t_num.num > num) {
			tail = mid;
			mid = (tail + head) / 2;
		} else {
			head = mid;
			mid = (tail + head) / 2 + 1;
		}
	} while (head != tail && tail != (head + 1)/*mid <= tail && mid >= head*/);
	if (pbacc[head].t_num.num == num) {
		pbacc += head;
		goto end;
	}
	if (pbacc[tail].t_num.num == num) {
		pbacc += tail;
		goto end;
	}
//	printk("no find %d %d\n", pbacc[head].t_num.num, pbacc[tail].t_num.num);
//	printk("t h %d %d\n", tail, head);
	pbacc = NULL;
end:
	return pbacc;
}

/*
 * flag: 0->ctl0; 1->ctl2
 * send data to 485, return count is ok, -1 is timeout, -2 is other error
 * created by wjzhe, 10/16/2006
 * λʱ��34.7us
 */
static int send_data2(char *buf, size_t count, term_ram *ptrm)	// attention: can continue sending?
{
	int i;
	// enable send data channel
	AT91_SYS->PIOC_SODR &= (!0x00001000);
	AT91_SYS->PIOC_CODR |= 0x00001000;
	udelay(1);
	// ��������ʱ����Ҫ����ͨ��, ���ԭ��ͼ
	if (!(ptrm->term_no & 0x80)) {
		// ǰ��ͨ����UART0
		// ������ַλ
		uart_ctl0->US_CR &= !(AT91C_US_SENDA);
		for (i = 0; i < count; i++) {
			// ��buf���ݿ��������ͻ���
			uart_ctl0->US_THR = *buf;
			// �ȴ��������
			if (!uart_poll(&uart_ctl0->US_CSR, AT91C_US_TXEMPTY))
				return -1;
			// У��
			ptrm->add_verify += (unsigned char)(*buf);
			ptrm->dif_verify ^= (unsigned char)(*buf);
			buf++;
		}
	} else {
		// ����ͨ����UART2
		uart_ctl2->US_CR &= !(AT91C_US_SENDA);
		for (i = 0; i < count; i++) {
			uart_ctl2->US_THR = *buf;
			if (!uart_poll(&uart_ctl2->US_CSR, AT91C_US_TXEMPTY))
				return -1;
			ptrm->add_verify += (unsigned char)(*buf);
			ptrm->dif_verify ^= (unsigned char)(*buf);
			buf++;
		}
	}
#ifdef RS485_REV
	udelay(1);
#endif
	// enable receive data channel
	AT91_SYS->PIOC_CODR &= (!0x00001000);
	AT91_SYS->PIOC_SODR |= 0x00001000;
	udelay(1);
	return count;
}

/*
 * ���ͺ���������, N��
 * write by wjzhe, June 12, 2007
 */
static int send_blkacc(term_ram *ptrm, black_acc *pbacc, int cnt)
{
	int i, ret;
	if (cnt == 0)
		return 0;
	if (pbacc == NULL)
		return -1;
	for (i = 0; i < cnt; i++) {
		// �������ֽڿ���
		ret = send_data2((char *)&pbacc[i].card_num, 4, ptrm);
		if (ret < 0)
			goto send_blkacc_failed;
		// ����һ�ֽڲ�����¼
		ret = send_data2((char *)&pbacc[i].opt, 1, ptrm);
		if (ret < 0)
			goto send_blkacc_failed;
	}
	return 0;
send_blkacc_failed:
	ptrm->status = NOCOM;
	return -1;
}

/*
 * �����������׺�, 9 bytes, ��У��д��ram
 * write by wjzhe, June 12, 2007
 */
static int send_trade_num(struct tradenum *t_num, term_ram *ptrm)
{
	int ret;
	// ��4�ֽڼ�¼��
	ret =  send_data2((char *)&t_num->num, sizeof(t_num->num), ptrm);
	if (ret < 0)
		return -1;
	// ��5�ֽ�ʱ��
	ret = send_data2((char *)t_num->tm, sizeof(t_num->tm), ptrm);
	if (ret < 0)
		return -1;
	return 0;
}

/*
 * �պ��������׺�, 9 bytes, ��У��д��ram
 * write by wjzhe, June 12, 2007
 */
static int recv_trade_num(struct tradenum *t_num, term_ram *ptrm)
{
	unsigned char *tmp;
	int i, ret;
	tmp = (unsigned char *)&t_num->num;
	// ����4�ֽڼ�¼��
	for (i = 0; i < sizeof(t_num->num); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d t_num->num error, ret:%d\n", i, ret);
			ptrm->status = ret;
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// ����5�ֽ�ʱ��
	tmp = (unsigned char *)t_num->tm;
	for (i = 0; i < sizeof(t_num->tm); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d termblk.trade_num.tm error, ret:%d\n", i, ret);
			ptrm->status = ret;
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	return 0;
}

/*
 * ���͵�ǰУ����Ϣ
 * write by wjzhe, June 12, 2007
 */
static int send_verify(term_ram *ptrm)
{
	int ret;
	// ��1�ֽ��ۼ�У��
	ret = send_byte(ptrm->add_verify, ptrm->term_no);
	if (ret < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	// ��1�ֽ����У��
	ret = send_byte(ptrm->dif_verify, ptrm->term_no);
	if (ret < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	return 0;
}

/*
 * ���ղ�У�鵱ǰУ����Ϣ, ����<0˵��У�������߽���ʧ��
 * write by wjzhe, June 12, 2007
 */
static int chk_verify(term_ram *ptrm)
{
	int ret;
	unsigned char trmadd, trmdif;
	// ����1�ֽ��ۼӺ�
	ret = recv_data((char *)&trmadd, ptrm->term_no);
	if (ret < 0) {
		if (ret == -1) {
			ptrm->status = -1;
		}
		if (ret == -2) {
			ptrm->status = -2;
		}
		return ret;
	}
	// ����1�ֽ�����
	ret = recv_data((char *)&trmdif, ptrm->term_no);
	if (ret < 0) {
		if (ret == -1) {
			ptrm->status = -1;
		}
		if (ret == -2) {
			ptrm->status = -2;
		}
		return ret;
	}
	// ��֤У���
	if ((trmadd != ptrm->add_verify) || (trmdif != ptrm->dif_verify)) {
		printk("verify %d:%d, %d:%d\n", ptrm->add_verify, trmadd,
			ptrm->dif_verify, trmdif);
		return ERRVERIFY;
	}
	return 0;
}
/*
 * ���հ�����, ��ڲ���Ϊ��ǰ�ն�����ָ��
 * �������������յİ�����ֵ, ����0xFF˵�����մ���
 */
unsigned char recv_len(term_ram *ptrm)
{
	int ret;
	unsigned char len;
	ret = recv_data((char *)&len, ptrm->term_no);
	if (ret < 0) {
		printk("recv len error, ret:%d\n", ret);
		if (ret == -1) {
			ptrm->status = -1;
		}
		if (ret == -2) {
			ptrm->status = -2;
		}
		return 0xFF;
	}
	ptrm->dif_verify ^= len;
	ptrm->add_verify += len;
	return len;
}

/*
0x88:(1)���ݽ�����ȷ(2)����������Ҫ����ʱ�������������ɹ���
0x99:������������Ҫ�ն�ִ�о������ʱ�������������ȷ������ʧ�ܣ�
0xAA:���մ���0xBB�����ش���
 * flag == 0 ������ȷ
 * flag > 1  �ն˻�ִ�о������, ������ȷ, ������ʧ��
 * flag < 0  ���մ���
 * �˺�����������GA��, ���һ��GA����
 */
static int send_ga(term_ram *ptrm, int flag)
{
	int ret;
	ptrm->add_verify = ptrm->dif_verify = 0;
	// ��1�ֽ��ն˺�
	ret = send_byte(ptrm->term_no, ptrm->term_no);
	if (ret < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	ptrm->add_verify += ptrm->term_no;
	ptrm->dif_verify ^= ptrm->term_no;
	if (flag == 0) {
		ret = send_byte(0x88, ptrm->term_no);
		if (ret < 0) {
			ptrm->status = NOCOM;
			return -1;
		}
		ptrm->add_verify += 0x88;
		ptrm->dif_verify ^= 0x88;
	} else if (flag > 0) {
		ret = send_byte(0x99, ptrm->term_no);
		if (ret < 0) {
			ptrm->status = NOCOM;
			return -1;
		}
		ptrm->add_verify += 0x99;
		ptrm->dif_verify ^= 0x99;
	} else {
		ret = send_byte(0xAA, ptrm->term_no);
		if (ret < 0) {
			ptrm->status = NOCOM;
			return -1;
		}
		ptrm->add_verify += 0xAA;
		ptrm->dif_verify ^= 0xAA;
	}
	// ��������
	ret = send_byte(2, ptrm->term_no);
	if (ret < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	ptrm->add_verify += 2;
	ptrm->dif_verify ^= 2;
	// ��У��
	ret = send_verify(ptrm);
	return ret;
}

/*
 * ����GA, ����0����, <0����
 * write by wjzhe, June 12, 2007
 */
static int recv_ga(term_ram *ptrm)
{
	int ret;
	unsigned char recvno, ga, len;
	ptrm->add_verify = ptrm->dif_verify = 0;
	// ���ն˺�
	ret = recv_data((char *)&recvno, ptrm->term_no);
	if (ret < 0) {
		printk("recv ga termno failed, ret: %d\n", ret);
		if (ret == -1) {
			ptrm->status = -1;
		}
		if (ret == -2) {
			ptrm->status = -2;
		}
		return ret;
	}
	if ((recvno & 0x7F) != (ptrm->term_no & 0x7F)) {
		printk("recv ga termno error, %d: %d\n", recvno, ptrm->term_no);
		ptrm->status = NOCOM;
		usart_delay(1);
		return -1;
	}
	ptrm->add_verify += recvno;
	ptrm->dif_verify ^= recvno;
	// ��GA
	ret = recv_data((char *)&ga, ptrm->term_no);
	if (ret < 0) {
		printk("recv ga failed, ret: %d\n", ret);
		if (ret == -1) {
			ptrm->status = -1;
		}
		if (ret == -2) {
			ptrm->status = -2;
		}
		return ret;
	}
	switch (ga) {
		case 0x88:
		case 0x99:
			ret = 0;
			break;
		case 0xAA:
		case 0xBB:
			usart_delay(4);
			return -1;
		default:
			usart_delay(4);
			return -1;
	}
	ptrm->add_verify += ga;
	ptrm->dif_verify ^= ga;
	// �հ�����
	len = recv_len(ptrm);
	if (len == 0xFF) {
		printk("purse_send_time: recv len failed\n");
		return -1;
	}
	// ��У��
	ret = chk_verify(ptrm);
	return ret;
}

/*
 * �����ϵ��ʹ�����ݰ�, ����ʱ��, �������汾��, ������������Ϣ
 * write by wjzhe, June 8 2007
 */
int purse_send_conf(term_ram *ptrm, unsigned char *tm, struct black_info *blkinfo)
{
	struct black_info termblk;
	int ret, i;
	unsigned char len;//, nouse[2];
	unsigned short psamno;
//	psam_local *psl = NULL;
	unsigned char *tmp = (unsigned char *)&termblk.edition;
	//unsigned char verify;
	memset(&termblk, 0, sizeof(termblk));
	// ��1�ֽڰ�����
	len = recv_len(ptrm);
	if (len == 0xFF) {
		printk("purse_send_conf: recv len failed\n");
		return -1;
	}
	// ��4�ֽں������汾��
	for (i = 0; i < sizeof(termblk.edition); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d termblk.edition error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// ��9�ֽڽ��׺�
	ret = recv_trade_num(&termblk.trade_num, ptrm);
	if (ret < 0)
		return -1;
	// ��PSAM�����
	tmp = (unsigned char *)&psamno;
	for (i = 0; i < sizeof(psamno); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d PSAM NO. error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// ��2�ֽ�У���
	ret = chk_verify(ptrm);
	if (ret < 0)
		return -1;
	// save terminal black info.
	memcpy(&ptrm->blkinfo, &termblk, sizeof(termblk));
#if 0
	// ��ʱҪ��������PSAM����, ����Ӧ�����������ն˲�����
	psl = search_psam(psamno, psinfo);
	if (psl) {// �������ȷ�������򱣴��ն�PSAM������
		memcpy(ptrm->pterm->param.psam_passwd, psl->pwd, sizeof(psl->pwd));
	}
#endif
#if BWT_66J10
	udelay(BWT_66J10);
#endif
	// ���У��
	ptrm->add_verify = 0;
	ptrm->dif_verify = 0;
	// ��1�ֽ��ն˺�
	ret = send_byte(ptrm->term_no, ptrm->term_no);
	if (ret < 0) {
		goto send_failed;
	}
	ptrm->add_verify += ptrm->term_no;
	ptrm->dif_verify ^= ptrm->term_no;
	// ��������0x12
	ret = send_byte(PURSE_RET_PAR, ptrm->term_no);
	if (ret < 0) {
		goto send_failed;
	}
	ptrm->add_verify += PURSE_RET_PAR;
	ptrm->dif_verify ^= PURSE_RET_PAR;
	// ��1�ֽڰ�����
#ifndef QINGHUA
	len = 45 + 2 * ptrm->pterm->fee_n;
#else
	len = 45 + ptrm->pterm->fee_n;
#endif
	ret = send_data2((char *)&len, sizeof(len), ptrm);
	if (ret < 0)
		goto send_failed;
	// ������������45+2N (ʱ��7�����ò�����16���ʹ�����16���º������汾��4)
	ret = send_data2(tm, 7, ptrm);
	if (ret < 0) {
		goto send_failed;
	}
	ret = send_data2((char *)&ptrm->pterm->param, sizeof(ptrm->pterm->param), ptrm);
	if (ret < 0) {
		goto send_failed;
	}
	ret = send_data2((char *)&ptrm->pterm->tmsg, sizeof(ptrm->pterm->tmsg), ptrm);
	if (ret < 0) {
		goto send_failed;
	}
	ret = send_data2((char *)&blkinfo->edition, sizeof(blkinfo->edition), ptrm);
	if (ret < 0) {
		goto send_failed;
	}
	// 2N: �����ϵ��, N��Ӧ��Ա������
#ifndef QINGHUA
	for (i = 0; i < ptrm->pterm->fee_n; i++) {
		ret = send_data2(ptrm->pterm->mfee(i), 2, ptrm);
		if (ret < 0) {
			goto send_failed;
		}
	}
#else
	ret = send_data2((char *)ptrm->pterm->managefee, ptrm->pterm->fee_n, ptrm);
	if (ret < 0) {
		goto send_failed;
	}
#endif
	// ��У���
	ret = send_verify(ptrm);
	if (ret < 0)
		goto send_failed;
#ifdef DEBUG
	printk("send %d\n", 7+sizeof(ptrm->pterm->param)+sizeof(ptrm->pterm->tmsg)+sizeof(blkinfo->edition)+2*ptrm->pterm->fee_n+2);
	printk("send conf recv all data\n");
#endif
	return 0;
send_failed:
	ptrm->status = NOCOM;
	return -1;
}

/*
 * ���͵�ǰʱ��(YY, YY, MM, DD, HH, MM, SS)
 * write by wjzhe, June 8 2007
 */
int purse_send_time(term_ram *ptrm, unsigned char *tm)
{
	int ret;
	unsigned char len;
	// �հ�����
	len = recv_len(ptrm);
	if (len == 0xFF) {
		printk("purse_send_time: recv len failed\n");
		return -1;
	}
	// ��2�ֽ�У���
	ret = chk_verify(ptrm);
	if (ret < 0) {
		printk("purse send time chk error\n");
		return -1;
	}
#if BWT_66J10
	udelay(BWT_66J10);
#endif
	ptrm->add_verify = 0;
	ptrm->dif_verify = 0;
	// ���ն˺�
	ret = send_byte(ptrm->term_no, ptrm->term_no);
	if (ret < 0) {
		goto send_time_failed;
	}
	ptrm->add_verify += ptrm->term_no;
	ptrm->dif_verify ^= ptrm->term_no;
	// ��������0x16
	ret = send_byte(PURSE_RET_TIME, ptrm->term_no);
	if (ret < 0) {
		goto send_time_failed;
	}
	ptrm->add_verify += PURSE_RET_TIME;
	ptrm->dif_verify ^= PURSE_RET_TIME;
	// ���Ͱ�����
	len = 9;
	ret = send_data2((char *)&len, sizeof(len), ptrm);
	if (ret < 0) {
		goto send_time_failed;
	}
	// ����ǰʱ��7�ֽ�
	ret = send_data2(tm, 7, ptrm);
	if (ret < 0) {
		goto send_time_failed;
	}
	// ��2�ֽ�У��
	ret = send_verify(ptrm);
	if (ret < 0)
		goto send_time_failed;
	return 0;
send_time_failed:
	ptrm->status = NOCOM;
	return -1;
}

/*
 * �����ն˻����������׺�, ����ͨ��Ӧ���GA
 * �������������к�YYMDHXXXX��YYMDHΪ�ꡢ�¡��ա�ʱ
 * XXXXΪ����ʽ���кţ���һֱ���е��������º������汾��Ҳ������
 * write by wjzhe, June 8 2007
 */
int purse_recv_btno(term_ram *ptrm)
{
	struct tradenum t_num;
	unsigned char len;
	int ret;
	memset(&t_num, 0, sizeof(t_num));
	// �հ�����
	len = recv_len(ptrm);
	if (len == 0xFF) {
		printk("purse_send_time: recv len failed\n");
		return -1;
	}
	// ��9�ֽں������������к�
	ret = recv_trade_num(&t_num, ptrm);
	if (ret < 0) {
		printk("purse recv btno: recv trade num failed\n");
		return -1;
	}
	// ��2�ֽ�У���
	ret = chk_verify(ptrm);
	if (ret < 0) {
		printk("purse recv btno: chk verify failed\n");
		if (ret != ERRVERIFY) {
			usart_delay(2);
		}
		// ��GA�����
		ret = send_ga(ptrm, ret);
		if (ret < 0) {
			goto recv_btno_failed;
		}
		return ret;
	}
#if BWT_66J10
	udelay(BWT_66J10);
#endif
	// ��GA��
	ret = send_ga(ptrm, 0);
	if (ret < 0) {
		goto recv_btno_failed;
	}
	// ��˳������GA֮��, ��¼�ն˵Ľ��׺�
	memcpy(&ptrm->blkinfo.trade_num, &t_num, sizeof(t_num));
	return 0;
recv_btno_failed:
	ptrm->status = NOCOM;
	return -1;
}

/*
 * �����ն˻�δ�ϴ���ˮ
 * write by wjzhe, June 8 2007
 */
int purse_recv_flow(term_ram *ptrm)
{
	int ret, i, tail;
	m1_flow m1flow;
	unsigned char *tmp;
	unsigned char nouse[4], len;
	memset(&m1flow, 0, sizeof(m1flow));
	// �հ�����
	len = recv_len(ptrm);
	if (len == 0xFF) {
		printk("purse_send_time: recv len failed\n");
		return -1;
	}
	// ����48�ֽ���ˮ��Ϣ
	// ��ˮʱ��
	tmp = (unsigned char *)&m1flow.date.hyear;
	for (i = 0; i < sizeof(m1flow.date); i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			ptrm->status = ret;
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp--;
	}
	// ����
	tmp = (unsigned char *)&m1flow.card_num;
	for (i = 0; i < sizeof(m1flow.card_num); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d m1flow.card_num error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
	// �˺�
	tmp = (unsigned char *)&m1flow.acc_num;
	for (i = 0; i < sizeof(m1flow.acc_num); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d m1flow.acc_num error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
#ifndef QINGHUA
	// ��ǰ�ʹ������ܶ�
	tmp = (unsigned char *)&m1flow.money_sum;
	for (i = 0; i < sizeof(m1flow.money_sum); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d m1flow.money_sum error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
	// 2�ֽڴ���չ
	for (i = 0; i < 2; i++) {
		ret = recv_data(&nouse[i], ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->add_verify += nouse[i];
		ptrm->dif_verify ^= nouse[i];
	}
	// ����Ѳ��
	tmp = (unsigned char *)&m1flow.manage_fee;
	for (i = 0; i < 3; i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d m1flow.manage_fee error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
#else
	// ��ǰ�ʹ������ܶ�, �廪ֻ��3�ֽ�
	tmp = (unsigned char *)&m1flow.money_sum;
	for (i = 0; i < 3; i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d m1flow.money_sum error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
	// 4�ֽ�TAC
	tmp = (unsigned char *)&m1flow.qh_tac;
	for (i = 0; i < sizeof(m1flow.qh_tac); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d m1flow.qh_tac error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
	// ����Ѳ��, �廪ֻ��2�ֽ�
	tmp = (unsigned char *)&m1flow.manage_fee;
	for (i = 0; i < 2; i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d m1flow.manage_fee error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
#endif
	// ����������ñ��
	tmp = (unsigned char *)&m1flow.tml_num;
	for (i = 0; i < sizeof(m1flow.tml_num); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d m1flow.tml_num error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// ���������������
	tmp = (unsigned char *)&m1flow.areano;
	for (i = 0; i < sizeof(m1flow.areano); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d m1flow.tml_num error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// �������PSAM�����
	tmp = (unsigned char *)&m1flow.psam_num;
	for (i = 0; i < sizeof(m1flow.psam_num); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d m1flow.psam_num error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// �������PSAM���������
	tmp = (unsigned char *)&m1flow.psam_trd_num;
	for (i = 0; i < sizeof(m1flow.psam_trd_num); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d m1flow.psam_trd_num error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// Ǯ���������
	tmp = (unsigned char *)&m1flow.walt_num;
	for (i = 0; i < sizeof(m1flow.walt_num); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d m1flow.walt_num error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// ���׽��
	tmp = (unsigned char *)&m1flow.consume_sum;
	for (i = 0; i < sizeof(m1flow.consume_sum); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d m1flow.consume_sum error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// Ǯ�����׺����
	tmp = (unsigned char *)&m1flow.remain_money;
	for (i = 0; i < sizeof(m1flow.remain_money); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d m1flow.remain_money error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// ��Ʒ������
	tmp = (unsigned char *)&m1flow.food_grp_num;
	for (i = 0; i < sizeof(m1flow.food_grp_num); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d m1flow.food_grp_num error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// �������ͱ�ʶ
	tmp = (unsigned char *)&m1flow.deal_type;
	for (i = 0; i < sizeof(m1flow.deal_type); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d m1flow.deal_type error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// ��ˮ����
	tmp = (unsigned char *)&m1flow.flow_type;
	for (i = 0; i < sizeof(m1flow.flow_type); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d m1flow.flow_type error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// tac code
#ifndef QINGHUA
	tmp = (unsigned char *)&m1flow.tac_code;
#else
	tmp = (unsigned char *)nouse;
#endif
	//for (i = 0; i < sizeof(m1flow.tac_code); i++) {
	for (i = 0; i < 2; i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d m1flow.tac_code error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// �Ƿ��ϴ���־
	for (i = 0; i < 1; i++) {
		ret = recv_data(&nouse[2], ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= nouse[2];
		ptrm->add_verify += nouse[2];
	}
	// ����2�ֽ�У��
	if ((ret = chk_verify(ptrm)) < 0) {
		if (ret != ERRVERIFY) {
			usart_delay(2);
		} else 
		// ������У����������·���GA
		send_ga(ptrm, ret);
		return -1;
	}
	// ������ˮ�洢ǰ�ж��Ƿ���δ�ϴ���ˮ
	if (nouse[2] == 0xFE) {
#if 0
		printk("flow no.: %d; tradenum: %d\n", maxflowno + 1, m1flow.psam_trd_num);
#endif
		// ����ͨ��Ӧ���(��ַ, ����, У��)
		if (send_ga(ptrm, 0) < 0) {// ��������ѳ���, ����UARTӲ���д���
			ptrm->status = NOTERMINAL;
			return -1;
		}
		return 0;
	}
	// ��������ˮ����Ϊ������ˮ���д��ж�
	if ((ptrm->psam_trd_num) && (m1flow.deal_type != 0x99)
		&& !(m1flow.flow_type & (1 << 1))) {
		if (m1flow.psam_trd_num <= ptrm->psam_trd_num) {
			// �Ѿ��ϴ�����ˮ
			// ����ͨ��Ӧ���(��ַ, ����, У��)
			if (send_ga(ptrm, 0) < 0) {// ��������ѳ���, ����UARTӲ���д���
				ptrm->status = NOTERMINAL;
				return -1;
			}
			return 0;
		}
	}
#if BWT_66J10
	udelay(BWT_66J10);
#endif
	//if (!(m1flow.flow_type & (1 << 1)))
	//ptrm->flow_flag = ptrm->term_no;
	// term_money plus
	// �˴�û����TAC����֤, ֻ�Ǽ򵥵��ж��Ƿ�����ȷ��ˮ
	if (!(m1flow.flow_type & (1 << 1))) {
		if (m1flow.deal_type == 0x99) {
			// ������ˮ, �������ն˽��
		} else {
			// ֻ��������ˮ�ҷ�������ˮ�ż�¼�ն˽������
			ptrm->psam_trd_num = m1flow.psam_trd_num;
			ptrm->term_money += m1flow.consume_sum;
			ptrm->term_cnt++;
		}
	}
	// ��ˮ�ŵĴ���
	m1flow.flow_num = maxflowno++;
	// ����ѵĴ���
	{
		int posneg = 0;
		if (sizeof(m1flow.manage_fee) == sizeof(int)) {
			posneg = 0x80000000;
		} else if (sizeof(m1flow.manage_fee) == sizeof(short)) {
			posneg = 0x8000;
		}
		if (m1flow.manage_fee & posneg) {
			// ���Ǹ��Ĺ����, ��Ҫ���⴦��
			m1flow.manage_fee &= ~(posneg);
			m1flow.manage_fee *= -1;
		}
	}
	//printk("recv cpu flow tail: %d\n", tail);
	tail = flowptr.tail;
	if (tail >= FLOWANUM) {
		printk("uart recv flow no space\n");
		// ����ͨ��Ӧ���(��ַ, ����, У��)
		if (send_ga(ptrm, -1) < 0) {// ��������ѳ���, ����UARTӲ���д���
			ptrm->status = NOTERMINAL;
			return -1;
		}
		return -1;
	}
	memcpy(&pflow[tail], &m1flow, sizeof(flow));
	tail++;
#if 0
	if (tail == FLOWANUM)
		tail = 0;
#endif
	flowptr.tail = tail;
	flow_sum++;
	space_remain--;
	// ����ͨ��Ӧ���(��ַ, ����, У��)
	if (send_ga(ptrm, 0) < 0) {// ��������ѳ���, ����UARTӲ���д���
		ptrm->status = NOTERMINAL;
		return -1;
	}
	return 0;
}

/*
 * ʵʱ�����ն˻�ȫ������������
 * ����������λ�����������´����������ݣ��ն˽��պ󽫴���FLASH�У�
 * ��λ�����ͱ������ʱҪ��֤���������׺ź��ն˵�ǰ�Ľ��׺�������
 * �����ն˽����洢�����ݰ������ݣ�
 * �ն˷���ͨ��Ӧ���������д��FLASH֮��
 * �Ͷ��ڸ��º��������ݵ���ͬ����ͬ���Ǳ������������������λ����
 * �������ݸ�ʽҲ��ͬ������ͬ���ǽ������̣����ڸ��½����ǵ�Ե�ѭ�������ģ�
 * ��������ֻ��һ�ν�������λ����ɱ��������������ת�������ն��ϣ�
 * ��һ�δ�������ʱ�ٴ�����ִ�б��������̡�
 * ���: ��ǰ�ն�����ָ��, ϵͳ��������Ϣ, ��������ͷָ��
 * ����0��ȷ, <0����
 * write by wjzhe, June 8 2007
 */
//#define DEBUG
int purse_send_bkin(term_ram *ptrm, struct black_info *blkinfo, black_acc *blkacc, int flag)
{
#ifndef MAXBLKCNT
#define MAXBLKCNT 20
#endif
	int ret;
	unsigned char n, len = 0;
	unsigned char recvno, sendno;
	black_acc *phead;
	if (blkacc == NULL)
		return 0;
	// ��ȡҪ���غ�����������, ֻ�ǻ�ü򵥵�����
	ret = compute_blk(ptrm, blkinfo);
	if (ret < 0) {
#ifdef DEBUG
		printk("compute blk error\n");
#endif
		return -1;
	} else if (ptrm->blkinfo.edition != blkinfo->edition) {
		// �汾�Ų���, Ҫ���к���������
#ifdef DEBUG
		printk("edition error send all, POS: %d, SRV: %d\n",
			ptrm->blkinfo.edition, blkinfo->edition);
#endif
		if (ret == 0) {
			// ѹ��û������??
		}
	} else if ((ret == 0) && (flag)) {
#ifdef DEBUG
		//printk("no new black\n");
#endif
		return 0;
	}
	//if (ptrm->blkinfo.trade_num.num == 0) {
		// ��ʱ�ն˻���û���κκ�����
	//	ret -= blkacc[0].t_num.num - 1;
	//}
	// ���Һ�������
	if ((ptrm->blkinfo.trade_num.num > 0)
		// �ն˻��İ汾��һ���ҽ��׺�>0�Ż���Һ�������
		/*&& (ptrm->blkinfo.edition == blkinfo->edition)*/) {
		phead = search_blk2(ptrm->blkinfo.trade_num.num, blkacc, blkinfo->count);
		if (phead == NULL) {
			// �˽��׺Ų��ں������б���, �޷����к���������
#ifdef DEBUG
			printk("trade number is not in black list\n");
			out_tnum(&ptrm->blkinfo.trade_num);
#endif
			return -1;
		}
		// ��ǰ�õ��ĺ������Ѿ������غ��, ��Ҫ+1
		phead += 1;
	} else if (blkinfo->count > 0) {
#ifdef DEBUG
		printk("phead = blkacc\n");
#endif
		phead = blkacc;
		ret = blkacc[blkinfo->count - 1].t_num.num - blkacc[0].t_num.num + 1;
	} else {
		phead = NULL;
		ret = 0;
	}
	// ��һ����Ҫ�·�n������������
	if (ret <= MAXBLKCNT) {
		n = ret;
		n |= 3 << 6;
		len = 4;
	} else {
		n = MAXBLKCNT;
		len = 0;
	}
#ifdef DEBUG
	if (phead) {
		printk("send : phead->no %d\n", phead->card_num);
		printk("send : phead->opt %02x\n", phead->opt);
	}
	printk("send %d black acc, ret %d, blkinfo->count %d, \nblkinfo->trade_num.num %d\n", n & 0x3F, ret, blkinfo->count, blkinfo->trade_num.num);
#endif
	// �к�
	sendno = ptrm->term_no | 0x80;
	ret = send_addr(sendno, ptrm->term_no);
	if (ret < 0)
		goto send_bkin_failed;
	// �غ�
	ret = recv_data((char *)&recvno, ptrm->term_no);
	if (ret < 0) {
#ifdef DEBUG
		printk("recv data %d: ret -> %d\n", ptrm->term_no, ret);
#endif
		ptrm->status = ret;
		return ret;
	}
	// �жϻ������Ƿ�һ��
	if ((recvno & 0x7F) != (ptrm->term_no & 0x7F)) {
#ifdef DEBUG
		printk("send bkin return term no. error %02x: %02x\n", recvno, ptrm->term_no);
#endif
		ptrm->status = NOCOM;
		return -1;
	}
	ptrm->add_verify = 0;
	ptrm->dif_verify = 0;
	// ��1�ֽ��ն˺�
	ret = send_byte(ptrm->term_no, ptrm->term_no);
	if (ret < 0) {
		goto send_bkin_failed;
	}
	com_verify(ptrm, &ptrm->term_no, sizeof(ptrm->term_no));
	// ��������
	ret = send_byte(PURSE_PUT_BLK, ptrm->term_no);
	if (ret < 0) {
		goto send_bkin_failed;
	}
	ptrm->add_verify += PURSE_PUT_BLK;
	ptrm->dif_verify ^= PURSE_PUT_BLK;
	// ��������
	len += 12 + 5 * (n & 0x3F);
	ret = send_data2(&len, sizeof(len), ptrm);
	if (ret < 0) {
#ifdef DEBUG
		printk("sendlen failed\n");
#endif
		return -1;
	}
	// ��1�ֽڰ�ͷ
	// bit5-bit0�������ݰ�������������
	// bit7, bit6 == 0x3������ȫ���������
	ret = send_byte(n, ptrm->term_no);
	if (ret < 0)
		goto send_bkin_failed;
	com_verify(ptrm, &n, sizeof(n));
	// ��9�ֽڽ��׺�
	if (phead == NULL) {
		// ��phead�����ж�!!!
		ret = send_trade_num(&blkinfo->trade_num, ptrm);
		if (ret < 0)
			goto send_bkin_failed;
	} else {
		ret = send_trade_num(&phead[(n & 0x3F) - 1].t_num, ptrm);
		if (ret < 0)
			goto send_bkin_failed;
	}
/*	ret =  send_data2((char *)&phead[(n & 0x3F) - 1].t_num.num, sizeof(phead[(n & 0x3F) - 1].t_num.num), ptrm);
	if (ret < 0)
		goto send_bkin_failed;
	ret = send_data2((char *)phead[(n & 0x3F) - 1].t_num.tm, sizeof(phead[(n & 0x3F) - 1].t_num.tm), ptrm);
	if (ret < 0)
		goto send_bkin_failed;*/
	// ��5 * N�ֽں���������
	//n &= 0x3F;
	if ((ret = send_blkacc(ptrm, phead, n & 0x3F)) < 0)
		return -1;
	// ���������汾��
	if (n & 0xC0) {
		ret = send_data2((char *)&blkinfo->edition, sizeof(blkinfo->edition), ptrm);
		if (ret < 0)
			goto send_bkin_failed;
	}
	// ��2�ֽ�У��
	ret = send_verify(ptrm);
	if (ret < 0)
		goto send_bkin_failed;
#if BWR_66J10
	udelay(BWR_66J10);
#endif
	// ��GA
	if (recv_ga(ptrm) < 0) {
#ifdef DEBUG
		printk("send bkin recv GA error: %d\n", ret);
#endif
		return -1;
	}
	if ((n & 0xC0) && (!ptrm->black_flag)) {
		ptrm->black_flag = 1;
#ifdef DEBUG
		printk("send bkin set black flag 1\n");
#endif
	} else {
#ifdef DEBUG
		printk("send bkin not set black flag 1, n %d, flag %d\n", n, ptrm->black_flag);
#endif
	}
	// ������
	if (phead)
		memcpy(&ptrm->blkinfo.trade_num, &phead[n - 1].t_num, sizeof(phead[n - 1].t_num));
	return 0;
send_bkin_failed:
#ifdef DEBUG
	printk("send_bkin_failed\n");
#endif
	ptrm->status = NOCOM;
	return -1;
}
#undef DEBUG
#if 0
/*
 * ���ڸ����ն˻�ȫ������������, δ��
 * write by wjzhe, June 8 2007
 */
int purse_send_bkta(term_ram *ptrm, struct black_info *blkinfo)
{
#ifndef MAXBLKCNT
#define MAXBLKCNT 20
#endif
	unsigned char recvno, cmd, *tmp;
	int ret, i;
	// ��1�ֽڵ�ַ
	ret = send_data2((char *)&ptrm->term_no, 1, ptrm);
	if (ret < 0)
		goto send_bkta_failed;
	// ��0x32, �ն˽���FLASH����
	ret = send_byte(PURSE_PUT_ABLK, ptrm->term_no);
	if (ret < 0)
		goto send_bkta_failed;
	ptrm->add_verify += ptrm->term_no;
	ptrm->dif_verify ^= ptrm->term_no;
	// ��У��
	ret = send_verify(ptrm);
	if (ret < 0)
		goto send_bkta_failed;
	// ��Ӧ����Ϣ��
	ptrm->add_verify = 0;
	ptrm->dif_verify = 0;
	// term number
	ret = recv_data(&recvno, ptrm->term_no);
	if (ret < 0)
		goto bkta_recv_failed;
	if ((recvno & 0x7F) != (ptrm->term_no & 0x7F)) {
		printk("send bkta return term no. error %02x: %02x\n", recvno, ptrm->term_no);
		ptrm->status = NOCOM;
		return -1;
	}
	ptrm->add_verify += recvno;
	ptrm->dif_verify ^= recvno;
	// command
	ret = recv_data(&cmd, ptrm->term_no);
	if (ret < 0)
		goto bkta_recv_failed;
	if (cmd != PURSE_RET_ABLK) {
		printk("ret cmd error, %02x, 33\n", cmd);
		ptrm->status = NOCOM;
		return -1;
	}
	ptrm->add_verify += cmd;
	ptrm->dif_verify ^= cmd;
	tmp = (unsigned char *)&ptrm->blkinfo.trade_num.num;
	for (i = 0; i < sizeof(ptrm->blkinfo.trade_num.num); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d ptrm->blkinfo.trade_num.num error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	tmp = ptrm->blkinfo.trade_num.tm;
	for (i = 0; i < sizeof(ptrm->blkinfo.trade_num.tm); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d ptrm->blkinfo.trade_num.tm error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	ret = compute_blk(ptrm, blkinfo);
	if (ret < 0) {
		return -1;
	} else if (ret == 0) {
		return 0;
	}
// ѭ�������²���
	// ��1�ֽڰ�ͷ,
	// bit5-bit0�������ݰ�������������
	// bit7, bit6 == 0x3������ȫ���������
	// ��9�ֽڽ��׺�
	// ��5 * N�ֽں���������
	// ��2�ֽ�У��
	// ��GA

	return 0;
send_bkta_failed:
	ptrm->status = NOCOM;
	return -1;
bkta_recv_failed:
	ptrm->status = ret;
	return ret;
}
#endif

/*
 * �㲥��ʽ�����ն˻�ȫ������������, �˹���ֻ������
 * write by wjzhe, June 11 2007
 */
int purse_broadcast_blk(black_acc *pbacc, struct black_info *pbinfo, term_ram *ptrm, int cnt)
{
#ifndef MAXBLKCNT
#define MAXBLKCNT 20
#endif
#ifndef ADDTIMENUM
#define ADDTIMENUM 50000
#endif
#define FC 0
#define LC 0xFF
#define BC 0xFF
	//unsigned char vadd = 0, vdif = 0, *tmp;
	int once, left = pbinfo->count, ret, i;
	unsigned char len = 0;
	term_ram term_tmp;// ������Ϊ��������, �����������ն�����
	if (pbinfo->count <= 0)
		return 0;
	memset(&term_tmp, 0, sizeof(term_tmp));
	// ���Ͳ�������
	// ��ǰ��ͨ�����л������й㲥
	term_tmp.term_no = FC;
	ret = send_addr(BC, term_tmp.term_no);
	if (ret < 0)
		return -1;
	term_tmp.add_verify += BC;
	term_tmp.dif_verify ^= BC;
	// ������
	ret = send_byte(PURSE_BCT_ERASE, term_tmp.term_no);
	if (ret < 0)
		return -1;
	term_tmp.add_verify += PURSE_BCT_ERASE;
	term_tmp.dif_verify ^= PURSE_BCT_ERASE;
	// ��������
	ret = send_byte(11, term_tmp.term_no);
	if (ret < 0)
		return -1;
	term_tmp.add_verify += 11;
	term_tmp.dif_verify ^= 11;
	// �����׺�, �µ���ʼ���׺�
	ret = send_trade_num(&pbacc->t_num, &term_tmp);
	if (ret < 0)
		return -1;
	// ��У��
	ret = send_verify(&term_tmp);
	if (ret < 0) {
		return -1;
	}
	// ������ͨ�����л���������
	memset(&term_tmp, 0, sizeof(term_tmp));
	term_tmp.term_no = LC;
	ret = send_addr(BC, term_tmp.term_no);
	if (ret < 0)
		return -1;
	term_tmp.add_verify += BC;
	term_tmp.dif_verify ^= BC;
	// ������
	ret = send_byte(PURSE_BCT_ERASE, term_tmp.term_no);
	if (ret < 0)
		return -1;
	term_tmp.add_verify += PURSE_BCT_ERASE;
	term_tmp.dif_verify ^= PURSE_BCT_ERASE;
	// ��������
	ret = send_byte(11, term_tmp.term_no);
	if (ret < 0)
		return -1;
	term_tmp.add_verify += 11;
	term_tmp.dif_verify ^= 11;
	// �����׺�
	ret = send_trade_num(&pbacc->t_num, &term_tmp);
	if (ret < 0)
		return -1;
	// ��У��
	ret = send_verify(&term_tmp);
	if (ret < 0) {
		return -1;
	}
	// �˴���Ҫ�ȴ�һ��ʱ��
	mdelay(3000);
	// ���������ն˽��׺�, �鿴�Ƿ�Ϊ�µ���ʼ���׺�-1, �������˳�
	for (i = 0; i < cnt; i++) {
		memcpy(&term_tmp, ptrm, sizeof(term_tmp));
		sel_ch(term_tmp.term_no);
		udelay(5);
		if (purse_get_edition(&term_tmp) < 0) {
			printk("broad cast get terminal edition failed: %d, %d\n", i, term_tmp.term_no);
			ptrm++;
			continue;
		}
		if ((term_tmp.blkinfo.trade_num.num != (pbacc->t_num.num - 1))
			|| term_tmp.blkinfo.edition) {
			printk("erase term %d failed\n", term_tmp.term_no);
			printk("term_tmp.blkinfo.trade_num.num: %d, pbinfo->trade_num.num: %d\n",
				term_tmp.blkinfo.trade_num.num, pbinfo->trade_num.num);
			return -1;
		}
		ptrm++;
	}
/*
1: bit5-bit0�������ݰ����ĺ�����������bit7bit6=11b,������ȫ�����ꣻ
9��BYTE8-BYTE0:���±������ݺ��µĽ��׺ţ�
5N:5�ֽ�/������������ֽڱ�ʶ��ҹ�ʧ��
(4)�������ܻ��У����������½���ʱ�а汾��
*/
	while (left) {
		unsigned char n = 0;
		unsigned char cmd = 0;
		memset(&term_tmp, 0, sizeof(term_tmp));
		len = 0;
		// ʣ��left������, ���һ��20
		if (left > MAXBLKCNT) {
			once = MAXBLKCNT;
			n = once;
		} else {
			once = left;
			n = once;
			n |= 0xC0;
			len = 4;
		}
		// ǰ��ͨ��
		// ��1�ֽڵ�ַ��Ϣ0xFF
		term_tmp.term_no = FC;
		cmd = BC;
		ret = send_addr(cmd, term_tmp.term_no);
		if (ret < 0)
			return -1;
		com_verify(&term_tmp, &cmd, 1);
		// ��0x37
		ret = send_byte(PURSE_BCT_BLK, term_tmp.term_no);
		if (ret < 0)
			return -1;
		term_tmp.add_verify += PURSE_BCT_BLK;
		term_tmp.dif_verify ^= PURSE_BCT_BLK;
		// ��������
		len += 12 + 5 * (n & 0x3F);
		//len += 12 + 5 * (n & 0x3F) + ((n & 0xC0) ? 4 : 0);
		ret = send_data2(&len, sizeof(len), &term_tmp);
		if (ret < 0)
			return -1;
		// ѭ����1+9+5N+(4)+2(N <= 10)
		ret = send_byte(n, term_tmp.term_no);
		if (ret < 0)
			return -1;
		com_verify(&term_tmp, &n, 1);
		// ��9�ֽڽ��׺�
		ret = send_trade_num(&pbacc[once - 1].t_num, &term_tmp);
		if (ret < 0)
			return -1;
		// ��5 * N�ֽں���������
		if ((ret = send_blkacc(&term_tmp, pbacc, once)) < 0)
			return -1;
		if (n & 0xC0) {// ˵�������һ������
			// �����ֽڰ汾��
			ret = send_data2((char *)&pbinfo->edition, sizeof(pbinfo->edition), &term_tmp);
			if (ret < 0)
				return -1;
		}
		// ��У��
		ret = send_verify(&term_tmp);
		if (ret < 0)
			return -1;
		// �������к���ͨ��
		// ��1�ֽڵ�ַ��Ϣ0xFF
		memset(&term_tmp, 0, sizeof(term_tmp));
		term_tmp.term_no = LC;
		//cmd = BC;
		ret = send_addr(cmd, LC);
		if (ret < 0)
			return -1;
		com_verify(&term_tmp, &cmd, 1);
		// ��0x37
		ret = send_byte(PURSE_BCT_BLK, term_tmp.term_no);
		if (ret < 0)
			return -1;
		term_tmp.add_verify += PURSE_BCT_BLK;
		term_tmp.dif_verify ^= PURSE_BCT_BLK;
		// ��������
		ret = send_data2(&len, sizeof(len), &term_tmp);
		if (ret < 0)
			return -1;
		// ѭ����1+9+5N+(4)+2(N <= 10)
		ret = send_byte(n, term_tmp.term_no);
		if (ret < 0)
			return -1;
		com_verify(&term_tmp, &n, 1);
		// ��9�ֽڽ��׺�
		ret = send_trade_num(&pbacc[once - 1].t_num, &term_tmp);
		if (ret < 0)
			return -1;
		// ��5 * N�ֽں���������
		if ((ret = send_blkacc(&term_tmp, pbacc, once)) < 0)
			return -1;
		if (n & 0xC0) {// ˵�������һ������
			// �����ֽڰ汾��
			ret = send_data2((char *)&pbinfo->edition, sizeof(pbinfo->edition), &term_tmp);
			if (ret < 0)
				return -1;
		}
		// ��У��
		ret = send_verify(&term_tmp);
		if (ret < 0)
			return -1;
		// ͨѶ����, ���б������ݴ���
		pbacc += once;
		// ������������ADDTIMENUM����, �ӳ�ʱ���Ϊ15ms
		if (pbacc->t_num.num > ADDTIMENUM)
			mdelay(15);
		else
			mdelay(10);
		left -= once;
	}
	return 0;
}

/*
 * ǿ�Ƹ��µ�Ʒ�ֺͿ�ݷ�ʽ�¸�����������
 * write by wjzhe, June 11 2007
 */
int purse_update_key(term_ram *ptrm)
{
	unsigned char sendno, recvno;
	int ret;
{
	unsigned char price[32] = {0};
	// ��ֵȫ0���˳�
	if (!memcmp(ptrm->pterm->price, price, sizeof(price))) {
		return 0;
	}
}
	ptrm->add_verify = 0;
	ptrm->dif_verify = 0;
	// �к�
	sendno = ptrm->term_no | 0x80;
	ret = send_addr(sendno, ptrm->term_no);
	if (ret < 0)
		goto update_key_failed;
	com_verify(ptrm, &sendno, sizeof(sendno));
	// �غ�
	ret = recv_data((char *)&recvno, ptrm->term_no);
	if (ret < 0) {
#ifdef DEBUG
		printk("purse update key recv no. %d: error %d\n", ptrm->term_no, ret);
#endif
		ptrm->status = ret;
		return ret;
	}
	// �жϻ������Ƿ�һ��
	com_verify(ptrm, &recvno, sizeof(recvno));
	if ((recvno & 0x7F) != (ptrm->term_no & 0x7F)) {
		printk("update key return term no. error %02x: %02x\n", recvno, ptrm->term_no);
		ptrm->status = NOCOM;
		return -1;
	}
	// ��1�ֽڵ�ַ
	// ���ն˺Ų��� | 0x80
	sendno = ptrm->term_no;
	ptrm->add_verify = 0;
	ptrm->dif_verify = 0;
	ret = send_addr(sendno, ptrm->term_no);
	if (ret < 0)
		goto update_key_failed;
	ptrm->add_verify += sendno;
	ptrm->dif_verify ^= sendno;
	// ��0x34������
	ret = send_byte(PURSE_UPDATE_KEY, ptrm->term_no);
	if (ret < 0)
		goto update_key_failed;
	ptrm->add_verify += PURSE_UPDATE_KEY;
	ptrm->dif_verify ^= PURSE_UPDATE_KEY;
	// ��������
	ret =  send_byte(34, ptrm->term_no);
	if (ret < 0)
		goto update_key_failed;
	ptrm->add_verify += 34;
	ptrm->dif_verify ^= 34;
	// ��������2+30, 2:��Ʒ�ַ�ʽ�µ��ۣ�30:��ݷ�ʽ��15�����������
	ret = send_data2((char *)ptrm->pterm->price, 32, ptrm);
	if (ret < 0)
		goto update_key_failed;
	// ��2�ֽ�У������
	ret = send_verify(ptrm);
	if (ret < 0)
		goto update_key_failed;
	// ��GA
	ret = recv_ga(ptrm);
	if (ret < 0) {
		printk("purse update key recv GA error\n");
		return -1;
	}
	return 0;
update_key_failed:
	ptrm->status = -1;
	return -1;
}

/*
 * ����ն˻��������汾�źͽ��׺�
 * write by wjzhe, June 11 2007
 */
int purse_get_edition(term_ram *ptrm)
{
	unsigned char sendno, recvno, cmd, *tmp, len;
	int ret, i;
	struct black_info info;
	memset(&info, 0, sizeof(info));
	// ��term_num | 0x80
	// �к�
	sendno = ptrm->term_no | 0x80;
	ret = send_addr(sendno, ptrm->term_no);
	if (ret < 0)
		goto get_edition_failed;
	//com_verify(ptrm, &sendno, sizeof(sendno));
	// ��term_num, ���ж�
	// �غ�
	ret = recv_data((char *)&recvno, ptrm->term_no);
	if (ret < 0) {
		ptrm->status = ret;
		return ret;
	}
	// �жϻ������Ƿ�һ��
	if ((recvno & 0x7F) != (ptrm->term_no & 0x7F)) {
		printk("get edition return term no. error %02x: %02x\n", recvno, ptrm->term_no);
		ptrm->status = NOCOM;
		return -1;
	}
	//com_verify(ptrm, &recvno, sizeof(recvno));
	ptrm->add_verify = ptrm->dif_verify = 0;
	// ��1�ֽ�term_num
	ret = send_byte(ptrm->term_no, ptrm->term_no);
	if (ret < 0) {
		goto get_edition_failed;
	}
	com_verify(ptrm, &ptrm->term_no, sizeof(ptrm->term_no));
	// ��1�ֽ�0x47
	ret = send_byte(PURSE_GET_TNO, ptrm->term_no);
	if (ret < 0) {
		goto get_edition_failed;
	}
	ptrm->add_verify += PURSE_GET_TNO;
	ptrm->dif_verify ^= PURSE_GET_TNO;
	// ��������
	ret = send_byte(2, ptrm->term_no);
	if (ret < 0)
		goto get_edition_failed;
	ptrm->add_verify += 2;
	ptrm->dif_verify ^= 2;
	// ��2�ֽ�У��
	if (send_verify(ptrm) < 0)
		goto get_edition_failed;
//#if BWT_66J10
//	udelay(BWT_66J10);
//#endif
	ptrm->add_verify = ptrm->dif_verify = 0;
	// ��1�ֽ�term_num
	ret = recv_data((char *)&recvno, ptrm->term_no);
	if (ret < 0) {
		ptrm->status = ret;
		return ret;
	}
	com_verify(ptrm, &recvno, sizeof(recvno));
	if ((recvno & 0x7F) != (ptrm->term_no & 0x7F)) {
		printk("get edition return term no. error %02x: %02x\n", recvno, ptrm->term_no);
		return -1;
	}
	// ��������0x48
	ret = recv_data((char *)&cmd, ptrm->term_no);
	if (ret < 0) {
		ptrm->status = ret;
		return ret;
	}
	com_verify(ptrm, &cmd, sizeof(cmd));
	if (cmd != PURSE_RET_TNO) {
		printk("send bkin return cmd error %02x: %02x\n", cmd, ptrm->term_no);
		return -1;
	}
	// �հ�����
	len = recv_len(ptrm);
	if (len == 0xFF)
		return -1;
	// ��13�ֽ�����, �汾��4�ֽ�, ���������׺�9�ֽ�
	// ��4�ֽں������汾��
	tmp = (unsigned char *)&info.edition;//&ptrm->blkinfo.edition;
	for (i = 0; i < sizeof(info.edition); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d termblk.edition error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// ��9�ֽڽ��׺�
	tmp = (unsigned char *)&info.trade_num.num;
	for (i = 0; i < sizeof(info.trade_num.num); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d ptrm->blkinfo.trade_num.num error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	tmp = info.trade_num.tm;
	for (i = 0; i < sizeof(info.trade_num.tm); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d ptrm->blkinfo.trade_num.tm error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// ��У��2�ֽ�
	if (chk_verify(ptrm) < 0)
		return -1;
	memcpy(&ptrm->blkinfo, &info, sizeof(ptrm->blkinfo));
	return 0;
get_edition_failed:
	ptrm->status = -1;
	return -1;
}

/*
 * ��POS����翨��������һ
 * write by wjzhe, June 13, 2007
 */
int purse_recv_leid(term_ram *ptrm, int allow)
{
	unsigned long cardno;
	acc_ram *pacc;
	unsigned char feature = 0, len = 6;
	int money = 0;
	unsigned char *tmp = (unsigned char *)&cardno;
	int i, ret;
#ifdef PRE_REDUCE
	int pos_money = 0;
	len = 9;
#endif
	//tmp += 3;
	ret = recv_len(ptrm);
	if (ret == 0xFF)
		return -1;
	//ret = send_byte(len, ptrm->term_no);
	//if (ret < 0)
	//	return -1;
	// receive 4-byte card number
	for (i = 0; i < 4; i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
				usart_delay(4 - i);
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
#ifdef PRE_REDUCE
	// ��3�ֽڿ������ѽ��
	tmp = (unsigned char *)&pos_money;
	for (i = 0; i < 4; i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
				usart_delay(4 - i);
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
#endif
	ret = chk_verify(ptrm);
	if (ret < 0) {
		printk("recv leid chk1 error\n");
		return -1;
	}
	// search id
	pacc = search_id(cardno);
#ifdef DEBUG
	printk("search res: pacc: %p, %d\n",
		pacc, (pacc) ? pacc->card_num : cardno);
#endif
	// if exist, then deal with data
	if (pacc == NULL) {
		// not find
#if 0
		printk("search over: %08x not exist\n", cardno);
		printk("0: %08x, 1: %08x, 2: %08x\n", paccmain[0].card_num,
			paccmain[1].card_num, paccmain[2].card_num);
#endif
		feature = 1;
	} else {
#ifdef PRE_REDUCE
		int m_tmp = pacc->feature & 0xF;
		pos_money /= 100;
		m_tmp -= pos_money;
		if (m_tmp < 0)
			m_tmp = 0;
#endif
		//printk("find!\n");
		feature = 4;
		//if (ptrm->pterm->term_type & 0x20) {
		//	feature = 0;
		//} else {
		//	feature = 4;
		//}
		if ((pacc->feature & 0xC0) == 0x80) {
			// ���ǹ�ʧ��
			feature |= 1 << 5;
		}
#ifdef PRE_REDUCE
		if (m_tmp == 0)
			feature |= 1 << 3;
#endif
		if ((pacc->feature & 0xC0) == 0x40 || (pacc->feature & 0xF) == 0
			) {
			// �˿��ﵽ����
			// �˿�Ҫ������
			feature |= 1 << 3;
		}
///////////////////////////////////////////////////////////////////////////////////
		if ( (ptrm->pterm->power_id == 0xF0) ||
			 ((pacc->feature & 0x30) == 0) ||
			 ((ptrm->pterm->power_id & 0x0F) == ((pacc->feature & 0x30) >> 4))
			 ) {
			//money[0] = pacc->money & 0xFF;// ���ն�Ϊ1���ն˻��ʻ�Ϊ1����ݻ��ʻ�������ն����Ͷ�Ӧ��������������
			//money[1] = (pacc->money >> 8) & 0xFF;
			//money[2] = (pacc->money >> 16) & 0xF;
			if (pacc->money < 0) {
				money = 0;
			} else {
				money = (unsigned long)pacc->money;
			}
		} else {
#ifdef DEBUG
			printk("termno %d powerid %02x, card %d feature %02x\n",
				ptrm->term_no, ptrm->pterm->power_id, pacc->card_num, pacc->feature);
#endif
			feature = 1;
		}
#if 0
		if (money > 999999) {
			printk("money is too large!\n");
			feature = 1;
			money = 0;
		}
#endif
///////////////////////////////////////////////////////////////////////////////////
	}
	// �ж��Ƿ������ѻ�ʹ�ù�翨
	if (!allow) {
#ifdef DEBUG
		printk("not allow: %d\n", cardno);
#endif
		feature = 1;
		money = 0;
	}
#if 0
		if (feature == 1) {
			printk("feature: %02x\n", feature);
			feature = 4;
			money = (unsigned long)pacc->money;
		}
#endif
	ptrm->add_verify = 0;
	ptrm->dif_verify = 0;
	// ���ն˺�
	ret = send_byte(ptrm->term_no, ptrm->term_no);
	if (ret < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	ptrm->dif_verify ^= ptrm->term_no;
	ptrm->add_verify += ptrm->term_no;
	// ��������
	ret = send_byte(PURSE_SEND_CARDNO, ptrm->term_no);
	if (ret < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	ptrm->dif_verify ^= PURSE_SEND_CARDNO;
	ptrm->add_verify += PURSE_SEND_CARDNO;
	// ��������
	len = 7;
	ret = send_byte(len, ptrm->term_no);
	if (ret < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	ptrm->dif_verify ^= len;
	ptrm->add_verify += len;
	// �������Ϣ
	if ((ret = send_byte(feature, ptrm->term_no)) < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	ptrm->dif_verify ^= feature;
	ptrm->add_verify += feature;
	//printk("feature is %02x\n", feature);
	//money = binl2bcd(money) & 0xFFFFFF;
	tmp = (unsigned char *)&money;
	//tmp += 2;
	// ��4�ֽ����
	for (i = 0; i < sizeof(money); i++) {
		if (send_byte(*tmp, ptrm->term_no) < 0)
			return -1;
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// ��У��
	if (send_verify(ptrm) < 0)
		return -1;
	ptrm->flow_flag = 0;		// �����ն˽�����ˮ
	// if not exist then send 1 and remain money 0
	// next check dis_verify, send 0 if right, send 0xF if wrong
	return 0;
}

/*
 * receive le card flow
 * but store into flow any data is HEX
 */
int purse_recv_leflow(term_ram *ptrm, unsigned char *tm)
{
	le_flow leflow;
	int ret, i;
	int limit, tail;
	unsigned char *tmp, nouse, len;
	acc_ram *pacc;
	memset(&leflow, 0, sizeof(leflow));
	// �հ�����
	len = recv_len(ptrm);
	if (len == 0xFF)
		return -1;
	// recv 4-byte card number
	tmp = (unsigned char *)&leflow.card_num;
	//tmp += 3;//sizeof(leflow.card_num) - 1;		// tmp point lenum high byte
	for (i = 0; i < 4; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// ��1�ֽڽ�������
	tmp = (unsigned char *)&leflow.tml_num;
	for (i = 0; i < sizeof(leflow.tml_num); i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
	// ��1�ֽ�����
	tmp = (unsigned char *)&leflow.areano;
	for (i = 0; i < sizeof(leflow.areano); i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
#ifndef QINGHUA
#error "Qinghua recv leflow: not allow recv psam_num..."
	// ��2�ֽ�PSAM�����
	tmp = (unsigned char *)&leflow.psam_num;
	for (i = 0; i < 2; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
	// ��4�ֽ�PSAM���������
	tmp = (unsigned char *)&leflow.psam_trd_num;
	for (i = 0; i < 4; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
#endif
	// recv 4-byte consume money
	tmp = (unsigned char *)&leflow.consume_sum;
	//tmp += 1;		//sizeof(leflow.consume_sum) - 1;
	for (i = 0; i < 4; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// recv one no used byte
	ret = recv_data((char *)&nouse, ptrm->term_no);
	if (ret < 0) {
		if (ret == -1) {		// timeout, pos is not exist
			ptrm->status = NOTERMINAL;
		}
		if (ret == -2) {		// other errors
			ptrm->status = NOCOM;
		}
		return ret;
	}
	ptrm->add_verify += nouse;
	ptrm->dif_verify ^= nouse;
	// recv flow type
	ret = recv_data((char *)&leflow.flow_type, ptrm->term_no);
	if (ret < 0) {
		if (ret == -1) {		// timeout, pos is not exist
			ptrm->status = NOTERMINAL;
		}
		if (ret == -2) {		// other errors
			ptrm->status = NOCOM;
		}
		return ret;
	}
	ptrm->add_verify += leflow.flow_type;
	ptrm->dif_verify ^= leflow.flow_type;
	ret = chk_verify(ptrm);
	if (ret < 0) {
		printk("recv le flow verify error\n");
		if (ret == ERRVERIFY) {
			ret = send_ga(ptrm, ret);
		}
		return -1;
	}
	//leflow.consume_sum &= 0xFFFF;
	//if (flag)		// if BCD then convert BIN
	//	leflow.consume_sum = bcdl2bin(leflow.consume_sum);
	// third check NXOR
	//if (verify_all(ptrm, CHKNXOR) < 0)
	//	return -1;
	// check if receive the flow
	if (ptrm->flow_flag) {
		ret = send_ga(ptrm, 0);
		return ret;
	}
	// search id
	pacc = search_id(leflow.card_num);
	if (pacc == NULL) {
		printk("no cardno: %08x", (unsigned int)leflow.card_num);
		ret = send_ga(ptrm, 1);
		return ret;
	}
	// adjust money limit
	limit = pacc->feature & 0xF;
	if ((limit != 0xF) && limit) {
		// ��Ҫ��������
		limit -= leflow.consume_sum / 100 + 1;
		if (!(leflow.consume_sum % 100)) {// �պ���100ʱ, ��Ϊ��һ��
			limit++;
		}
		if (limit < 0)
			limit = 0;
	}
	//if (leflow.consume_sum < 0) {
	//	printk("error!!!!!!!!\n");
	//}
	// �˻����д
	pacc->money -= leflow.consume_sum;
	if (pacc->money < 0) {
		printk("account money below zero!!!\n");
		pacc->money = 0;
	}
	pacc->feature &= 0xF0;
	pacc->feature |= limit;
	// �ն˻����ݼ�¼
	ptrm->term_cnt++;
	ptrm->term_money += leflow.consume_sum;
	// init leflow
	leflow.flow_type |= LECONSUME;
	leflow.date.hyear = *tm++;
	leflow.date.lyear = *tm++;
	leflow.date.mon = *tm++;
	leflow.date.mday = *tm++;
	leflow.date.hour = *tm++;
	leflow.date.min = *tm++;
	leflow.date.sec = *tm;
	//leflow.tml_num = ptrm->term_no;
	leflow.acc_num = pacc->acc_num;
	// ��ˮ�ŵĴ���
	//flow_no = maxflowno;
	leflow.flow_num = maxflowno++;
	//maxflowno = flow_no;
	// set ptrm->flow_flag
	ptrm->flow_flag = ptrm->term_no;
	// ��ˮ��ͷβ�Ĵ���
	tail = flowptr.tail;
	if (tail >= FLOWANUM) {
		// ��ˮ��������?
		printk("uart 485 flow buffer no space\n");
		ret = send_ga(ptrm, -1);
		return -1;
	}
	//printk("recv le flow tail: %d\n", tail);
	memcpy(&pflow[tail], &leflow, sizeof(flow));
	tail++;
#if 0
	if (tail == FLOWANUM)
		tail = 0;
#endif
	flowptr.tail = tail;
	flow_sum++;
	space_remain--;
	ret = send_ga(ptrm, 0);
	return ret;
}

int purse_recv_ledep(term_ram *ptrm, unsigned char *tm)
{
	le_flow leflow;
	int ret, i;
	int tail, managefee;
	unsigned char *tmp, nouse, len;
	acc_ram *pacc;
	memset(&leflow, 0, sizeof(leflow));
	if (!(ptrm->pterm->param.term_type & 0x20)) {	// �϶��Ƿ�Ϊ���ɻ�
		// �ⲻ�ǳ��ɻ�
		//printk("this is not cash terminal\n");
		usart_delay(20);
		return 0;
	}
	// �հ�����
	len = recv_len(ptrm);
	if (len == 0xFF)
		return -1;
	// recv 4-byte card number
	tmp = (unsigned char *)&leflow.card_num;
	//tmp += 3;//sizeof(leflow.card_num) - 1;		// tmp point lenum high byte
	for (i = 0; i < 4; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// ��1�ֽڽ�������
	tmp = (unsigned char *)&leflow.tml_num;
	for (i = 0; i < sizeof(leflow.tml_num); i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
	// ��1�ֽ�����
	tmp = (unsigned char *)&leflow.areano;
	for (i = 0; i < sizeof(leflow.areano); i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
	// ��2�ֽ�PSAM�����
	tmp = (unsigned char *)&leflow.psam_num;
	for (i = 0; i < 2; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
	// ��4�ֽ�PSAM���������
	tmp = (unsigned char *)&leflow.psam_trd_num;
	for (i = 0; i < 4; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
	// recv 4-byte consume money
	tmp = (unsigned char *)&leflow.consume_sum;
	//tmp += 1;		//sizeof(leflow.consume_sum) - 1;
	for (i = 0; i < 4; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// recv one no used byte
	ret = recv_data((char *)&nouse, ptrm->term_no);
	if (ret < 0) {
		if (ret == -1) {		// timeout, pos is not exist
			ptrm->status = NOTERMINAL;
		}
		if (ret == -2) {		// other errors
			ptrm->status = NOCOM;
		}
		return ret;
	}
	ptrm->add_verify += nouse;
	ptrm->dif_verify ^= nouse;
	// recv flow type
	ret = recv_data((char *)&leflow.flow_type, ptrm->term_no);
	if (ret < 0) {
		if (ret == -1) {		// timeout, pos is not exist
			ptrm->status = NOTERMINAL;
		}
		if (ret == -2) {		// other errors
			ptrm->status = NOCOM;
		}
		return ret;
	}
	ptrm->add_verify += leflow.flow_type;
	ptrm->dif_verify ^= leflow.flow_type;
	ret = chk_verify(ptrm);
	if (ret < 0) {
		printk("recv le flow verify error\n");
		if (ret == ERRVERIFY) {
			ret = send_ga(ptrm, ret);
		}
		return -1;
	}
	// check if receive the flow
	if (ptrm->flow_flag) {
		ret = send_ga(ptrm, 0);
		return ret;
	}
	// search id
	pacc = search_id(leflow.card_num);
	if (pacc == NULL) {
		printk("no cardno: %08x", (unsigned int)leflow.card_num);
		ret = send_ga(ptrm, 1);
		return ret;
	}
	// �ָ�����
	if ((pacc->feature & 0xF) != 0xF) {
		pacc->feature &= 0xF0;
		pacc->feature |= 0xE;
	}
	managefee = fee[pacc->managefee & 0xF];		//����ѱ���
	managefee = managefee * leflow.consume_sum / 100;	//��������
	//printk("pacc->money: %d, managefee: %d\n", pacc->money, managefee);
	pacc->money += leflow.consume_sum - managefee;		//����Ǯ���ܷ�
	//printk("pacc->money: %d\n", pacc->money);

	ptrm->term_cnt++;
	ptrm->term_money += leflow.consume_sum;
	// init leflow
	leflow.flow_type |= LECHARGE;
	leflow.date.hyear = *tm++;
	leflow.date.lyear = *tm++;
	leflow.date.mon = *tm++;
	leflow.date.mday = *tm++;
	leflow.date.hour = *tm++;
	leflow.date.min = *tm++;
	leflow.date.sec = *tm;
	leflow.acc_num = pacc->acc_num;
	// ��ˮ�ŵĴ���
	leflow.flow_num = maxflowno++;
	// set ptrm->flow_flag
	ptrm->flow_flag = ptrm->term_no;
	// ��ˮ��ͷβ�Ĵ���
	tail = flowptr.tail;
	if (tail >= FLOWANUM) {
		// ��ˮ��������?
		printk("recv le dep uart 485 flow buffer no space\n");
		ret = send_ga(ptrm, -1);
		return -1;
	}
	//printk("recv le flow tail: %d\n", tail);
	memcpy(&pflow[tail], &leflow, sizeof(flow));
	tail++;
#if 0
	if (tail == FLOWANUM)
		tail = 0;
#endif
	flowptr.tail = tail;
	flow_sum++;
	space_remain--;
	ret = send_ga(ptrm, 0);
	return ret;
}

int purse_recv_letake(term_ram *ptrm, unsigned char *tm)
{
	le_flow leflow;
	int ret, i;
	int tail;
	unsigned char *tmp, nouse, len;
	acc_ram *pacc;
	memset(&leflow, 0, sizeof(leflow));
	if (!(ptrm->pterm->param.term_type & 0x20)) {	// �϶��Ƿ�Ϊ���ɻ�
		// �ⲻ�ǳ��ɻ�
		//printk("this is not cash terminal\n");
		usart_delay(20);
		return 0;
	}
	// �հ�����
	len = recv_len(ptrm);
	if (len == 0xFF)
		return -1;
	// recv 4-byte card number
	tmp = (unsigned char *)&leflow.card_num;
	//tmp += 3;//sizeof(leflow.card_num) - 1;		// tmp point lenum high byte
	for (i = 0; i < 4; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// ��1�ֽڽ�������
	tmp = (unsigned char *)&leflow.tml_num;
	for (i = 0; i < sizeof(leflow.tml_num); i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
	// ��1�ֽ�����
	tmp = (unsigned char *)&leflow.areano;
	for (i = 0; i < sizeof(leflow.areano); i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
	// ��2�ֽ�PSAM�����
	tmp = (unsigned char *)&leflow.psam_num;
	for (i = 0; i < 2; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
	// ��4�ֽ�PSAM���������
	tmp = (unsigned char *)&leflow.psam_trd_num;
	for (i = 0; i < 4; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
	// recv 4-byte consume money
	tmp = (unsigned char *)&leflow.consume_sum;
	//tmp += 1;		//sizeof(leflow.consume_sum) - 1;
	for (i = 0; i < 4; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// recv one no used byte
	ret = recv_data((char *)&nouse, ptrm->term_no);
	if (ret < 0) {
		if (ret == -1) {		// timeout, pos is not exist
			ptrm->status = NOTERMINAL;
		}
		if (ret == -2) {		// other errors
			ptrm->status = NOCOM;
		}
		return ret;
	}
	ptrm->add_verify += nouse;
	ptrm->dif_verify ^= nouse;
	// recv flow type
	ret = recv_data((char *)&leflow.flow_type, ptrm->term_no);
	if (ret < 0) {
		if (ret == -1) {		// timeout, pos is not exist
			ptrm->status = NOTERMINAL;
		}
		if (ret == -2) {		// other errors
			ptrm->status = NOCOM;
		}
		return ret;
	}
	ptrm->add_verify += leflow.flow_type;
	ptrm->dif_verify ^= leflow.flow_type;
	ret = chk_verify(ptrm);
	if (ret < 0) {
		printk("recv le flow verify error\n");
		if (ret == ERRVERIFY) {
			ret = send_ga(ptrm, ret);
		}
		return -1;
	}
	// check if receive the flow
	if (ptrm->flow_flag) {
		ret = send_ga(ptrm, 0);
		return ret;
	}
	// search id
	pacc = search_id(leflow.card_num);
	if (pacc == NULL) {
		printk("no cardno: %08x", (unsigned int)leflow.card_num);
		ret = send_ga(ptrm, 1);
		return ret;
	}
	//printk("pacc->money: %d, managefee: %d\n", pacc->money, managefee);
	pacc->money -= leflow.consume_sum;		//����Ǯ
	if (pacc->money < 0) {
		printk("account money below zero!\n");
		pacc->money = 0;
	}
	//printk("pacc->money: %d\n", pacc->money);

	ptrm->term_cnt++;
	ptrm->term_money -= leflow.consume_sum;
	// init leflow
	leflow.flow_type |= LETAKE;
	leflow.date.hyear = *tm++;
	leflow.date.lyear = *tm++;
	leflow.date.mon = *tm++;
	leflow.date.mday = *tm++;
	leflow.date.hour = *tm++;
	leflow.date.min = *tm++;
	leflow.date.sec = *tm;
	leflow.acc_num = pacc->acc_num;
	// ��ˮ�ŵĴ���
	leflow.flow_num = maxflowno++;
	// set ptrm->flow_flag
	ptrm->flow_flag = ptrm->term_no;
	// ��ˮ��ͷβ�Ĵ���
	tail = flowptr.tail;
	if (tail >= FLOWANUM) {
		// ��ˮ��������?
		printk("recv le dep uart 485 flow buffer no space\n");
		ret = send_ga(ptrm, -1);
		return -1;
	}
	//printk("recv le flow tail: %d\n", tail);
	memcpy(&pflow[tail], &leflow, sizeof(flow));
	tail++;
#if 0
	if (tail == FLOWANUM)
		tail = 0;
#endif
	flowptr.tail = tail;
	flow_sum++;
	space_remain--;
	ret = send_ga(ptrm, 0);
	return ret;
}

/*
 * �洢������ˮ����, ������ˮָ��, ����ָ��, ����ˮ��
 * ���ش�������, ���󷵻�-1, ��ʵ���Ϻ������������
 */
static inline int store_gpflow(le_flow *pleflow, int *money, int flowno)
{
	le_flow *pfw = pleflow;		// Ҫ�������ݵ�ָ��
	le_flow *pinit = pleflow;	// ��ʼ�����ݵ�ָ��
	int i = 0, num = flowno;
	// Ҫһ��һ���Ĵ�����ˮ����
	// ��Ǯ������ʱ���д洢����
	while ((*money) && (i < 5)) {
		// ��Ҫ��������ݽ��г�ʼ��
		if (pinit != pfw) {
			memcpy(pfw, pinit, sizeof(le_flow));
		}
		// ������Ѷ�
		pfw->consume_sum = *money;
		// ��ˮ��
		pfw->flow_num = flowno;
		// group info
		pfw->food_grp_num = i + 1;
		flowno++;
		money++;
		i++;
		pfw++;
	}
	return flowno - num;
}

/*
 * ���շ�����ˮ, ����0����, ��������
 * AppCardID	DWORD	4	HEX	���ۿ�����	
 * PosSetNo		BYTE	1	HEX	����������ñ��	
 * PosSecNo		BYTE	1	HEX	���������������	
 * CsmMny		DWORD	4	HEX	�����ܽ���λΪ��         	
 * GroupAMny	SWORD	3	HEX	A������ѽ���Ϊ��λ��	
 * GroupBMny	SWORD	3	HEX	B������ѽ���Ϊ��λ��	
 * GroupCMny	SWORD	3	HEX	C������ѽ���Ϊ��λ��	
 * GroupDMny	SWORD	3	HEX	D������ѽ���Ϊ��λ��	
 * GroupEMny	SWORD	3	HEX	E������ѽ���Ϊ��λ��	
 * BakReg		BYTE	1	HEX	����չ	0X00
 * FlowType		BYTE	1	HEX	��ˮ���ͱ�ʶ��Bit0������ˮ��Bit1ʧ����ˮ��Bit2�ա�Bit3���ɼ�Ǯ��ˮ��Bit4����ȡǮ��ˮ	
 */
int purse_recv_gpflow(term_ram *ptrm, unsigned char *tm)
{
	le_flow leflow[5];
	int ret, i;
	int limit, tail;
	unsigned char *tmp, nouse, len;
	acc_ram *pacc;
	int money[6] = {0};
	memset(leflow, 0, sizeof(leflow));
	// �հ�����
	len = recv_len(ptrm);
	if (len == 0xFF)	// len != 35 ???
		return -1;
	// recv 4-byte card number
	tmp = (unsigned char *)&leflow[0].card_num;
	//tmp += 3;//sizeof(leflow.card_num) - 1;		// tmp point lenum high byte
	for (i = 0; i < 4; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// ��1�ֽڽ�������
	tmp = (unsigned char *)&leflow[0].tml_num;
	for (i = 0; i < sizeof(leflow[0].tml_num); i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
	// ��1�ֽ�����
	tmp = (unsigned char *)&leflow[0].areano;
	for (i = 0; i < sizeof(leflow[0].areano); i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
#ifndef QINGHUA
#error "Qinghua recv gpflow: not allow recv psam_num..."
	// ��2�ֽ�PSAM�����
	tmp = (unsigned char *)&leflow[0].psam_num;
	for (i = 0; i < 2; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
	// ��4�ֽ�PSAM���������
	tmp = (unsigned char *)&leflow[0].psam_trd_num;
	for (i = 0; i < 4; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
#endif
	// recv 4-byte consume money
	tmp = (unsigned char *)&money[0];
	//tmp += 1;		//sizeof(leflow.consume_sum) - 1;
	for (i = 0; i < 4; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// recv 3 * 5 byte group consume money, recv ABCDE
	for (i = 0; i < 5; i++) {
		int k;
		tmp = (unsigned char *)&money[i + 1];
		//tmp += 1;		//sizeof(leflow.consume_sum) - 1;
		for (k = 0; k < 3; k++) {
			ret = recv_data((char *)tmp, ptrm->term_no);
			if (ret < 0) {
				if (ret == -1) {		// timeout, pos is not exist
					ptrm->status = NOTERMINAL;
				}
				if (ret == -2) {		// other errors
					ptrm->status = NOCOM;
				}
				return ret;
			}
			ptrm->dif_verify ^= *tmp;
			ptrm->add_verify += *tmp;
			tmp++;
		}
	}
	// recv one no used byte
	ret = recv_data((char *)&nouse, ptrm->term_no);
	if (ret < 0) {
		if (ret == -1) {		// timeout, pos is not exist
			ptrm->status = NOTERMINAL;
		}
		if (ret == -2) {		// other errors
			ptrm->status = NOCOM;
		}
		return ret;
	}
	ptrm->add_verify += nouse;
	ptrm->dif_verify ^= nouse;
	// recv flow type
	ret = recv_data((char *)&leflow[0].flow_type, ptrm->term_no);
	if (ret < 0) {
		if (ret == -1) {		// timeout, pos is not exist
			ptrm->status = NOTERMINAL;
		}
		if (ret == -2) {		// other errors
			ptrm->status = NOCOM;
		}
		return ret;
	}
	ptrm->add_verify += leflow[0].flow_type;
	ptrm->dif_verify ^= leflow[0].flow_type;
	ret = chk_verify(ptrm);
	if (ret < 0) {
		printk("recv le flow verify error\n");
		if (ret == ERRVERIFY) {
			ret = send_ga(ptrm, ret);
		}
		return -1;
	}
	//leflow.consume_sum &= 0xFFFF;
	//if (flag)		// if BCD then convert BIN
	//	leflow.consume_sum = bcdl2bin(leflow.consume_sum);
	// third check NXOR
	//if (verify_all(ptrm, CHKNXOR) < 0)
	//	return -1;
	// check if receive the flow
	if (ptrm->flow_flag) {
		ret = send_ga(ptrm, 0);
		return ret;
	}
	// search id
	pacc = search_id(leflow[0].card_num);
	if (pacc == NULL) {
		printk("no cardno: %08x", (unsigned int)leflow[0].card_num);
		ret = send_ga(ptrm, 1);
		return ret;
	}
	// adjust money limit
	limit = pacc->feature & 0xF;
	if ((limit != 0xF) && limit) {
		// ��Ҫ��������
		limit -= money[0] / 100 + 1;
		if (!(money[0] % 100)) {// �պ���100ʱ, ��Ϊ��һ��
			limit++;
		}
		if (limit < 0)
			limit = 0;
	}
	//if (leflow.consume_sum < 0) {
	//	printk("error!!!!!!!!\n");
	//}
	// �˻����д
	pacc->money -= money[0];
	if (pacc->money < 0) {
		printk("account money below zero!!!\n");
		pacc->money = 0;
	}
	pacc->feature &= 0xF0;
	pacc->feature |= limit;
	// �ն˻����ݼ�¼
	ptrm->term_cnt++;
	ptrm->term_money += money[0];
	// init leflow
	leflow[0].flow_type |= LECONSUME;
	leflow[0].date.hyear = *tm++;
	leflow[0].date.lyear = *tm++;
	leflow[0].date.mon = *tm++;
	leflow[0].date.mday = *tm++;
	leflow[0].date.hour = *tm++;
	leflow[0].date.min = *tm++;
	leflow[0].date.sec = *tm;
	//leflow.tml_num = ptrm->term_no;
	leflow[0].acc_num = pacc->acc_num;
	// ��ˮ�ŵĴ���
	//flow_no = maxflowno;
	i = store_gpflow(leflow, &money[1], maxflowno);
	// һ��������i������, ������Ҫ�ٴ�����ˮ��֮�����Ϣ
	maxflowno += i;
	//leflow.flow_num = maxflowno++;
	//maxflowno = flow_no;
	// set ptrm->flow_flag
	ptrm->flow_flag = ptrm->term_no;
	// ��ˮ��ͷβ�Ĵ���
	tail = flowptr.tail;
	if (tail >= FLOWANUM) {
		// ��ˮ��������?
		printk("uart 485 flow buffer no space\n");
		ret = send_ga(ptrm, -1);
		return -1;
	}
	//printk("recv le flow tail: %d\n", tail);
	memcpy(&pflow[tail], &leflow[0], sizeof(flow) * i);
	tail += i;
#if 0
	if (tail == FLOWANUM)
		tail = 0;
#endif
	flowptr.tail = tail;
	flow_sum += i;
	space_remain -= i;// �˴���ʣ����ˮ����ʱ���д���, BUG!!!
	ret = send_ga(ptrm, 0);
	return ret;
}

#if 0
/*
 * ����PSAM����
 */
psam_local *search_psam(unsigned short num, struct psam_info *psinfo)
{
	psam_local *psl;
	int cnt, i;
	// �򵥵Ľ��в�����֤
	if (psinfo == NULL)
		return NULL;
	cnt = psinfo->cnt;
	psl = psinfo->psam;
	if (cnt == 0 || psl == NULL)
		return NULL;
	for (i = 0; i < cnt; i++) {
		if (num == psl[i].psamno) {
			// find it, return
			return &psl[i];
		}
	}
	return NULL;
}
/*
 * ��Ƭ����Ӧ�ù������ü���������
 * write by wjzhe, June 11 2007
 */
int purse_startup_newfunc(term_ram *ptrm)
{
	// ��1�ֽڵ�ַ
	// ��0x34������
	/*
	A:������ʱ�䣨�ʹΣ����ã�
	B:�����Ѵ������ƣ�
	C:��������ˮ�Ƿ�洢
	...
	*/
	return 0;
}

/*
 * ָ����ַ���ڴ洢�ĺ���������
 * write by wjzhe, June 11 2007
 */
int purse_update_data(term_ram *ptrm, int addr, void *data, int n)
{
	return 0;
}

/*
 * ָ��ʱ�����ˮ��δ�ϴ���ˮ�ϴ�
 * write by wjzhe, June 11 2007
 */
int purse_get_flow(term_ram *ptrm, unsigned char *tm_s, unsigned char *tm_e)
{
	return 0;
}
#endif

