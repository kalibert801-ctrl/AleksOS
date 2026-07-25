/*
** map076.c
** Mapper 76 — Namcot 109
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint8 reg76;

static void map76_write(uint32 address, uint8 value)
{
   switch (address)
   {
   case 0x8000:
      reg76 = value;
      break;

   case 0x8001:
      switch (reg76 & 0x07)
      {
      case 0x02:
         mmc_bankvrom(1, 0x0000, (value << 1) + 0);
         mmc_bankvrom(1, 0x0400, (value << 1) + 1);
         break;
      case 0x03:
         mmc_bankvrom(1, 0x0800, (value << 1) + 0);
         mmc_bankvrom(1, 0x0C00, (value << 1) + 1);
         break;
      case 0x04:
         mmc_bankvrom(1, 0x1000, (value << 1) + 0);
         mmc_bankvrom(1, 0x1400, (value << 1) + 1);
         break;
      case 0x05:
         mmc_bankvrom(1, 0x1800, (value << 1) + 0);
         mmc_bankvrom(1, 0x1C00, (value << 1) + 1);
         break;
      case 0x06:
         mmc_bankrom(8, 0x8000, value);
         break;
      case 0x07:
         mmc_bankrom(8, 0xA000, value);
         break;
      }
      break;
   }
}

static void map76_init(void)
{
   reg76 = 0;
   mmc_bankrom(8, 0x8000, 0);
   mmc_bankrom(8, 0xA000, 1);
   mmc_bankrom(8, 0xC000, mmc_getinfo()->rom_banks * 2 - 2);
   mmc_bankrom(8, 0xE000, mmc_getinfo()->rom_banks * 2 - 1);
   mmc_bankvrom(1, 0x0000, 0);
   mmc_bankvrom(1, 0x0400, 1);
   mmc_bankvrom(1, 0x0800, 2);
   mmc_bankvrom(1, 0x0C00, 3);
   mmc_bankvrom(1, 0x1000, 4);
   mmc_bankvrom(1, 0x1400, 5);
   mmc_bankvrom(1, 0x1800, 6);
   mmc_bankvrom(1, 0x1C00, 7);
}

static const map_memwrite map76_memwrite[] =
{
   { 0x8000, 0xFFFF, map76_write },
   {     -1,     -1, NULL }
};

const mapintf_t map76_intf =
{
   76,           /* mapper number */
   "Namcot 109", /* mapper name */
   map76_init,   /* init */
   NULL,         /* vblank */
   NULL,         /* hblank */
   NULL, NULL, NULL,
   map76_memwrite,
   NULL
};
