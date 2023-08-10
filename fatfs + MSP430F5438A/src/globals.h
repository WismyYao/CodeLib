#pragma once

#ifndef _GLOBALS_H_
#define _GLOBALS_H_

typedef signed char   int8_t;
typedef signed int    int16_t;
typedef signed long   int32_t;
typedef unsigned char uint8_t;
typedef unsigned int  uint16_t;
typedef unsigned long uint32_t;

typedef enum {
  false = 0,
  true = 1
} bool_t;

// global timers
typedef uint32_t tmr10ms_t;
extern volatile tmr10ms_t g_tmr10ms;
extern volatile tmr10ms_t Timer1;
extern volatile tmr10ms_t Timer2;



#endif // !_GLOBALS_H_
