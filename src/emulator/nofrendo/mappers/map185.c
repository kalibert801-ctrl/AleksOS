/*
** map185.c
** Mapper 185 — Tecmo (CHR enable/disable)
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static void map185_write(uint32 address, uint8 value)
{
   UNUSED(address);

   if (value & 0x03)
   {
      /* CHR ROM enabled — map banks 0-7 normally */
      mmc_bankvrom(1, 0x0000, 0);
      mmc_bankvrom(1, 0x0400, 1);
      mmc_bankvrom(1, 0x0800, 2);
      mmc_bankvrom(1, 0x0C00, 3);
      mmc_bankvrom(1, 0x1000, 4);
      mmc_bankvrom(1, 0x1400, 5);
      mmc_bankvrom(1, 0x1800, 6);
      mmc_bankvrom(1, 0x1C00, 7);
   }
   else
   {
      /* CHR ROM disabled — map all to bank 0 (blank) */
      mmc_bankvrom(1, 0x0000, 0);
      mmc_bankvrom(1, 0x0400, 0);
      mmc_bankvrom(1, 0x0800, 0);
      mmc_bankvrom(1, 0x0C00, 0);
      mmc_bankvrom(1, 0x1000, 0);
      mmc_bankvrom(1, 0x1400, 0);
      mmc_bankvrom(1, 0x1800, 0);
      mmc_bankvrom(1, 0x1C00, 0);
   }
}

static void map185_init(void)
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

static const map_memwrite map185_memwrite[] =
{
   { 0x8000, 0xFFFF, map185_write },
   {     -1,     -1, NULL }
};

const mapintf_t map185_intf =
{
   185,          /* mapper number */
   "Tecmo",      /* mapper name */
   map185_init,  /* init */
   NULL,         /* vblank */
   NULL,         /* hblank */
   NULL, NULL, NULL,
   map185_memwrite,
   NULL
};
