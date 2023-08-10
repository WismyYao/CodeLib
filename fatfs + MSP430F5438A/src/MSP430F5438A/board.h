#pragma once

#ifndef _BOARD_H_
#define _BOARD_H_

#include "msp430x54x.h"
#include "../globals.h"

#define SYSTICK_FREQ    (1105920L)  // (7372800L)
#define delay_ms(x)     __delay_cycles((unsigned long)(SYSTICK_FREQ*(double)x/1000.0))

void boardInit(void);

void clockInit(void);     /* initialize system clock */

void init10msTimer(void); /* Start TIMER A0 at 100Hz */

void SPI_Init(void);

// SD driver
#define BLOCK_SIZE 512
bool_t sdMount(void);   /* initialize SD card */
bool_t sdMounted();
bool_t sdIsHC();      /* is the type of SD card SDHC */
void sdUnmount(); /* unmount SD card */ 
bool_t sdFormat();  /* format SD card */ 

// uart
void uartInit(uint32_t uart_rate);
void traceString(const uint8_t * string);
void traceNumber(uint32_t number);


#endif // !_BOARD_H_
