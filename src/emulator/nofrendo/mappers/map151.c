/*
** map151.c
** Mapper 151 — VS Unisystem
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static void map151_write(uint32 address, uint8 value)
{
   switch (address & 0xF000)
   {
   case 0x8000:
      mmc_bankrom(8, 0x8000, value);
      break;
   case 0xA000:
      mmc_bankrom(8, 0xA000, value);
      break;
   case 0xC000:
      mmc_bankrom(8, 0xC000, value);
      break;
   case 0xE000:
      mmc_bankvrom(1, 0x0000, value * 4 + 0);
      mmc_bankvrom(1, 0x0400, value * 4 + 1);
      mmc_bankvrom(1, 0x0800, value * 4 + 2);
      mmc_bankvrom(1, 0x0C00, value * 4 + 3);
      break;
   case 0xF000:
      mmc_bankvrom(1, 0x1000, value * 4 + 0);
      mmc_bankvrom(1, 0x1400, value * 4 + 1);
      mmc_bankvrom(1, 0x1800, value * 4 + 2);
      mmc_bankvrom(1, 0x1C00, value * 4 + 3);
      break;
   }
}

static void map151_init(void)
{
   int last = mmc_getinfo()->rom_banks * 2 - 1;
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

static const map_memwrite map151_memwrite[] =
{
   { 0x8000, 0xFFFF, map151_write },
   {     -1,     -1, NULL }
};

const mapintf_t map151_intf =
{
   151,                /* mapper number */
   "VS Unisystem",     /* mapper name */
   map151_init,        /* init */
   NULL,               /* vblank */
   NULL,               /* hblank */
   NULL, NULL, NULL,
   map151_memwrite,
   NULL
};
