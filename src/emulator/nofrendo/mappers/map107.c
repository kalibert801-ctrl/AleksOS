/*
** map107.c
** Mapper 107 — Magic Dragon
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static void map107_write(uint32 address, uint8 value)
{
   uint8 prg = ((value >> 1) & 0x03) << 2;
   uint8 chr = (value & 0x07) << 3;

   mmc_bankrom(8, 0x8000, prg + 0);
   mmc_bankrom(8, 0xA000, prg + 1);
   mmc_bankrom(8, 0xC000, prg + 2);
   mmc_bankrom(8, 0xE000, prg + 3);

   mmc_bankvrom(1, 0x0000, chr + 0);
   mmc_bankvrom(1, 0x0400, chr + 1);
   mmc_bankvrom(1, 0x0800, chr + 2);
   mmc_bankvrom(1, 0x0C00, chr + 3);
   mmc_bankvrom(1, 0x1000, chr + 4);
   mmc_bankvrom(1, 0x1400, chr + 5);
   mmc_bankvrom(1, 0x1800, chr + 6);
   mmc_bankvrom(1, 0x1C00, chr + 7);

   UNUSED(address);
}

static void map107_init(void)
{
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

static const map_memwrite map107_memwrite[] =
{
   { 0x8000, 0xFFFF, map107_write },
   {     -1,     -1, NULL }
};

const mapintf_t map107_intf =
{
   107,              /* mapper number */
   "Magic Dragon",   /* mapper name */
   map107_init,      /* init */
   NULL,             /* vblank */
   NULL,             /* hblank */
   NULL, NULL, NULL,
   map107_memwrite,
   NULL
};
