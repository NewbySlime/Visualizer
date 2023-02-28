#ifndef DEFINES_HEADER
#define DEFINES_HEADER

#if defined(STM32F103xB)
#include "stm32f1xx.h"
#include "stm32f1xx_hal_rcc.h"

#define STM32F1XX
#define STM32XX
#endif

#ifdef STM32XX
  #ifndef F_CPU
  #define F_CPU 72000000L
  #endif

  #define F_APB1 36000000L
  #define F_APB2 72000000L

void SystemClock_Config(void);

#define ASYNC_TIMER_NUM 2
#define ASYNC_TIMER TIM2
#define ASYNC_TIMER_BASE TIM2_BASE
#define ASYNC_TIMER_ISR tim2_isr

#define WS2812B_PERIPH USART1
#define WS2812B_PERIPH_NUM 1
#define WS2812B_PERIPH_TYPE pt_uart | WS2812B_PERIPH_NUM

#define HC05_PERIPH USART2
#define HC05_PERIPH_NUM 2
#define HC05_PERIPH_TYPE pt_uart | HC05_PERIPH_NUM

#define SERIAL_PERIPH USART3
#define SERIAL_PERIPH_NUM 3
#define SERIAL_PERIPH_TYPE pt_uart | SERIAL_PERIPH_NUM
#define SERIAL_BAUDRATE 160000

void __rcc_init();
void __gpio_init();
#endif

#define MAX_COLOR_RANGE_COUNT 16
#define MAX_SPLIT_PARTS_COUNT 8

#define SEC_TO_US 1000000
#define SEC_TO_MS 1000
#define MS_TO_US 1000


// bluetooth modules
#define USING_HC05


// use this when the MCU always interrupted by a feature (like ESP8266 for the WiFi feature)
//#define WS2812B_USE_EXTERNAL_DRIVER


#define VIS_LED_COUNT 2

typedef struct waveform_data{
  float *wvf_data;    // the data already resized to match the led length
  uint16_t wvf_datalen;

  float avg_wvf;    // the average from wvf_data
} waveform_data;

#endif
