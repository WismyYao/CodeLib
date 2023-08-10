#pragma once

#ifndef _TIMERS_DRIVER_C_
#define _TIMERS_DRIVER_C_

#include "board.h"

volatile tmr10ms_t Timer1;
volatile tmr10ms_t Timer2;

// Start TIMER A0 at 100Hz
void init10msTimer(void)
{
  TA0CTL = MC_0;                            // �ر�TA0��ʱ��
  TA0CCTL0 = CCIE;                          // TA�ж�ʹ��
  TA0CCR0 = SYSTICK_FREQ / 8 / 100 - 1;     // ������
  TA0CTL = TASSEL_2 + ID_3 + MC_1 + TACLR;  // ����TA0��ʱ��ԴΪSMCLK,ʱ�ӷ�Ƶϵ��=/8,������ģʽ
}

// �жϺ���
#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer0_A0_ISR (void)
{
  g_tmr10ms++;

  uint32_t n = Timer1;
  if (n) Timer1 = --n;
  n = Timer2;
  if (n) Timer2 = --n;

  // LPM1_EXIT;									    //�жϽ����������˳��͹���ģʽ,ִ��������
}

#endif // !_TIMERS_DRIVER_C_