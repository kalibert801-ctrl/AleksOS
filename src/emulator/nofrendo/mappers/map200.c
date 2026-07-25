/*
** map200.c
** Mapper 200 — 1200-in-1
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static void map200_write(uint32 address, uint8 value)
{
   uint8 prg = (address & 0x07) << 1;
   uint8 chr = (address & 0x07) << 3;

   UNUSED(value);

   mmc_bankrom(8, 0x8000, prg + 0);
   mmc_bankrom(8, 0xA000, prg + 1);
   mmc_bankrom(8, 0xC000, prg + 0);
   mmc_bankrom(8, 0xE000, prg + 1);

   mmc_bankvrom(1, 0x0000, chr + 0);
   mmc_bankvrom(1, 0x0400, chr + 1);
   mmc_bankvrom(1, 0x0800, chr + 2);
   mmc_bankvrom(1, 0x0C00, chr + 3);
   mmc_bankvrom(1, 0x1000, chr + 4);
   mmc_bankvrom(1, 0x1400, chr + 5);
   mmc_bankvrom(1, 0x1800, chr + 6);
   mmc_bankvrom(1, 0x1C00, chr + 7);

   if (address & 0x01)
      ppu_mirror(0, 1, 0, 1); /* vertical */
   else
      ppu_mirror(0, 0, 1, 1); /* horizontal */
}

static void map200_init(void)
{
   mmc_bankrom(8, 0x8000, 0);
   mmc_bankrom(8, 0xA000, 1);
   mmc_bankrom(8, 0xC000, 0);
   mmc_bankrom(8, 0xE000, 1);
   mmc_bankvrom(1, 0x0000, 0);
   mmc_bankvrom(1, 0x0400, 1);
   mmc_bankvrom(1, 0x0800, 2);
   mmc_bankvrom(1, 0x0C00, 3);
   mmc_bankvrom(1, 0x1000, 4);
   mmc_bankvrom(1, 0x1400, 5);
   mmc_bankvrom(1, 0x1800, 6);
   mmc_bankvrom(1, 0x1C00, 7);
}

static const map_memwrite map200_memwrite[] =
{
   { 0x8000, 0xFFFF, map200_write },
   {     -1,     -1, NULL }
};

const mapintf_t map200_intf =
{
   200,           /* mapper number */
   "1200-in-1",   /* mapper name */
   map200_init,   /* init */
   NULL,          /* vblank */
   NULL,          /* hblank */
   NULL, NULL, NULL,
   map200_memwrite,
   NULL
};
