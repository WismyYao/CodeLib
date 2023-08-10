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
  P5SEL |= BIT2 + BIT3;       // 复用端口配置为XT2模式
  UCSCTL6 &= ~XT2OFF;         // 使能XT2
  UCSCTL3 = SELREF_2;         // MCU上电默认的FLL时钟源为XT1；设置FLL的时钟源为REFO,设置为REFO后LFXT1可以不在使用
  UCSCTL6 |= XT1OFF;          // 关闭XT1
  UCSCTL4 |= SELA_2;          // 设置ACLK的时钟源为REFO
  __bis_SR_register(SCG0);    // 设置DCO,debug
  UCSCTL0 = 0x0000;
  UCSCTL1 = DCORSEL_5;
  UCSCTL2 = FLLD_1 + 239;     // 设置为7.8M  (1+239)*32768
  __bic_SR_register(SCG0);
  __delay_cycles(250000);
  do                          // 循环直至XT1,XT2 & DCO稳定
  {	
    UCSCTL7 &= ~(XT2OFFG + XT1LFOFFG + XT1HFOFFG + DCOFFG);
    SFRIFG1 &= ~OFIFG;        // Clear fault flags
  }
  while (SFRIFG1 & OFIFG);    // Test oscillator fault flag
  UCSCTL6 &= ~XT2DRIVE1;      // XT2的晶振频率在4M~8M范围内
  UCSCTL4 = SELA_4 + SELS_5 + SELM_4; // 设置XT2为SMCLK的时钟源,DCOCLKDIV为ACLK和MCLK
  UCSCTL5 = DIVA_1 + DIVS_0 + DIVM_0; // 设置SMCLK为XT2的全频，MCLK的频率为XT2的全频
}

void SPI_Init(void)
{
  P1SEL &= ~BIT0;                                 //SD POWER
  P1DIR |= BIT0;

  UCA0CTL1 |= UCSWRST;                            //USCIA0处于复位状态
  UCA0CTL0 = UCCKPL + UCMST + UCSYNC + UCMSB;     //UCMST:选择主模式；UCSYNC:同步方式；UCCKPL=0:空闲时为低电平；UCMSB:高位先传递补；3线，8位，工作模式
  UCA0CTL1 |= UCSSEL_2;                           //选择ACLK作为SPI的时钟源
  UCA0BR0 = 0x02;                                 //频率为SMCLK的1/2
  UCA0BR1 = 0;
  UCA0MCTL = 0;                                   //不需要调制
  P3SEL |= BIT5 + BIT4 + BIT0;                    //使用P3.5,P3.4,P3.0的特殊功能，设置为SPI接口
  P3DIR |= BIT4 + BIT0;                           //设置输出
  UCA0CTL1 &= ~UCSWRST;                           //USCIA0激活，处于工作状态
  //UCA0IE |= UCRXIE;                         				//使能接收中断

  P3SEL &= ~BIT2;                                 //SPI从控制口设置为输出状态，SD卡片选线
  P3DIR |= BIT2;
}
