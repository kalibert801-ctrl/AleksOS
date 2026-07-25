/*
** map096.c
** Mapper 96 — Bandai 74161 (CHR RAM)
** Note: original uses PPU address callback to select CHR bank; omitted here.
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint8 reg96_upper; /* high bit of CHR bank from write */

static void map96_write(uint32 address, uint8 value)
{
   UNUSED(address);

   mmc_bankrom(8, 0x8000, ((value & 0x03) << 2) + 0);
   mmc_bankrom(8, 0xA000, ((value & 0x03) << 2) + 1);
   mmc_bankrom(8, 0xC000, ((value & 0x03) << 2) + 2);
   mmc_bankrom(8, 0xE000, ((value & 0x03) << 2) + 3);

   reg96_upper = (value & 0x04) >> 2;

   /* CHR RAM page: bank from upper flag, slot from reg96_upper*4 */
   mmc_bankvrom(1, 0x0000, reg96_upper * 4 + 0);
   mmc_bankvrom(1, 0x0400, reg96_upper * 4 + 1);
   mmc_bankvrom(1, 0x0800, reg96_upper * 4 + 2);
   mmc_bankvrom(1, 0x0C00, reg96_upper * 4 + 3);
   /* upper half fixed at the end of that block */
   mmc_bankvrom(1, 0x1000, reg96_upper * 4 + 12);
   mmc_bankvrom(1, 0x1400, reg96_upper * 4 + 13);
   mmc_bankvrom(1, 0x1800, reg96_upper * 4 + 14);
   mmc_bankvrom(1, 0x1C00, reg96_upper * 4 + 15);
}

static void map96_init(void)
{
   reg96_upper = 0;
   mmc_bankrom(8, 0x8000, 0);
   mmc_bankrom(8, 0xA000, 1);
   mmc_bankrom(8, 0xC000, 2);
   mmc_bankrom(8, 0xE000, 3);
   mmc_bankvrom(1, 0x0000, 0);
   mmc_bankvrom(1, 0x0400, 1);
   mmc_bankvrom(1, 0x0800, 2);
   mmc_bankvrom(1, 0x0C00, 3);
   mmc_bankvrom(1, 0x1000, 12);
   mmc_bankvrom(1, 0x1400, 13);
   mmc_bankvrom(1, 0x1800, 14);
   mmc_bankvrom(1, 0x1C00, 15);
   ppu_mirror(1, 1, 1, 1); /* NT1 */
}

static const map_memwrite map96_memwrite[] =
{
   { 0x8000, 0xFFFF, map96_write },
   {     -1,     -1, NULL }
};

const mapintf_t map96_intf =
{
   96,               /* mapper number */
   "Bandai 74161",   /* mapper name */
   map96_init,       /* init */
   NULL,             /* vblank */
   NULL,             /* hblank */
   NULL, NULL, NULL,
   map96_memwrite,
   NULL
};
