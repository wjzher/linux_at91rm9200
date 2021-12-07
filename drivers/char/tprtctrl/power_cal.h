#ifndef _POWER_CAL_H
#define _POWER_CAL_H
#ifdef CONFIG_TPRT_NC
typedef unsigned char pwn_t;
typedef struct pwcont_s {
	unsigned char low;		// 低终端号
	unsigned char high;		// 高终端号
} pwcont;

// 全局使用此数据
typedef struct power_ctrl_knl_s {
	unsigned char is_set;	// 此电源控制是否启用
	unsigned char num;		// 终端号
	unsigned char base;		// 通道基数
	unsigned char cival;	// 继电器控制时间间隔
	unsigned int tflag;		// 终端存在标识
	unsigned short s_ctrl;	// 发送给终端的状态
	unsigned short r_ctrl;	// 终端控制信息
	short flag;				// 通讯状态
} power_ctrl_knl;
// 全局使用此数据
typedef struct power_ctrl_s {
	unsigned char is_set;	// 此电源控制是否启用
	unsigned char num;		// 终端号
	unsigned char base;		// 通道基数
	unsigned char cival;	// 继电器控制时间间隔
	unsigned int tflag;		// 终端存在标识
	unsigned short s_ctrl;	// 需要发送的数据
	int flag;				// 标志是否需要更新
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

// 查找电源控制指针, 返回const编号指针
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

// 查找终端所在电源控制位置, 返回位置编号
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
