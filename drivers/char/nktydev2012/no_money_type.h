#ifndef _NO_MONEY_TYPE_
#define _NO_MONEY_TYPE_
// 1�����ʧ��
// 2�����ң�
// 3������͵�ͣ�
// 4�����͵�ͣ�
// 5��������
// 6�����˻�ע����
// 7������ע�᣻
// 8���������

#define NOM_LOSS_CARD 1
#define NOM_FIND_CARD 2
#define NOM_SET_THIEVE 3
#define NOM_CLR_THIEVE 4
#define NOM_CHANGED_CARD 5
#define NOM_LOGOUT_CARD 6
#define NOM_CURRENT_RGE 7
#define NOM_BLACK 8
#define NOM_CHGPASSWD 9
#define NOM_FREEZE_CARD 10//write by duyy, 2013.5.6
#define NOM_UNFREEZE_CARD 11//write by duyy, 2013.5.6
#define NOM_SUBFREEZE_CARD 12//write by duyy, 2013.5.6
#define NOM_SUBUNFREEZE_CARD 13//write by duyy, 2013.5.6

typedef unsigned char BYTE;

#ifndef CONFIG_UART_V2
typedef struct {
	int				OperAreaID;					//�������������
	unsigned long	OperationID;				//��������¼��
	unsigned long	CardId;					//������Ӧ����--OldCardId
	int			BLTypeId;					//��������������
	unsigned long	NewCardId;					//�����Ŀ���
	int				Type;	//1�����ʧ��2�����ң�3������͵�ͣ�4�����͵�ͣ�5��������6�����˻�ע����7������ע�᣻8���������; 9�����޸��ʻ�����; 10�������ʻ�����; 11�������ʻ��ⶳ; 12�������ʻ�����; 13�������ʻ��ⶳ
	long			AccountId;					//����ע����˺�
	int				RemainMoney;				//����ע���˻����
	int				Flag;						////����ע���˻�״̬
	int				UpLimitMoney;				//����ע���˻���������
	int				ManageId;					//����ע���˻������ϵ��
	int				PowerId;					//����ע���˻��������
	char        FlowTime[20];					//��������ˮ�ṹ
} no_money_flow;

typedef struct __money_flow {
	long  AccountId; //�˻�����
	unsigned long CardNo; 
	int   Money; //���仯������λΪ�� 
} money_flow;
#endif

#endif

