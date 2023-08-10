#include "board.h"

void uartInit(uint32_t uart_rate)	
{
  P6SEL |= BIT6 + BIT7;      // P5.6,7 = USCI_A1 TXD/RXD
  //UART_PxDIR |= UART_TX_PIN;                  // Set P5.6 to output direction

  UCA1CTL1 |= UCSWRST;                         // **Put state machine in reset**
  UCA1CTL0 &= ~UCPEN;               	       //8位字符，无奇、偶校验，1位停止位，UART MODE
  UCA1CTL1 |= UCSSEL_2;                        // SMCLK
  UCA1BR0 = (SYSTICK_FREQ / uart_rate) % 256;        // rate=SYSCLK/(UCA0BR0+UCA0BR1*256)
  UCA1BR1 = (SYSTICK_FREQ / uart_rate) / 256;

  UCA1MCTL = UCBRS_1 + UCBRF_0;                 // Modulation UCBRSx=1, UCBRFx=0
  UCA1CTL1 &= ~UCSWRST;                        // **Initialize USCI state machine**
  UCA1IE |= UCRXIE;                             // Enable USCI_A0 RX interrupt
}