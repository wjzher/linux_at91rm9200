#ifndef _POWER_CAL_H
#define _POWER_CAL_H
#ifdef CONFIG_TPRT_NC
typedef unsigned char pwn_t;
typedef struct pwcont_s {
	unsigned char low;		// ���ն˺�
	unsigned char high;		// ���ն˺�
} pwcont;

// ȫ��ʹ�ô�����
typedef struct power_ctrl_knl_s {
	unsigned char is_set;	// �˵�Դ�����Ƿ�����
	unsigned char num;		// �ն˺�
	unsigned char base;		// ͨ������
	unsigned char cival;	// �̵�������ʱ����
	unsigned int tflag;		// �ն˴��ڱ�ʶ
	unsigned short s_ctrl;	// ���͸��ն˵�״̬
	unsigned short r_ctrl;	// �ն˿�����Ϣ
	short flag;				// ͨѶ״̬
} power_ctrl_knl;
// ȫ��ʹ�ô�����
typedef struct power_ctrl_s {
	unsigned char is_set;	// �˵�Դ�����Ƿ�����
	unsigned char num;		// �ն˺�
	unsigned char base;		// ͨ������
	unsigned char cival;	// �̵�������ʱ����
	unsigned int tflag;		// �ն˴��ڱ�ʶ
	unsigned short s_ctrl;	// ��Ҫ���͵�����
	int flag;				// ��־�Ƿ���Ҫ����
} power_ctrl;
static const pwn_t cpower_num[] = {15, 31, 47, 63, 79, 95, 111, 127,
	143, 159, 175, 191, 207, 223, 239, 254};

static const pwcont cpower_cont[] = {
	{1, 14},
	{16, 30},
	{32, 46},
	{48, 62},
	{64, 78},
	{80, 94},
	{96, 110},
	{112, 126},
	{129, 142},
	{144, 158},
	{160, 174},
	{176, 190},
	{192, 206},
	{208, 222},
	{224, 238},
	{240, 253}
};
#define PCTRLN (sizeof(cpower_num))

extern power_ctrl_knl pwctrl[PCTRLN];

static inline int num_in_cont(pwn_t n, const pwcont *c)
{
	return (n >= c->low && n <= c->high);
}

// ���ҵ�Դ����ָ��, ����const���ָ��
static inline const pwn_t *find_powernum(pwn_t num)
{
	int i;
	for (i = 0; i < PCTRLN; i++) {
		if (num == cpower_num[i]) {
			return &cpower_num[i];
		}
	}
	return NULL;
}
static inline int find_powernum2(pwn_t num)
{
	int i;
	for (i = 0; i < PCTRLN; i++) {
		if (num == cpower_num[i]) {
			return i;
		}
	}
	return -1;
}

static inline int is_power_num(pwn_t num)
{
	return (find_powernum(num) ? 1 : 0);
}

// �����ն����ڵ�Դ����λ��, ����λ�ñ��
static inline int find_power_ctrl(pwn_t num)
{
	int i;
	for (i = 0; i < sizeof(cpower_cont) / sizeof(*cpower_cont); i++) {
		if (num_in_cont(num, &cpower_cont[i])) {
			return i;
		}
	}
	return -1;
}
#endif
#endif
