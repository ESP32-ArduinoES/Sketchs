#include <stdlib.h>
#include <string.h>

#include <esp32-hal.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "Z80.h"
#include "config.h"

#define CPU_EMULATION
#include "emulation.h"

#define RAMSIZE 8192
#define INST_PER_FRAME (300000 / 60 / 4)
#define Z80_CPU_COUNT 1

#include "pacman.h"

Z80 cpu[Z80_CPU_COUNT];
unsigned char *memory;
char game_started = 0;
char irq_enable[Z80_CPU_COUNT] = { 0 };
unsigned char irq_ptr = 0;
unsigned char soundregs[32] = { 0 };

void PatchZ80(Z80 *R) {
  (void)R;
}

void OutZ80(unsigned short Port, unsigned char Value) {
  pacman_OutZ80(Port, Value);
}

unsigned char InZ80(unsigned short Port) {
  (void)Port;
  return 0;
}

void WrZ80(unsigned short Addr, unsigned char Value) {
  pacman_WrZ80(Addr, Value);
}

unsigned char RdZ80(unsigned short Addr) {
  return pacman_RdZ80(Addr);
}

void prepare_emulation(void) {
  memory = malloc(RAMSIZE);
  memset(memory, 0, RAMSIZE);
  ResetZ80(&cpu[0]);
}

void emulate_frame(void) {
  pacman_run_frame();

  if(game_started)
    ulTaskNotifyTake(1, 0xffffffffUL);
  else
    vTaskDelay(1);
}
