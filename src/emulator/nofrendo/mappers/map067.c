/*
** map067.c
** Mapper 67 (Sunsoft Fantasy Zone II / Sunsoft-3)
**
** CHR: 4 × 2KB pages via $8800/$9800/$A800/$B800.
** PRG: 16KB switchable at $8000 via $E800, $C000 fixed to last.
** IRQ: 16-bit counter, decremented per scanline; fires on underflow.
**      Counter written in two bytes via $C800 (MSB first, LSB second).
**      $D800 bit 4 = enable; write also resets the toggle.
** Mirroring: $F800 bits 1-0 (0=vert, 1=horiz, 2=NT0, 3=NT1).
*/

#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static struct
{
   bool     enabled;
   bool     toggle;  /* false = next $C800 write = MSB, true = LSB */
   uint16   counter;
   uint8    latch_hi;
} irq;

static void map67_write(uint32 address, uint8 value)
{
   switch (address & 0xF800)
   {
   case 0x8800: mmc_bankvrom(2, 0x0000, value & 0x3F); break;
   case 0x9800: mmc_bankvrom(2, 0x0800, value & 0x3F); break;
   case 0xA800: mmc_bankvrom(2, 0x1000, value & 0x3F); break;
   case 0xB800: mmc_bankvrom(2, 0x1800, value & 0x3F); break;

   case 0xC800:
      if (!irq.toggle)
      {
         irq.latch_hi = value;
         irq.toggle = true;
      }
      else
      {
         irq.counter = ((uint16)irq.latch_hi << 8) | value;
         irq.toggle = false;
      }
      break;

   case 0xD800:
      irq.enabled = !!(value & 0x10);
      irq.toggle  = false;
      break;

   case 0xE800:
      mmc_bankrom(16, 0x8000, value & 0x07);
      break;

   case 0xF800:
      switch (value & 0x03)
      {
      case 0: ppu_mirror(0, 1, 0, 1); break; /* vertical   */
      case 1: ppu_mirror(0, 0, 1, 1); break; /* horizontal */
      case 2: ppu_mirror(0, 0, 0, 0); break; /* one-screen NT0 */
      case 3: ppu_mirror(1, 1, 1, 1); break; /* one-screen NT1 */
      }
      break;
   }
}

static void map67_hblank(int vblank)
{
   UNUSED(vblank);
   if (!irq.enabled) return;

   if (irq.counter == 0)
   {
      irq.enabled = false;
      irq.counter = 0xFFFF;
      nes_irq();
   }
   else
   {
      irq.counter--;
   }
}

static void map67_init(void)
{
   irq.enabled  = false;
   irq.toggle   = false;
   irq.counter  = 0;
   irq.latch_hi = 0;

   mmc_bankrom(16, 0x8000, 0);
   mmc_bankrom(16, 0xC000, mmc_getinfo()->rom_banks - 1);
}

static const map_memwrite map67_memwrite[] =
{
   { 0x8000, 0xFFFF, map67_write },
   {     -1,     -1, NULL }
};

const mapintf_t map67_intf =
{
   67,              /* mapper number */
   "Sunsoft-3",     /* mapper name */
   map67_init,      /* init routine */
   NULL,            /* vblank callback */
   map67_hblank,    /* hblank callback */
   NULL, NULL, NULL,
   map67_memwrite,
   NULL
};
