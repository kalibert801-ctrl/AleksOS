/*
** map060.c
** Mapper 60 — Multi-game pirate board
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static void map60_write(uint32 address, uint8 value)
{
   uint8 prg = (address & 0x70) >> 3;

   if (address & 0x80)
   {
      mmc_bankrom(8, 0x8000, prg + 0);
      mmc_bankrom(8, 0xA000, prg + 1);
      mmc_bankrom(8, 0xC000, prg + 0);
      mmc_bankrom(8, 0xE000, prg + 1);
   }
   else
   {
      mmc_bankrom(8, 0x8000, prg + 0);
      mmc_bankrom(8, 0xA000, prg + 1);
      mmc_bankrom(8, 0xC000, prg + 2);
      mmc_bankrom(8, 0xE000, prg + 3);
   }

   uint8 chr = (address & 0x07) << 3;
   mmc_bankvrom(1, 0x0000, chr + 0);
   mmc_bankvrom(1, 0x0400, chr + 1);
   mmc_bankvrom(1, 0x0800, chr + 2);
   mmc_bankvrom(1, 0x0C00, chr + 3);
   mmc_bankvrom(1, 0x1000, chr + 4);
   mmc_bankvrom(1, 0x1400, chr + 5);
   mmc_bankvrom(1, 0x1800, chr + 6);
   mmc_bankvrom(1, 0x1C00, chr + 7);

   if (value & 0x08) ppu_mirror(0, 0, 1, 1); /* horizontal */
   else              ppu_mirror(0, 1, 0, 1); /* vertical */
}

static void map60_init(void)
{
   mmc_bankrom(8, 0x8000, 0);
   mmc_bankrom(8, 0xA000, 1);
   mmc_bankrom(8, 0xC000, 2);
   mmc_bankrom(8, 0xE000, 3);
}

static const map_memwrite map60_memwrite[] =
{
   { 0x8000, 0xFFFF, map60_write },
   {     -1,     -1, NULL }
};

const mapintf_t map60_intf =
{
   60,           /* mapper number */
   "60",         /* mapper name */
   map60_init,   /* init */
   NULL,         /* vblank */
   NULL,         /* hblank */
   NULL, NULL, NULL,
   map60_memwrite,
   NULL
};
