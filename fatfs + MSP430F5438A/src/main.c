#include <string.h>
#include "MSP430F5438A/board.h"
#include "thirdparty/FatFs/ff.h"

/***********************************************************************/
/* main */
/***********************************************************************/
static FIL datafile = {0};
static uint8_t buf[64] = "this is a test file ...\n";

void main( void )
{
  UINT br = sizeof(buf);
  UINT bw = sizeof(buf);

  WDTCTL = WDTPW + WDTHOLD;   // stop watchdog timer

  boardInit();
  
  if(!sdMount()) {
    while(1);
  }

  FRESULT res = f_open(&datafile, "0:data.txt", FA_CREATE_ALWAYS | FA_WRITE);
  if (res != FR_OK) {
    f_close(&datafile);
    while(1);
  }

  res = f_write(&datafile, buf, br, &bw);
  f_close(&datafile);
  while(1);
}


