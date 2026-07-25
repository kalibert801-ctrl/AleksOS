/*
** map181.c
** Mapper 181 — Hacker International Type 2
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static void map181_write(uint32 address, uint8 value)
{
   uint8 prg, chr;

   if (address != 0x4120)
      return;

   prg = (value & 0x08) >> 1; /* bits 3-0 of 8KB bank: 0 or 4 */
   chr = (value & 0x07) << 3; /* CHR 1KB base */

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
}

static void map181_init(void)
{
   mmc_bankrom(8, 0x8000, 0);
   mmc_bankrom(8, 0xA000, 1);
   mmc_bankrom(8, 0xC000, 2);
   mmc_bankrom(8, 0xE000, 3);
   mmc_bankvrom(1, 0x0000, 0);
   mmc_bankvrom(1, 0x0400, 1);
   mmc_bankvrom(1, 0x0800, 2);
   mmc_bankvrom(1, 0x0C00, 3);
   mmc_bankvrom(1, 0x1000, 4);
   mmc_bankvrom(1, 0x1400, 5);
   mmc_bankvrom(1, 0x1800, 6);
   mmc_bankvrom(1, 0x1C00, 7);
}

static const map_memwrite map181_memwrite[] =
{
   { 0x4100, 0x5FFF, map181_write },
   {     -1,     -1, NULL }
};

const mapintf_t map181_intf =
{
   181,                          /* mapper number */
   "Hacker International Type2", /* mapper name */
   map181_init,                  /* init */
   NULL,                         /* vblank */
   NULL,                         /* hblank */
   NULL, NULL, NULL,
   map181_memwrite,
   NULL
};
