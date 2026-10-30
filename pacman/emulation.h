#ifndef _EMULATION_H_
#define _EMULATION_H_

#include "Z80.h"

#define BUTTON_LEFT  0x01
#define BUTTON_RIGHT 0x02
#define BUTTON_UP    0x04
#define BUTTON_DOWN  0x08
#define BUTTON_FIRE  0x10
#define BUTTON_START 0x20
#define BUTTON_COIN  0x40
#define BUTTON_EXTRA 0x80

#define MACHINE_IS_PACMAN 1
#define PACMAN_BEGIN
#define PACMAN_END

#if defined(__cplusplus)
extern "C" {
#endif

void prepare_emulation(void);
void emulate_frame(void);

extern unsigned char *memory;
extern char game_started;
extern unsigned char soundregs[32];
extern unsigned char irq_ptr;
extern char irq_enable[1];
extern Z80 cpu[1];
extern const unsigned char pacman_rom[];

unsigned char buttons_get(void);

#if defined(__cplusplus)
}
#endif

static inline byte OpZ80_INL(register z80_word Addr) {
  return pacman_rom[Addr];
}

#endif
