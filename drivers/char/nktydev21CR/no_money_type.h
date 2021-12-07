#ifndef _NO_MONEY_TYPE_
#define _NO_MONEY_TYPE_
// 1代表挂失；
// 2代表解挂；
// 3代表设偷餐；
// 4代表解偷餐；
// 5代表换卡；
// 6代表账户注销；
// 7代表当餐注册；
// 8代表黑名单

#define NOM_LOSS_CARD 1
#define NOM_FIND_CARD 2
#define NOM_SET_THIEVE 3
#define NOM_CLR_THIEVE 4
#define NOM_CHANGED_CARD 5
#define NOM_LOGOUT_CARD 6
#define NOM_CURRENT_RGE 7
#define NOM_BLACK 8
#define NOM_MANAGEFEETABLE 9 //write by duyy, 2013.11.18
#define NOM_FORBIDTABLE 10//write by duyy, 2013.11.18
#define NOM_FORBIDSEG 11//write by duyy, 2013.11.18

typedef unsigned char BYTE;
typedef struct {
	unsigned long	FlowId;		//非现金流水号
	int				Type;		//1代表挂失；2代表解挂；3代表设偷餐；4代表解偷餐；5代表换卡；6代表账户注销；7代表当餐注册；8代表黑名单
								//9代表管理费表更新；10代表禁止消费时段匹配表更新；11代表禁止消费时段库更新//modified by duyy, 2013.10.24
	unsigned long	BlackFlowId;//黑名单纪录号
	unsigned long	CardId;		//操作对应卡号
	int				BLTypeId;	//黑名单操作类型
	char			FlowTime[20];	//黑名单操作时间
	unsigned long	NewCardId;		//换卡的卡号
	//当餐注册信息各自段解释详见白名单帐户
	long			AccountId;		//当餐注册的账号
									//当Type值为9，10，11时这个变量表示更新版本号，每一个表更新时都由此变量表示对应的版本号。modified by duyy, 2013.10.24
	int				RemainMoney;	//当餐注册账户余额
	BYTE			Flag;			//当餐注册账户状态
	BYTE			ManageId;		//当餐注册账户管理费系数
	BYTE			UpLimitMoney;	//当餐注册账户消费上限
	BYTE			PowerId;		//当餐注册账户身份类型
} no_money_flow;

typedef struct __money_flow {
	//long  AccountId; //账户卡号
	unsigned long CardNo; 
	int   Money; //余额变化量，单位为分 
} money_flow;

#if 0
typedef struct {
	unsigned long	FlowId;		//非现金流水号
	int				Type;		//1代表挂失；2代表解挂；3代表设偷餐；4代表解偷餐；5代表换卡；6代表账户注销；7代表当餐注册；8代表黑名单
	unsigned long	BlackFlowId;//黑名单纪录号
	unsigned long	CardId;		//操作对应卡号
	int				BLTypeId;	//黑名单操作类型
	char			FlowTime[20];	//黑名单操作时间
	unsigned long	NewCardId;		//换卡的卡号
	//当餐注册信息各自段解释详见白名单帐户
	long			AccountId;		//当餐注册的账号
	int				RemainMoney;	//当餐注册账户余额
	BYTE			Flag;			//当餐注册账户状态
	BYTE			UpLimitMoney;	//当餐注册账户消费上限
	BYTE			ManageId;		//当餐注册账户管理费系数
	BYTE			PowerId;		//当餐注册账户身份类型
} no_money_flow;

typedef struct __money_flow {
	//long  AccountId; //账户卡号
	unsigned long CardNo; 
	int   Money; //余额变化量，单位为分 
} money_flow;
#endif
#endif
