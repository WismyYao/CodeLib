#pragma once

#ifndef _TIMERS_DRIVER_C_
#define _TIMERS_DRIVER_C_

#include "board.h"

volatile tmr10ms_t Timer1;
volatile tmr10ms_t Timer2;

// Start TIMER A0 at 100Hz
void init10msTimer(void)
{
  TA0CTL = MC_0;                            // 关闭TA0定时器
  TA0CCTL0 = CCIE;                          // TA中断使能
  TA0CCR0 = SYSTICK_FREQ / 8 / 100 - 1;     // 采样率
  TA0CTL = TASSEL_2 + ID_3 + MC_1 + TACLR;  // 设置TA0的时钟源为SMCLK,时钟分频系数=/8,增计数模式
}

// 中断函数
#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer0_A0_ISR (void)
{
  g_tmr10ms++;

  uint32_t n = Timer1;
  if (n) Timer1 = --n;
  n = Timer2;
  if (n) Timer2 = --n;

  // LPM1_EXIT;									    //中断结束后允许退出低功耗模式,执行主程序
}

#endif // !_TIMERS_DRIVER_C_