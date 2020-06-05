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

typedef unsigned char BYTE;
typedef struct {
	int				OperAreaID;					//�������������
	unsigned long	OperationID;				//��������¼��
	unsigned long	CardId;					//������Ӧ����--OldCardId
	int			BLTypeId;					//��������������
	unsigned long	NewCardId;					//�����Ŀ���
	int				Type;	//1�����ʧ��2�����ң�3������͵�ͣ�4�����͵�ͣ�5��������6�����˻�ע����7������ע�᣻8���������
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

#if 0
typedef struct {
	unsigned long	FlowId;		//���ֽ���ˮ��
	int				Type;		//1�����ʧ��2�����ң�3������͵�ͣ�4�����͵�ͣ�5��������6�����˻�ע����7������ע�᣻8���������
	unsigned long	BlackFlowId;//��������¼��
	unsigned long	CardId;		//������Ӧ����
	int				BLTypeId;	//��������������
	char			FlowTime[20];	//����������ʱ��
	unsigned long	NewCardId;		//�����Ŀ���
	//����ע����Ϣ���Զν�������������ʻ�
	long			AccountId;		//����ע����˺�
	int				RemainMoney;	//����ע���˻����
	BYTE			Flag;			//����ע���˻�״̬
	BYTE			UpLimitMoney;	//����ע���˻���������
	BYTE			ManageId;		//����ע���˻������ϵ��
	BYTE			PowerId;		//����ע���˻��������
} no_money_flow;

typedef struct __money_flow {
	//long  AccountId; //�˻�����
	unsigned long CardNo; 
	int   Money; //���仯������λΪ�� 
} money_flow;
#endif
#endif
