/*
** map006.c
** Mapper 6 — FFE (with CHR RAM and IRQ)
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint8  reg6_irq_enable;
static uint32 reg6_irq_cnt;

static void map6_hblank(int vblank)
{
   if (!vblank && reg6_irq_enable)
   {
      reg6_irq_cnt += 133;
      if (reg6_irq_cnt >= 0xFFFF)
      {
         reg6_irq_cnt = 0;
         nes_irq();
      }
   }
}

static void map6_apu_write(uint32 address, uint8 value)
{
   switch (address)
   {
   case 0x42FE:
      if (value & 0x10) ppu_mirror(0, 0, 0, 0); /* NT0 */
      else              ppu_mirror(1, 1, 1, 1); /* NT1 */
      break;
   case 0x42FF:
      if (value & 0x10) ppu_mirror(0, 0, 1, 1); /* horizontal */
      else              ppu_mirror(0, 1, 0, 1); /* vertical */
      break;
   case 0x4501:
      reg6_irq_enable = 0;
      break;
   case 0x4502:
      reg6_irq_cnt = (reg6_irq_cnt & 0xFF00) | (uint32)value;
      break;
   case 0x4503:
      reg6_irq_cnt = (reg6_irq_cnt & 0x00FF) | ((uint32)value << 8);
      reg6_irq_enable = 1;
      break;
   }
}

static void map6_write(uint32 address, uint8 value)
{
   uint8 prg = ((value & 0x3C) >> 2) << 1;
   uint8 chr = (value & 0x03) << 3;

   UNUSED(address);

   mmc_bankrom(8, 0x8000, prg + 0);
   mmc_bankrom(8, 0xA000, prg + 1);

   mmc_bankvrom(1, 0x0000, chr + 0);
   mmc_bankvrom(1, 0x0400, chr + 1);
   mmc_bankvrom(1, 0x0800, chr + 2);
   mmc_bankvrom(1, 0x0C00, chr + 3);
   mmc_bankvrom(1, 0x1000, chr + 4);
   mmc_bankvrom(1, 0x1400, chr + 5);
   mmc_bankvrom(1, 0x1800, chr + 6);
   mmc_bankvrom(1, 0x1C00, chr + 7);
}

static void map6_init(void)
{
   reg6_irq_enable = 0;
   reg6_irq_cnt    = 0;
   mmc_bankrom(8, 0x8000,  0);
   mmc_bankrom(8, 0xA000,  1);
   mmc_bankrom(8, 0xC000, 14);
   mmc_bankrom(8, 0xE000, 15);
   mmc_bankvrom(1, 0x0000, 0);
   mmc_bankvrom(1, 0x0400, 1);
   mmc_bankvrom(1, 0x0800, 2);
   mmc_bankvrom(1, 0x0C00, 3);
   mmc_bankvrom(1, 0x1000, 4);
   mmc_bankvrom(1, 0x1400, 5);
   mmc_bankvrom(1, 0x1800, 6);
   mmc_bankvrom(1, 0x1C00, 7);
}

static const map_memwrite map6_memwrite[] =
{
   { 0x4100, 0x5FFF, map6_apu_write },
   { 0x8000, 0xFFFF, map6_write },
   {     -1,     -1, NULL }
};

const mapintf_t map6_intf =
{
   6,          /* mapper number */
   "FFE",      /* mapper name */
   map6_init,  /* init */
   NULL,       /* vblank */
   map6_hblank,/* hblank */
   NULL, NULL, NULL,
   map6_memwrite,
   NULL
};
