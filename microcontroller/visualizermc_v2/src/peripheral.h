#ifndef PERIPHERAL_HEADER
#define PERIPHERAL_HEADER

#ifndef PERIPHERAL_BUFFERSIZE
#define PERIPHERAL_BUFFERSIZE 1024
#endif

#include "stdint.h"
#include "defines.h"



typedef void(*peripheral_interrupt_cb)(void*);


typedef enum peripheral_event{
  // [FLAG] means that the called command has been done
  // data in __pe_data is peripheral_command data (only the command)
  pe_donecommand = 0x0100,

  // flags that indicate what kind of event
  __pe_flags = 0xFF00,

  // data of the event that depends on the flag
  __pe_data = 0x00FF
} peripheral_event;

typedef enum peripheral_result{
  pr_ok,
  pr_false,
  pr_nocommand,
  pr_wrongtype,
  pr_notready
} peripheral_result;

typedef enum peripheral_command{
  pc_NOP        = 0x0000,

  // async capability: No
  pc_setup      = 0x0001,
  // async capability: Depends
  pc_start      = 0x0002,
  // async capability: Depends
  pc_stop       = 0x0003,

  // async capability: Yes
  pc_write      = 0x0004,
  // async capability: Yes
  pc_read       = 0x0005,

  // NOTE: If using async, the caller can only call command function once after this
  // since the command will run in another thread (interrupts).
  //
  // If needed to call more function as the procedure, use event function. Calling
  // command function from event always have to specify "using async."
  pc_async      = 0x0010,

  // used when the module is already resetted beforehand
  pc_noreset    = 0x0020,

  __pc_command    = 0x000F,
  __pc_optional   = 0x00F0,
  __pc_hardware   = 0xFF00
} peripheral_command;


#ifdef STM32XX
typedef enum peripheral_type{
  pt_tim16 = 0x0000,
  pt_uart = 0x0100,
  pt_i2c = 0x0200,

  __pt_peripheral_code = 0xFF00,
  __pt_peripheral_num = 0xFF
} peripheral_type;
#endif


typedef struct peripheral_info peripheral_info;
struct peripheral_info{
  peripheral_command command;
  
  void *peripheral_data;
  void *peripheral_base;

  peripheral_type type;

  void *additional_param;
  void *function_param;


  peripheral_result (*peripheralcb_command)(peripheral_info *pinfo);
  void (*peripheralcb_onevent)(peripheral_event event, peripheral_info *pinfo);
};

void set_peripheral_interrupt(peripheral_type type, peripheral_interrupt_cb cb, void *data);
void delete_peripheral_interrupt(peripheral_type type);

#endif