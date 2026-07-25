/*
** map182.c
** Mapper 182 — Pirates (command/data with scanline IRQ)
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint8 reg182_cmd;
static uint8 reg182_irq_enable;
static uint8 reg182_irq_cnt;

static void map182_hblank(int vblank)
{
   if (!vblank && reg182_irq_enable)
   {
      if (!(--reg182_irq_cnt))
      {
         reg182_irq_cnt    = 0;
         reg182_irq_enable = 0;
         nes_irq();
      }
   }
}

static void map182_write(uint32 address, uint8 value)
{
   switch (address & 0xF003)
   {
   case 0x8001:
      if (value & 0x01) ppu_mirror(0, 0, 1, 1); /* horizontal */
      else              ppu_mirror(0, 1, 0, 1); /* vertical */
      break;
   case 0xA000:
      reg182_cmd = value & 0x07;
      break;
   case 0xC000:
      switch (reg182_cmd)
      {
      case 0x00:
         mmc_bankvrom(1, 0x0000, (value & 0xFE) + 0);
         mmc_bankvrom(1, 0x0400, (value & 0xFE) + 1);
         break;
      case 0x01:
         mmc_bankvrom(1, 0x1400, value);
         break;
      case 0x02:
         mmc_bankvrom(1, 0x0800, (value & 0xFE) + 0);
         mmc_bankvrom(1, 0x0C00, (value & 0xFE) + 1);
         break;
      case 0x03:
         mmc_bankvrom(1, 0x1C00, value);
         break;
      case 0x04:
         mmc_bankrom(8, 0x8000, value);
         break;
      case 0x05:
         mmc_bankrom(8, 0xA000, value);
         break;
      case 0x06:
         mmc_bankvrom(1, 0x1000, value);
         break;
      case 0x07:
         mmc_bankvrom(1, 0x1800, value);
         break;
      }
      break;
   case 0xE003:
      reg182_irq_cnt    = value;
      reg182_irq_enable = value;
      break;
   }
}

static void map182_init(void)
{
   int last = mmc_getinfo()->rom_banks * 2 - 1;
   reg182_cmd         = 0;
   reg182_irq_enable  = 0;
   reg182_irq_cnt     = 0;
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

static const map_memwrite map182_memwrite[] =
{
   { 0x8000, 0xFFFF, map182_write },
   {     -1,     -1, NULL }
};

const mapintf_t map182_intf =
{
   182,            /* mapper number */
   "Pirates 182",  /* mapper name */
   map182_init,    /* init */
   NULL,           /* vblank */
   map182_hblank,  /* hblank */
   NULL, NULL, NULL,
   map182_memwrite,
   NULL
};
