/*
** map189.c
** Mapper 189 — Pirates MMC3 variant
**
** PRG: 32KB bank select via $4100-$41FF (bits 5-4 of write value).
** CHR: MMC3-style 1KB switching via $8000/$8001 pair.
** PRG fine-tune: individual 8KB slots via $8001 cases 0x46/0x47.
** IRQ: scanline counter, reload from latch, active-low fire when counter wraps.
*/

#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint8 regs[1];
static uint8 irq_cnt, irq_latch, irq_enable;

/* $4100-$41FF: select 32KB PRG block via bits 5-4 of written value */
static void map189_apu_write(uint32 address, uint8 value)
{
   (void)address;
   value = (value & 0x30) >> 4;   /* 2-bit 32KB bank number */
   mmc_bankrom(8, 0x8000, value * 4 + 0);
   mmc_bankrom(8, 0xA000, value * 4 + 1);
   mmc_bankrom(8, 0xC000, value * 4 + 2);
   mmc_bankrom(8, 0xE000, value * 4 + 3);
}

static void map189_write(uint32 address, uint8 value)
{
   switch (address)
   {
   case 0x8000:
      regs[0] = value;
      break;

   case 0x8001:
      switch (regs[0])
      {
      case 0x40:
         mmc_bankvrom(1, 0x0000, value + 0);
         mmc_bankvrom(1, 0x0400, value + 1);
         break;
      case 0x41:
         mmc_bankvrom(1, 0x0800, value + 0);
         mmc_bankvrom(1, 0x0C00, value + 1);
         break;
      case 0x42:
         mmc_bankvrom(1, 0x1000, value);
         break;
      case 0x43:
         mmc_bankvrom(1, 0x1400, value);
         break;
      case 0x44:
         mmc_bankvrom(1, 0x1800, value);
         break;
      case 0x45:
         mmc_bankvrom(1, 0x1C00, value);
         break;
      case 0x46:
         mmc_bankrom(8, 0xC000, value); /* ROMBANK2 */
         break;
      case 0x47:
         mmc_bankrom(8, 0xA000, value); /* ROMBANK1 */
         break;
      }
      break;

   case 0xC000:
      irq_cnt = value;
      break;
   case 0xC001:
      irq_latch = value;
      break;
   case 0xE000:
      irq_enable = 0;
      break;
   case 0xE001:
      irq_enable = 1;
      break;
   }
}

static void map189_hblank(int vblank)
{
   if (!vblank && irq_enable) {
      if (!(--irq_cnt)) {
         irq_cnt = irq_latch;
         nes_irq();
      }
   }
}

static void map189_init(void)
{
   int i;
   int nb = mmc_getinfo()->rom_banks * 2;

   regs[0] = 0;
   irq_cnt = irq_latch = irq_enable = 0;

   mmc_bankrom(8, 0x8000, 0);
   mmc_bankrom(8, 0xA000, 1);
   mmc_bankrom(8, 0xC000, nb - 2); /* ROMLASTPAGE(1) */
   mmc_bankrom(8, 0xE000, nb - 1); /* ROMLASTPAGE(0) */

   for (i = 0; i < 8; i++)
      mmc_bankvrom(1, i * 0x400, i);
}

static const map_memwrite map189_memwrite[] =
{
   { 0x4100, 0x4FFF, map189_apu_write },
   { 0x8000, 0xFFFF, map189_write },
   {     -1,     -1, NULL }
};

const mapintf_t map189_intf =
{
   189,
   "Pirates MMC3",
   map189_init,
   NULL,             /* vblank */
   map189_hblank,    /* hblank */
   NULL, NULL, NULL,
   map189_memwrite,
   NULL
};
