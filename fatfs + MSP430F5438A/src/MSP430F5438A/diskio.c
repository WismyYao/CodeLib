/*
 * Copyright (C) OpenTX
 *
 * Based on code named
 *   th9x - http://code.google.com/p/th9x 
 *   er9x - http://code.google.com/p/er9x
 *   gruvin9x - http://code.google.com/p/gruvin9x
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "board.h"
#include "../thirdparty/FatFs/diskio.h"
#include "../thirdparty/FatFs/ff.h"
#include "../globals.h"

/* Definitions for MMC/SDC command */
#define CMD0    (0x40+0)        /* GO_IDLE_STATE */
#define CMD1    (0x40+1)        /* SEND_OP_COND (MMC) */
#define ACMD41  (0xC0+41)       /* SEND_OP_COND (SDC) */
#define CMD8    (0x40+8)        /* SEND_IF_COND */
#define CMD9    (0x40+9)        /* SEND_CSD */
#define CMD10   (0x40+10)       /* SEND_CID */
#define CMD12   (0x40+12)       /* STOP_TRANSMISSION */
#define ACMD13  (0xC0+13)       /* SD_STATUS (SDC) */
#define CMD16   (0x40+16)       /* SET_BLOCKLEN */
#define CMD17   (0x40+17)       /* READ_SINGLE_BLOCK */
#define CMD18   (0x40+18)       /* READ_MULTIPLE_BLOCK */
#define CMD23   (0x40+23)       /* SET_BLOCK_COUNT (MMC) */
#define ACMD23  (0xC0+23)       /* SET_WR_BLK_ERASE_COUNT (SDC) */
#define CMD24   (0x40+24)       /* WRITE_BLOCK */
#define CMD25   (0x40+25)       /* WRITE_MULTIPLE_BLOCK */
#define CMD55   (0x40+55)       /* APP_CMD */
#define CMD58   (0x40+58)       /* READ_OCR */

/* Card type flags (CardType) */
#define CT_MMC              0x01
#define CT_SD1              0x02
#define CT_SD2              0x04
#define CT_SDC              (CT_SD1|CT_SD2)
#define CT_BLOCK            0x08

#define DATA_RESPONSE_TIMEOUT   20   

static volatile DSTATUS Stat = STA_NOINIT;      /* Disk status */

BYTE CardType;                  /* Card type flags */

/*----------------------------------------------------------------*/
/* SPI driver                                                     */
/*----------------------------------------------------------------*/
#define DUMMY_CHAR 0xFF

#define SD_SELECT()         P3OUT &= ~BIT2     // Card Select, pull LOW
#define SD_DESELECT()       P3OUT |= BIT2      // Card Deselect, pull HIGH

#define SD_POWER_ON()       P1OUT &= (~BIT0)   // 低电平打开SD卡电源
#define SD_POWER_OFF()      P1OUT |= BIT0      // 高电平关闭SD卡电源

//Send one byte via SPI
int8_t spi_send_byte(const int8_t data)
{
  while ((UCA0IFG & UCTXIFG) ==0);    // wait while not ready for TX
  UCA0TXBUF = data;                   // write
  while ((UCA0IFG & UCRXIFG) ==0);    // wait for RX buffer (full)
  return (UCA0RXBUF);
}

//Receiver one byte via SPI
int8_t spi_receiver_byte()
{
  return spi_send_byte(DUMMY_CHAR);
}

// Alternative macro to receive data fast
#define rcvr_spi_m(dst)  *(dst)=spi_receiver_byte()

/*-----------------------------------------------------------------------*/
/* Wait for card ready                                                   */
/*-----------------------------------------------------------------------*/
static BYTE wait_ready (void)
{
  BYTE res;

  Timer2 = 100;    /* Wait for ready in timeout of 1s */
  spi_receiver_byte();
  do {
    res = spi_receiver_byte();
  } while ((res != 0xFF) && Timer2);

  return res;
}

static void spi_reset()
{
  for (int n = 0; n < 520; ++n) {
    spi_send_byte(0xFF);
  }
}

/*-----------------------------------------------------------------------*/
/* Deselect the card and release SPI bus                                 */
/*-----------------------------------------------------------------------*/
static void release_spi (void)
{
  SD_DESELECT();
  spi_receiver_byte();
}

/*-----------------------------------------------------------------------*/
/* Receive a data packet from SD                                        */
/*-----------------------------------------------------------------------*/
static bool_t sdReceiverBlock (
        BYTE *buff,                     /* Data buffer to store received data */
        UINT btr                        /* Byte count (must be multiple of 4) */
)
{
  BYTE token;

  Timer1 = DATA_RESPONSE_TIMEOUT;
  do {                                                    /* Wait for data packet in timeout of 200ms */
    token = spi_receiver_byte();
  } while ((token == 0xFF) && Timer1);

  if(token != 0xFE) {
    spi_reset();
    return false; /* If not valid data token, return with error */
  }

  do {                                                    /* Receive the data block into buffer */
    rcvr_spi_m(buff++);
    rcvr_spi_m(buff++);
    rcvr_spi_m(buff++);
    rcvr_spi_m(buff++);
  } while (btr -= 4);

  spi_receiver_byte();                                             /* Discard CRC */
  spi_receiver_byte();

  return true;                                    /* Return with success */
}

/*-----------------------------------------------------------------------*/
/* Send a data packet to SD                                             */
/*-----------------------------------------------------------------------*/

static bool_t sdTransmitBlock (
        const BYTE *buff,       /* 512 byte data block to be transmitted */
        BYTE token                      /* Data/Stop token */
)
{
  BYTE resp;
  BYTE wc;
  
  SD_SELECT();

  if (wait_ready() != 0xFF) {
    spi_reset();
    return false;
  }

  spi_send_byte(token);                                        /* transmit data token */
  if (token != 0xFD) {    /* Is data token */
    wc = 0;
    do {                                                    /* transmit the 512 byte data block to MMC */
      spi_send_byte(*buff++);
      spi_send_byte(*buff++);
    } while (--wc);

    spi_send_byte(0xFF);                                 /* CRC (Dummy) */
    spi_send_byte(0xFF);

    /*
    Despite what the SD card standard says, the reality is that (at least for some SD cards)
    the Data Response byte does not come immediately after the last byte of data.

    This delay only happens very rarely, but it does happen. Typical response delay is some 10ms
    */
    Timer2 = DATA_RESPONSE_TIMEOUT;   
    do {
      resp = spi_receiver_byte();            /* Receive data response */
      if ((resp & 0x1F) == 0x05) {
        return true;
      }
      if (resp != 0xFF) {
        spi_reset();
        return false;
      }
    } while (Timer2);
    return false;
  }
  return true;
}


/*-----------------------------------------------------------------------*/
/* Send a command packet to SD                                           */
/*-----------------------------------------------------------------------*/
static BYTE sdSendCmd (
        BYTE cmd,               /* Command byte */
        DWORD arg               /* Argument */
)
{
  char crc, res;

  if (cmd & 0x80) {   // ACMD<n> is the command sequence of CMD55-CMD<n>
    cmd &= 0x7F;
    res = sdSendCmd(CMD55, 0);
    if (res > 1) return res;
  }

  // Select the card and wait for ready
  SD_SELECT();
  if (wait_ready() != 0xFF) {
    spi_reset();
    return 0xFF;
  }

  // Send command packet
  spi_send_byte(cmd);                 // Start + Command index
  spi_send_byte((char)(arg >> 24));   // Argument[31...24]
  spi_send_byte((char)(arg >> 16));   // Argument[23...16]
  spi_send_byte((char)(arg >> 8));    // Argument[15...8]
  spi_send_byte((char)arg);           // Argument[7...0]
  crc = 0xFF;                       // Dummy CRC + Stop
  if (cmd == CMD0) crc = 0x95;      // Valid CRC for CMD0(0)
  if (cmd == CMD8) crc = 0x87;      // Valid CRC for CMD8(0x1AA)
  spi_send_byte(crc);

  if (cmd == CMD12) spi_receiver_byte();    // Skip a stuff byte when stop reading

  unsigned char n = 20;    //  Wait for a valid response in timeout of 20 attempts
  do {
    res = spi_receiver_byte();
  } while (((res & 0x80) != 0) && (--n));
  return res;
}



/*--------------------------------------------------------------------------

   Public Functions

---------------------------------------------------------------------------*/


/*-----------------------------------------------------------------------*/
/* Initialize Disk Drive                                                 */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (BYTE drv)                /* Physical drive number (0) */
{
  BYTE n, cmd, ty, ocr[4];

  if (drv) return STA_NOINIT;                     /* Supports only single drive */
  if (Stat & STA_NODISK) return Stat;     /* No card in the socket */

  SD_POWER_ON();
  delay_ms(100);
  
  // initialization sequence on PowerUp
  SD_DESELECT();
  for (n = 20; n; n--) spi_receiver_byte();        /* 80 dummy clocks */

  ty = 0;
  if (sdSendCmd(CMD0, 0) == 1) {   // Enter Idle state
    Timer1 = 200; // 2s
    if (sdSendCmd(CMD8, 0x1AA) == 1) {   // SDHC
      for (n = 0; n < 4; n++) ocr[n] = spi_receiver_byte();   // Get trailing return value of R7 response
      if (ocr[2] == 0x01 && ocr[3] == 0xAA) {               // The card can work at VDD range of 2.7-3.6V
        while (Timer1 && sdSendCmd(ACMD41, 0x40000000));
        if (Timer1 && sdSendCmd(CMD58, 0) == 0) {        // Check CCS bit in the OCR
          for (n = 0; n < 4; n++) ocr[n] = spi_receiver_byte();
          ty = (ocr[0] & 0x40) ? CT_SD2 | CT_BLOCK : CT_SD2;
        }
      }
    }
    else {    // SDSC or MMC
      if (sdSendCmd(ACMD41, 0) <= 1)   {
        ty = CT_SD1; cmd = ACMD41;    // SDSC
      }
      else {
        ty = CT_MMC; cmd = CMD1;      // MMC
      }
      while (Timer1 && sdSendCmd(cmd, 0));   // Wait for leaving idle state
      if (!Timer1 || sdSendCmd(CMD16, 512) != 0) {   // Set R/W block length to 512 */
        ty = 0;
      }
    }
  }

  CardType = ty;

  release_spi();

  if (ty) {                       /* Initialization succeeded */
    Stat &= ~STA_NOINIT;          /* Clear STA_NOINIT */
  }
  else {
    SD_POWER_OFF();
  }

  return Stat;
}



/*-----------------------------------------------------------------------*/
/* Get Disk Status                                                       */
/*-----------------------------------------------------------------------*/
DSTATUS disk_status (BYTE drv)               /* Physical drive number (0) */
{
  if (drv) return STA_NOINIT;             /* Supports only single drive */
  return Stat;
}


/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/
bool_t sdReadSector(uint8_t * buff, uint32_t sector)
{
  if (!(CardType & CT_BLOCK)) sector *= BLOCK_SIZE;      /* Convert to byte address if needed */
  if (sdSendCmd(CMD17, sector) == 0) {
    // READ_SINGLE_BLOCK
    if (sdReceiverBlock(buff, BLOCK_SIZE)) {
      return true;
    }
  }
  return false;
}

bool_t sdReadSectors(uint8_t * buff, uint32_t sector, uint32_t count)
{
  SD_SELECT();
  while(count--) {
    bool_t res = sdReadSector(buff, sector++);
    if (!res)
      return false;
    buff += BLOCK_SIZE;
  }
  release_spi();
  return true;
}

DRESULT disk_read (
        BYTE drv,                       /* Physical drive number (0) */
        BYTE *buff,                     /* Pointer to the data buffer to store read data */
        DWORD sector,           /* Start sector number (LBA) */
        UINT count                      /* Sector count (1..255) */
)
{
  if (drv || !count) return RES_PARERR;
  if (Stat & STA_NOINIT) return RES_NOTRDY;
  bool_t res = sdReadSectors(buff, sector, count);
  return (res == true) ? RES_OK : RES_ERROR;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/
bool_t sdWriteSector(const uint8_t * buff, uint32_t sector)
{
  if (!(CardType & CT_BLOCK)) sector *= BLOCK_SIZE;      /* Convert to byte address if needed */
  if (sdSendCmd(CMD24, sector) == 0) {
    // WRITE_SINGLE_BLOCK
    if (sdTransmitBlock(buff, 0xFE)) {
      return true;
    }
  }
  return false;
}

char sdWriteSectors(const uint8_t * buff, uint32_t sector, uint32_t count)
{
  SD_SELECT();
  while (count--) {
    bool_t res = sdWriteSector(buff, sector++);
    if (!res)
      return false;
    buff += BLOCK_SIZE;
  }
  release_spi();
  return true;
}

DRESULT disk_write (
        BYTE drv,                       /* Physical drive number (0) */
        const BYTE *buff,       /* Pointer to the data to be written */
        DWORD sector,           /* Start sector number (LBA) */
        UINT count                      /* Sector count (1..255) */
)
{
  if (drv || !count) return RES_PARERR;
  if (Stat & STA_NOINIT) return RES_NOTRDY;
  if (Stat & STA_PROTECT) return RES_WRPRT;
  char res = sdWriteSectors(buff, sector, count);
  return (res) ? RES_OK : RES_ERROR;
}


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
        BYTE drv,               /* Physical drive number (0) */
        BYTE ctrl,              /* Control code */
        void *buff              /* Buffer to send/receive control data */
)
{
  DRESULT res;
  BYTE n, csd[16];
  WORD csize;

  if (drv) return RES_PARERR;

  res = RES_ERROR;

  if (Stat & STA_NOINIT) {
    return RES_NOTRDY;
  }

  switch (ctrl) {
  case CTRL_SYNC :                /* Make sure that no pending write process */
    SD_SELECT();
    if (wait_ready() == 0xFF) {
      res = RES_OK;
    }
    break;

  case GET_SECTOR_COUNT : /* Get number of sectors on the disk (DWORD) */
    if ((sdSendCmd(CMD9, 0) == 0) && sdReceiverBlock(csd, 16)) {
      if ((csd[0] >> 6) == 1) {
        // SDC version 2.00
        csize = csd[9] + ((DWORD)csd[8] << 8) + 1;  /* WORD -> DWORD !!!*/
        *(DWORD*)buff = (DWORD)csize << 10;
      }
      else {
        // SDC version 1.XX or MMC
        n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
        csize = (csd[8] >> 6) + ((DWORD)csd[7] << 2) + ((DWORD)(csd[6] & 3) << 10) + 1;  /* WORD -> DWORD !!!*/
        *(DWORD*)buff = (DWORD)csize << (n - 9);
      }
      res = RES_OK;
    }
    break;

  case GET_SECTOR_SIZE :  /* Get R/W sector size (WORD) */
    *(DWORD*)buff = BLOCK_SIZE;
    res = RES_OK;
    break;

  case GET_BLOCK_SIZE :   /* Get erase block size in unit of sector (DWORD) */
    if (CardType & CT_SD2) {
      // SDC version 2.00 
      if (sdSendCmd(ACMD13, 0) == 0) { /* Read SD status */
        spi_receiver_byte();
        if (sdReceiverBlock(csd, 16)) {                          /* Read partial block */
          for (n = 64 - 16; n; n--) spi_receiver_byte();   /* Purge trailing data */
          *(DWORD*)buff = 16UL << (csd[10] >> 4);
          res = RES_OK;
        }
      }
    }
    else {                                        /* SDC version 1.XX or MMC */
      if ((sdSendCmd(CMD9, 0) == 0) && sdReceiverBlock(csd, 16)) {      /* Read CSD */
        if (CardType & CT_SD1) {        /* SDC version 1.XX */
          *(DWORD*)buff = (((csd[10] & 63) << 1) + ((DWORD)(csd[11] & 128) >> 7) + 1) << ((csd[13] >> 6) - 1);
        }
        else {                                        /* MMC */
          *(DWORD*)buff = ((DWORD)((csd[10] & 124) >> 2) + 1) * (((csd[11] & 3) << 3) + ((csd[11] & 224) >> 5) + 1);
        }
        res = RES_OK;
      }
    }
    // *((DWORD *)buff) = 512;
    // res = RES_OK;
    break;

#if 0
  case MMC_GET_TYPE :             /* Get card type flags (1 byte) */
    *ptr = CardType;
    res = RES_OK;
    break;

  case MMC_GET_CSD :              /* Receive CSD as a data block (16 bytes) */
    if (sdSendCmd(CMD9, 0) == 0              /* READ_CSD */
        && sdReceiverBlock(ptr, 16)) {
      res = RES_OK;
    }
    break;

  case MMC_GET_CID :              /* Receive CID as a data block (16 bytes) */
    if (sdSendCmd(CMD10, 0) == 0             /* READ_CID */
        && sdReceiverBlock(ptr, 16)) {
      res = RES_OK;
    }
    break;

  case MMC_GET_OCR :              /* Receive OCR as an R3 resp (4 bytes) */
    if (sdSendCmd(CMD58, 0) == 0) {  /* READ_OCR */
      for (n = 4; n; n--) *ptr++ = spi_receiver_byte();
      res = RES_OK;
    }
    break;

  case MMC_GET_SDSTAT :   /* Receive SD status as a data block (64 bytes) */
    if (sdSendCmd(ACMD13, 0) == 0) { /* SD_STATUS */
      spi_receiver_byte();
      if (sdReceiverBlock(ptr, 64)) {
        res = RES_OK;
      }
    }
    break;
#endif

  default:
    res = RES_PARERR;
  }

  release_spi();

  return res;
}

FATFS g_FATFS_Obj = { 0 };
DIR g_FATFS_Dir = { 0 };

#if defined(LOG_DEBUG)
FIL g_debugFile = {};
#endif

bool_t sdMount(void)
{
  FRESULT res = f_mount(&g_FATFS_Obj, "0:", 1);
  if (res == FR_OK) {
#if defined(LOG_DEBUG)
    f_open(&g_debugFile, "debug.log", FA_OPEN_ALWAYS | FA_WRITE);
    if (f_size(&g_debugFile) > 0) {
      f_lseek(&g_debugFile, f_size(&g_debugFile)); // append
    }
#endif
  }
  else if (res == FR_NO_FILESYSTEM) {
    if (sdFormat()) {
      f_mount(((void *)0), "0:", 0); // unmount SD
      delay_ms(100);
      res = f_mount(&g_FATFS_Obj, "0:", 1);
    }
  }
  return (res == FR_OK) ? true : false;
}

void sdUnmount()
{
  if (sdMounted()) {
#if defined(LOG_DEBUG)
    f_close(&g_debugFile);
#endif
    f_mount(((void *)0), "0:", 1); // unmount SD
  }
}

bool_t sdMounted()
{
  return (g_FATFS_Obj.fs_type != 0) ? true : false;
}

bool_t sdIsHC()
{
  return (CardType & CT_BLOCK) ? true : false;
}

bool_t sdFormat()
{
  FRESULT res;
  BYTE work[FF_MAX_SS];

  if (sdIsHC()) {
    MKFS_PARM mkfs = {FM_FAT32, 0, 0, 0, 0};
    res = f_mkfs("0:", &mkfs, work, sizeof(work));
  }
  else {
    MKFS_PARM mkfs = {FM_FAT, 0, 0, 0, 0};
    res = f_mkfs("0:", &mkfs, work, sizeof(work));
  }

  return (res == FR_OK) ? true : false;
}


