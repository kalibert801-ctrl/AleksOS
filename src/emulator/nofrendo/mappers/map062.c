/*
** map062.c
** Mapper 62 — Multi-game pirate board
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static void map62_write(uint32 address, uint8 value)
{
   switch (address & 0xFF00)
   {
   case 0x8100:
      mmc_bankrom(8, 0x8000, value + 0);
      mmc_bankrom(8, 0xA000, value + 1);
      break;
   case 0x8500:
      mmc_bankrom(8, 0x8000, value);
      break;
   case 0x8700:
      mmc_bankrom(8, 0xA000, value);
      break;
   default:
      mmc_bankvrom(1, 0x0000, value + 0);
      mmc_bankvrom(1, 0x0400, value + 1);
      mmc_bankvrom(1, 0x0800, value + 2);
      mmc_bankvrom(1, 0x0C00, value + 3);
      mmc_bankvrom(1, 0x1000, value + 4);
      mmc_bankvrom(1, 0x1400, value + 5);
      mmc_bankvrom(1, 0x1800, value + 6);
      mmc_bankvrom(1, 0x1C00, value + 7);
      break;
   }
}

static void map62_init(void)
{
   mmc_bankrom(8, 0x8000, 0);
   mmc_bankrom(8, 0xA000, 1);
   mmc_bankrom(8, 0xC000, 2);
   mmc_bankrom(8, 0xE000, 3);
}

static const map_memwrite map62_memwrite[] =
{
   { 0x8000, 0xFFFF, map62_write },
   {     -1,     -1, NULL }
};

const mapintf_t map62_intf =
{
   62,           /* mapper number */
   "62",         /* mapper name */
   map62_init,   /* init */
   NULL,         /* vblank */
   NULL,         /* hblank */
   NULL, NULL, NULL,
   map62_memwrite,
   NULL
};
