/*
** map252.c
** Mapper 252 — Sangokushi: Chuugen no Hasha (VRC2/4-style CHR split-nibble)
**
** PRG: $8000/$A000 = 8KB switchable; $C000/$E000 = fixed to last two 8KB pages.
** CHR: 8 individual 1KB pages, each set in two nibble writes (lo at +0, hi at +4).
** IRQ: count-up 8-bit counter; fires when it wraps 0xFF→overflow.
**      Enable register also controls auto-reload (bit 0 = latch after IRQ).
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint8  reg252[9];   /* CHR nibble registers (2 per page × 8 pages, but shared) */
static uint8  irq252_enable, irq252_counter, irq252_latch, irq252_occur;

static void map252_hblank(int vblank)
{
   (void)vblank;   /* counter runs unconditionally every hblank */
   if (irq252_enable & 0x02) {
      if (irq252_counter == 0xFF) {
         irq252_occur   = 1;
         irq252_counter = irq252_latch;
         /* auto-reload: if latch bit (bit 0) was set, re-enable; else disable */
         irq252_enable  = (uint8)((irq252_enable & 0x01) * 3);
      } else {
         irq252_counter++;
      }
   }
   if (irq252_occur)
      nes_irq();
}

static void map252_write(uint32 address, uint8 value)
{
   /* Simple 8KB PRG selects */
   if ((address & 0xF000) == 0x8000) {
      mmc_bankrom(8, 0x8000, value);
      return;
   }
   if ((address & 0xF000) == 0xA000) {
      mmc_bankrom(8, 0xA000, value);
      return;
   }

   /* CHR nibble / IRQ registers via address bits [15:12] and [3:2] */
   switch (address & 0xF00C) {
   /* PPU $0000 — CHR page 0 */
   case 0xB000:
      reg252[0] = (reg252[0] & 0xF0) | (value & 0x0F);
      mmc_bankvrom(1, 0x0000, reg252[0]);
      break;
   case 0xB004:
      reg252[0] = (reg252[0] & 0x0F) | ((value & 0x0F) << 4);
      mmc_bankvrom(1, 0x0000, reg252[0]);
      break;

   /* PPU $0400 — CHR page 1 */
   case 0xB008:
      reg252[1] = (reg252[1] & 0xF0) | (value & 0x0F);
      mmc_bankvrom(1, 0x0400, reg252[1]);
      break;
   case 0xB00C:
      reg252[1] = (reg252[1] & 0x0F) | ((value & 0x0F) << 4);
      mmc_bankvrom(1, 0x0400, reg252[1]);
      break;

   /* PPU $0800 — CHR page 2 */
   case 0xC000:
      reg252[2] = (reg252[2] & 0xF0) | (value & 0x0F);
      mmc_bankvrom(1, 0x0800, reg252[2]);
      break;
   case 0xC004:
      reg252[2] = (reg252[2] & 0x0F) | ((value & 0x0F) << 4);
      mmc_bankvrom(1, 0x0800, reg252[2]);
      break;

   /* PPU $0C00 — CHR page 3 */
   case 0xC008:
      reg252[3] = (reg252[3] & 0xF0) | (value & 0x0F);
      mmc_bankvrom(1, 0x0C00, reg252[3]);
      break;
   case 0xC00C:
      reg252[3] = (reg252[3] & 0x0F) | ((value & 0x0F) << 4);
      mmc_bankvrom(1, 0x0C00, reg252[3]);
      break;

   /* PPU $1000 — CHR page 4 */
   case 0xD000:
      reg252[4] = (reg252[4] & 0xF0) | (value & 0x0F);
      mmc_bankvrom(1, 0x1000, reg252[4]);
      break;
   case 0xD004:
      reg252[4] = (reg252[4] & 0x0F) | ((value & 0x0F) << 4);
      mmc_bankvrom(1, 0x1000, reg252[4]);
      break;

   /* PPU $1400 — CHR page 5 */
   case 0xD008:
      reg252[5] = (reg252[5] & 0xF0) | (value & 0x0F);
      mmc_bankvrom(1, 0x1400, reg252[5]);
      break;
   case 0xD00C:
      reg252[5] = (reg252[5] & 0x0F) | ((value & 0x0F) << 4);
      mmc_bankvrom(1, 0x1400, reg252[5]);
      break;

   /* PPU $1800 — CHR page 6 */
   case 0xE000:
      reg252[6] = (reg252[6] & 0xF0) | (value & 0x0F);
      mmc_bankvrom(1, 0x1800, reg252[6]);
      break;
   case 0xE004:
      reg252[6] = (reg252[6] & 0x0F) | ((value & 0x0F) << 4);
      mmc_bankvrom(1, 0x1800, reg252[6]);
      break;

   /* PPU $1C00 — CHR page 7 */
   case 0xE008:
      reg252[7] = (reg252[7] & 0xF0) | (value & 0x0F);
      mmc_bankvrom(1, 0x1C00, reg252[7]);
      break;
   case 0xE00C:
      reg252[7] = (reg252[7] & 0x0F) | ((value & 0x0F) << 4);
      mmc_bankvrom(1, 0x1C00, reg252[7]);
      break;

   /* IRQ latch low nibble */
   case 0xF000:
      irq252_latch  = (irq252_latch & 0xF0) | (value & 0x0F);
      irq252_occur  = 0;
      break;
   /* IRQ latch high nibble */
   case 0xF004:
      irq252_latch  = (irq252_latch & 0x0F) | ((value & 0x0F) << 4);
      irq252_occur  = 0;
      break;
   /* IRQ enable */
   case 0xF008:
      irq252_enable = value & 0x03;
      if (irq252_enable & 0x02) {
         irq252_counter = irq252_latch;
      }
      irq252_occur = 0;
      break;
   /* IRQ acknowledge (carry latch bit to enable) */
   case 0xF00C:
      irq252_enable = (uint8)((irq252_enable & 0x01) * 3);
      irq252_occur  = 0;
      break;
   }
}

static void map252_init(void)
{
   int last = mmc_getinfo()->rom_banks * 2 - 1;
   int i;

   for (i = 0; i < 9; i++) reg252[i] = (uint8)i;

   irq252_enable  = 0;
   irq252_counter = 0;
   irq252_latch   = 0;
   irq252_occur   = 0;

   mmc_bankrom(8, 0x8000, 0);
   mmc_bankrom(8, 0xA000, 1);
   mmc_bankrom(8, 0xC000, last - 1);
   mmc_bankrom(8, 0xE000, last);

   if (mmc_getinfo()->vrom_banks > 0) {
      mmc_bankvrom(1, 0x0000, 0); mmc_bankvrom(1, 0x0400, 1);
      mmc_bankvrom(1, 0x0800, 2); mmc_bankvrom(1, 0x0C00, 3);
      mmc_bankvrom(1, 0x1000, 4); mmc_bankvrom(1, 0x1400, 5);
      mmc_bankvrom(1, 0x1800, 6); mmc_bankvrom(1, 0x1C00, 7);
   }
}

static const map_memwrite map252_memwrite[] =
{
   { 0x8000, 0xFFFF, map252_write },
   {     -1,     -1, NULL }
};

const mapintf_t map252_intf =
{
   252,                        /* mapper number */
   "Sangokushi",               /* mapper name */
   map252_init,                /* init */
   NULL,                       /* vblank */
   map252_hblank,              /* hblank */
   NULL, NULL, NULL,
   map252_memwrite,
   NULL
};
