/*
** map183.c
** Mapper 183 — Gimmick (Bootleg)
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint8  reg183[8];
static uint8  reg183_irq_enable;
static uint32 reg183_irq_cnt;

static void map183_hblank(int vblank)
{
   UNUSED(vblank);
   if (reg183_irq_enable & 0x02)
   {
      if (reg183_irq_cnt <= 113)
      {
         reg183_irq_cnt = 0;
         nes_irq();
      }
      else
      {
         reg183_irq_cnt -= 113;
      }
   }
}

static void map183_write(uint32 address, uint8 value)
{
   int i;

   switch (address)
   {
   case 0x8800: mmc_bankrom(8, 0x8000, value); break;
   case 0xA800: mmc_bankrom(8, 0xA000, value); break;
   case 0xA000: mmc_bankrom(8, 0xC000, value); break;

   /* CHR nibble-based banking */
   case 0xB000: reg183[0] = (reg183[0] & 0xF0) | (value & 0x0F); mmc_bankvrom(1, 0x0000, reg183[0]); break;
   case 0xB004: reg183[0] = (reg183[0] & 0x0F) | ((value & 0x0F) << 4); mmc_bankvrom(1, 0x0000, reg183[0]); break;
   case 0xB008: reg183[1] = (reg183[1] & 0xF0) | (value & 0x0F); mmc_bankvrom(1, 0x0400, reg183[1]); break;
   case 0xB00C: reg183[1] = (reg183[1] & 0x0F) | ((value & 0x0F) << 4); mmc_bankvrom(1, 0x0400, reg183[1]); break;
   case 0xC000: reg183[2] = (reg183[2] & 0xF0) | (value & 0x0F); mmc_bankvrom(1, 0x0800, reg183[2]); break;
   case 0xC004: reg183[2] = (reg183[2] & 0x0F) | ((value & 0x0F) << 4); mmc_bankvrom(1, 0x0800, reg183[2]); break;
   case 0xC008: reg183[3] = (reg183[3] & 0xF0) | (value & 0x0F); mmc_bankvrom(1, 0x0C00, reg183[3]); break;
   case 0xC00C: reg183[3] = (reg183[3] & 0x0F) | ((value & 0x0F) << 4); mmc_bankvrom(1, 0x0C00, reg183[3]); break;
   case 0xD000: reg183[4] = (reg183[4] & 0xF0) | (value & 0x0F); mmc_bankvrom(1, 0x1000, reg183[4]); break;
   case 0xD004: reg183[4] = (reg183[4] & 0x0F) | ((value & 0x0F) << 4); mmc_bankvrom(1, 0x1000, reg183[4]); break;
   case 0xD008: reg183[5] = (reg183[5] & 0xF0) | (value & 0x0F); mmc_bankvrom(1, 0x1400, reg183[5]); break;
   case 0xD00C: reg183[5] = (reg183[5] & 0x0F) | ((value & 0x0F) << 4); mmc_bankvrom(1, 0x1400, reg183[5]); break;
   case 0xE000: reg183[6] = (reg183[6] & 0xF0) | (value & 0x0F); mmc_bankvrom(1, 0x1800, reg183[6]); break;
   case 0xE004: reg183[6] = (reg183[6] & 0x0F) | ((value & 0x0F) << 4); mmc_bankvrom(1, 0x1800, reg183[6]); break;
   case 0xE008: reg183[7] = (reg183[7] & 0xF0) | (value & 0x0F); mmc_bankvrom(1, 0x1C00, reg183[7]); break;
   case 0xE00C: reg183[7] = (reg183[7] & 0x0F) | ((value & 0x0F) << 4); mmc_bankvrom(1, 0x1C00, reg183[7]); break;

   case 0x9008:
      if (value == 1)
      {
         int last = mmc_getinfo()->rom_banks * 2 - 1;
         for (i = 0; i < 8; i++) reg183[i] = i;
         mmc_bankrom(8, 0x8000, 0);
         mmc_bankrom(8, 0xA000, 1);
         mmc_bankrom(8, 0xC000, last - 1);
         mmc_bankrom(8, 0xE000, last);
         for (i = 0; i < 8; i++) mmc_bankvrom(1, i * 0x400, i);
      }
      break;
   case 0x9800:
      if      (value == 0) ppu_mirror(0, 1, 0, 1); /* vertical */
      else if (value == 1) ppu_mirror(0, 0, 1, 1); /* horizontal */
      else if (value == 2) ppu_mirror(0, 0, 0, 0); /* NT0 */
      else if (value == 3) ppu_mirror(1, 1, 1, 1); /* NT1 */
      break;

   case 0xF000:
      reg183_irq_cnt = (reg183_irq_cnt & 0xFF00) | value;
      break;
   case 0xF004:
      reg183_irq_cnt = (reg183_irq_cnt & 0x00FF) | ((uint32)value << 8);
      break;
   case 0xF008:
      reg183_irq_enable = value;
      break;
   }
}

static void map183_init(void)
{
   int i, last;
   last = mmc_getinfo()->rom_banks * 2 - 1;
   for (i = 0; i < 8; i++) reg183[i] = i;
   reg183_irq_enable = 0;
   reg183_irq_cnt    = 0;
   mmc_bankrom(8, 0x8000, 0);
   mmc_bankrom(8, 0xA000, 1);
   mmc_bankrom(8, 0xC000, last - 1);
   mmc_bankrom(8, 0xE000, last);
   for (i = 0; i < 8; i++) mmc_bankvrom(1, i * 0x400, i);
}

static const map_memwrite map183_memwrite[] =
{
   { 0x8000, 0xFFFF, map183_write },
   {     -1,     -1, NULL }
};

const mapintf_t map183_intf =
{
   183,                   /* mapper number */
   "Gimmick Bootleg",     /* mapper name */
   map183_init,           /* init */
   NULL,                  /* vblank */
   map183_hblank,         /* hblank */
   NULL, NULL, NULL,
   map183_memwrite,
   NULL
};
