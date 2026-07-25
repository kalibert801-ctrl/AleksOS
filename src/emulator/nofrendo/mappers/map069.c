/*
** map069.c
** Mapper 69 (Sunsoft FME-7 / 5B)
**
** Register pair: write command to $8000-$9FFF, then data to $A000-$BFFF.
**
** Commands:
**   0-7:  CHR 1KB banks at PPU $0000-$1FFF (8 × 1KB)
**   8:    $6000 PRG/RAM control (ignored here)
**   9:    PRG 8KB bank at $8000
**   A:    PRG 8KB bank at $A000
**   B:    PRG 8KB bank at $C000  ($E000 always fixed to last)
**   C:    Mirroring (bits 1-0: 0=vert, 1=horiz, 2=NT0, 3=NT1)
**   D:    IRQ control (bit 7=IRQ enable, bit 0=counter enable)
**   E:    IRQ counter low byte
**   F:    IRQ counter high byte
**
** IRQ: 16-bit counter decremented ~114 cycles/scanline (hblank).
**      Fires nes_irq() on underflow when both enables are set.
*/

#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint8 cmd;

static struct
{
   bool  irq_en;   /* fires IRQ on wrap */
   bool  cnt_en;   /* counter is counting */
   int   counter;  /* signed for underflow detection */
} irq;

static void map69_write(uint32 address, uint8 value)
{
   if (address < 0xA000)
   {
      cmd = value & 0x0F;
      return;
   }

   switch (cmd)
   {
   /* CHR 1KB banks */
   case 0: mmc_bankvrom(1, 0x0000, value); break;
   case 1: mmc_bankvrom(1, 0x0400, value); break;
   case 2: mmc_bankvrom(1, 0x0800, value); break;
   case 3: mmc_bankvrom(1, 0x0C00, value); break;
   case 4: mmc_bankvrom(1, 0x1000, value); break;
   case 5: mmc_bankvrom(1, 0x1400, value); break;
   case 6: mmc_bankvrom(1, 0x1800, value); break;
   case 7: mmc_bankvrom(1, 0x1C00, value); break;

   /* PRG 8KB banks (command 8 = $6000 WRAM, ignored) */
   case 9:  mmc_bankrom(8, 0x8000, value & 0x3F); break;
   case 10: mmc_bankrom(8, 0xA000, value & 0x3F); break;
   case 11: mmc_bankrom(8, 0xC000, value & 0x3F); break;

   case 12:
      switch (value & 0x03)
      {
      case 0: ppu_mirror(0, 1, 0, 1); break; /* vertical   */
      case 1: ppu_mirror(0, 0, 1, 1); break; /* horizontal */
      case 2: ppu_mirror(0, 0, 0, 0); break; /* one-screen NT0 */
      case 3: ppu_mirror(1, 1, 1, 1); break; /* one-screen NT1 */
      }
      break;

   case 13:
      irq.irq_en = !!(value & 0x80);
      irq.cnt_en = !!(value & 0x01);
      break;

   case 14:
      irq.counter = (irq.counter & 0xFF00) | value;
      break;

   case 15:
      irq.counter = (irq.counter & 0x00FF) | (value << 8);
      break;
   }
}

static void map69_hblank(int vblank)
{
   UNUSED(vblank);
   if (!irq.cnt_en) return;

   irq.counter -= 114;
   if (irq.counter < 0)
   {
      irq.counter += 0x10000; /* wrap to 16-bit range */
      if (irq.irq_en)
         nes_irq();
   }
}

static void map69_init(void)
{
   cmd = 0;
   irq.irq_en  = false;
   irq.cnt_en  = false;
   irq.counter = 0;

   /* $E000 always fixed to last 8KB bank */
   mmc_bankrom(8, 0x8000, 0);
   mmc_bankrom(8, 0xA000, 0);
   mmc_bankrom(8, 0xC000, 0);
   mmc_bankrom(8, 0xE000, mmc_getinfo()->rom_banks * 2 - 1);
}

static const map_memwrite map69_memwrite[] =
{
   { 0x8000, 0xBFFF, map69_write },
   {     -1,     -1, NULL }
};

const mapintf_t map69_intf =
{
   69,           /* mapper number */
   "FME-7",      /* mapper name */
   map69_init,   /* init routine */
   NULL,         /* vblank callback */
   map69_hblank, /* hblank callback */
   NULL, NULL, NULL,
   map69_memwrite,
   NULL
};
