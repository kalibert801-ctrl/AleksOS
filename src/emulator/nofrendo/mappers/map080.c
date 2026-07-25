/*
** map080.c
** Mapper 80 — Taito X1-005
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static void map080_write(uint32 address, uint8 value)
{
   switch (address)
   {
   case 0x7EF0:
      value &= 0x7F;
      mmc_bankvrom(1, 0x0000, value + 0);
      mmc_bankvrom(1, 0x0400, value + 1);
      break;
   case 0x7EF1:
      value &= 0x7F;
      mmc_bankvrom(1, 0x0800, value + 0);
      mmc_bankvrom(1, 0x0C00, value + 1);
      break;
   case 0x7EF2:
      mmc_bankvrom(1, 0x1000, value);
      break;
   case 0x7EF3:
      mmc_bankvrom(1, 0x1400, value);
      break;
   case 0x7EF4:
      mmc_bankvrom(1, 0x1800, value);
      break;
   case 0x7EF5:
      mmc_bankvrom(1, 0x1C00, value);
      break;
   case 0x7EF6:
      if (value & 0x01)
         ppu_mirror(0, 1, 0, 1); /* vertical */
      else
         ppu_mirror(0, 0, 1, 1); /* horizontal */
      /* fall through to ROM bank */
   case 0x7EFA:
   case 0x7EFB:
      mmc_bankrom(8, 0x8000, value);
      break;
   case 0x7EFC:
   case 0x7EFD:
      mmc_bankrom(8, 0xA000, value);
      break;
   case 0x7EFE:
   case 0x7EFF:
      mmc_bankrom(8, 0xC000, value);
      break;
   }
}

static void map080_init(void)
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

static const map_memwrite map080_memwrite[] =
{
   { 0x6000, 0x7FFF, map080_write },
   {     -1,     -1, NULL }
};

const mapintf_t map80_intf =
{
   80,              /* mapper number */
   "Taito X1-005",  /* mapper name */
   map080_init,     /* init */
   NULL,            /* vblank */
   NULL,            /* hblank */
   NULL, NULL, NULL,
   map080_memwrite,
   NULL
};
