#include "board.h"

void boardInit(void)
{
  clockInit();
  init10msTimer();
  // uartInit(115200);
  SPI_Init();
  _BIS_SR(GIE);
}

void clockInit(void)
{
  P5SEL |= BIT2 + BIT3;       // ���ö˿�����ΪXT2ģʽ
  UCSCTL6 &= ~XT2OFF;         // ʹ��XT2
  UCSCTL3 = SELREF_2;         // MCU�ϵ�Ĭ�ϵ�FLLʱ��ԴΪXT1������FLL��ʱ��ԴΪREFO,����ΪREFO��LFXT1���Բ���ʹ��
  UCSCTL6 |= XT1OFF;          // �ر�XT1
  UCSCTL4 |= SELA_2;          // ����ACLK��ʱ��ԴΪREFO
  __bis_SR_register(SCG0);    // ����DCO,debug
  UCSCTL0 = 0x0000;
  UCSCTL1 = DCORSEL_5;
  UCSCTL2 = FLLD_1 + 239;     // ����Ϊ7.8M  (1+239)*32768
  __bic_SR_register(SCG0);
  __delay_cycles(250000);
  do                          // ѭ��ֱ��XT1,XT2 & DCO�ȶ�
  {	
    UCSCTL7 &= ~(XT2OFFG + XT1LFOFFG + XT1HFOFFG + DCOFFG);
    SFRIFG1 &= ~OFIFG;        // Clear fault flags
  }
  while (SFRIFG1 & OFIFG);    // Test oscillator fault flag
  UCSCTL6 &= ~XT2DRIVE1;      // XT2�ľ���Ƶ����4M~8M��Χ��
  UCSCTL4 = SELA_4 + SELS_5 + SELM_4; // ����XT2ΪSMCLK��ʱ��Դ,DCOCLKDIVΪACLK��MCLK
  UCSCTL5 = DIVA_1 + DIVS_0 + DIVM_0; // ����SMCLKΪXT2��ȫƵ��MCLK��Ƶ��ΪXT2��ȫƵ
}

void SPI_Init(void)
{
  P1SEL &= ~BIT0;                                 //SD POWER
  P1DIR |= BIT0;

  UCA0CTL1 |= UCSWRST;                            //USCIA0���ڸ�λ״̬
  UCA0CTL0 = UCCKPL + UCMST + UCSYNC + UCMSB;     //UCMST:ѡ����ģʽ��UCSYNC:ͬ����ʽ��UCCKPL=0:����ʱΪ�͵�ƽ��UCMSB:��λ�ȴ��ݲ���3�ߣ�8λ������ģʽ
  UCA0CTL1 |= UCSSEL_2;                           //ѡ��ACLK��ΪSPI��ʱ��Դ
  UCA0BR0 = 0x02;                                 //Ƶ��ΪSMCLK��1/2
  UCA0BR1 = 0;
  UCA0MCTL = 0;                                   //����Ҫ����
  P3SEL |= BIT5 + BIT4 + BIT0;                    //ʹ��P3.5,P3.4,P3.0�����⹦�ܣ�����ΪSPI�ӿ�
  P3DIR |= BIT4 + BIT0;                           //�������
  UCA0CTL1 &= ~UCSWRST;                           //USCIA0������ڹ���״̬
  //UCA0IE |= UCRXIE;                         				//ʹ�ܽ����ж�

  P3SEL &= ~BIT2;                                 //SPI�ӿ��ƿ�����Ϊ���״̬��SD��Ƭѡ��
  P3DIR |= BIT2;
}
