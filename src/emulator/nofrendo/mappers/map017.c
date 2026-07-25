/*
** map017.c
** Mapper 17 — FFE F8 Series
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint8  reg17_irq_enable;
static uint32 reg17_irq_cnt;
static uint32 reg17_irq_latch;

static void map17_hblank(int vblank)
{
   if (!vblank && reg17_irq_enable)
   {
      if (reg17_irq_cnt >= 0xFFFF - 113)
      {
         nes_irq();
         reg17_irq_cnt    = 0;
         reg17_irq_enable = 0;
      }
      else
      {
         reg17_irq_cnt += 113;
      }
   }
}

static void map17_write(uint32 address, uint8 value)
{
   switch (address)
   {
   case 0x42FE:
      if ((value & 0x10) == 0) ppu_mirror(1, 1, 1, 1); /* NT1 */
      else                     ppu_mirror(0, 0, 0, 0); /* NT0 */
      break;
   case 0x42FF:
      if ((value & 0x10) == 0) ppu_mirror(0, 1, 0, 1); /* vertical */
      else                     ppu_mirror(0, 0, 1, 1); /* horizontal */
      break;
   case 0x4501: reg17_irq_enable = 0; break;
   case 0x4502:
      reg17_irq_latch = (reg17_irq_latch & 0xFF00) | value;
      break;
   case 0x4503:
      reg17_irq_latch = (reg17_irq_latch & 0x00FF) | ((uint32)value << 8);
      reg17_irq_cnt   = reg17_irq_latch;
      reg17_irq_enable = 1;
      break;
   case 0x4504: mmc_bankrom(8, 0x8000, value); break;
   case 0x4505: mmc_bankrom(8, 0xA000, value); break;
   case 0x4506: mmc_bankrom(8, 0xC000, value); break;
   case 0x4507: mmc_bankrom(8, 0xE000, value); break;
   case 0x4510: mmc_bankvrom(1, 0x0000, value); break;
   case 0x4511: mmc_bankvrom(1, 0x0400, value); break;
   case 0x4512: mmc_bankvrom(1, 0x0800, value); break;
   case 0x4513: mmc_bankvrom(1, 0x0C00, value); break;
   case 0x4514: mmc_bankvrom(1, 0x1000, value); break;
   case 0x4515: mmc_bankvrom(1, 0x1400, value); break;
   case 0x4516: mmc_bankvrom(1, 0x1800, value); break;
   case 0x4517: mmc_bankvrom(1, 0x1C00, value); break;
   }
}

static void map17_init(void)
{
   int last = mmc_getinfo()->rom_banks * 2 - 1;
   reg17_irq_enable = 0;
   reg17_irq_cnt    = 0;
   reg17_irq_latch  = 0;
   mmc_bankrom(8, 0x8000, 0);
   mmc_bankrom(8, 0xA000, 1);
   mmc_bankrom(8, 0xC000, last - 1);
   mmc_bankrom(8, 0xE000, last);
   mmc_bankvrom(1, 0x0000, 0);
   mmc_bankvrom(1, 0x0400, 1);
   mmc_bankvrom(1, 0x0800, 2);
   mmc_bankvrom(1, 0x0C00, 3);
   mmc_bankvrom(1, 0x1000, 4);
   mmc_bankvrom(1, 0x1400, 5);
   mmc_bankvrom(1, 0x1800, 6);
   mmc_bankvrom(1, 0x1C00, 7);
}

static const map_memwrite map17_memwrite[] =
{
   { 0x4100, 0x5FFF, map17_write },
   {     -1,     -1, NULL }
};

const mapintf_t map17_intf =
{
   17,             /* mapper number */
   "FFE F8",       /* mapper name */
   map17_init,     /* init */
   NULL,           /* vblank */
   map17_hblank,   /* hblank */
   NULL, NULL, NULL,
   map17_memwrite,
   NULL
};
