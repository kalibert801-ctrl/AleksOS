/*
** map091.c
** Mapper 91 — Street Fighter / Pirates (J.Y. Company)
**
** CHR banking via $6000-$6003 writes (2KB pages, 8 total 1KB slots).
** PRG banking via $7000-$7001 writes (8KB pages at $8000/$A000).
** $C000/$E000 are fixed to the last two 8KB banks.
**
** Address masking: (addr & 0xF00F) is used to decode registers, so
** sub-page addresses in each 16-byte aligned block all alias to the
** base address (0x6000, 0x6001, etc.).
**
** Mirroring: vertical (fixed).
** No IRQ on this mapper.
*/

#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

/* ------------------------------------------------------------------ */
/* SRAM write ($6000-$7FFF) — CHR and PRG bank select                  */
/* ------------------------------------------------------------------ */
static void map91_sram_write(uint32 address, uint8 value)
{
   switch (address & 0xF00F)
   {
   /* CHR 2KB banks */
   case 0x6000:
      mmc_bankvrom(1, 0x0000, value * 2 + 0);
      mmc_bankvrom(1, 0x0400, value * 2 + 1);
      break;

   case 0x6001:
      mmc_bankvrom(1, 0x0800, value * 2 + 0);
      mmc_bankvrom(1, 0x0C00, value * 2 + 1);
      break;

   case 0x6002:
      mmc_bankvrom(1, 0x1000, value * 2 + 0);
      mmc_bankvrom(1, 0x1400, value * 2 + 1);
      break;

   case 0x6003:
      mmc_bankvrom(1, 0x1800, value * 2 + 0);
      mmc_bankvrom(1, 0x1C00, value * 2 + 1);
      break;

   /* PRG 8KB banks */
   case 0x7000:
      mmc_bankrom(8, 0x8000, value);
      break;

   case 0x7001:
      mmc_bankrom(8, 0xA000, value);
      break;
   }
}

/* ------------------------------------------------------------------ */
/* Init                                                                  */
/* ------------------------------------------------------------------ */
static void map91_init(void)
{
   int i;
   int last = mmc_getinfo()->rom_banks * 2 - 1;

   /* $8000/$A000: last two banks (swappable), $C000/$E000: fixed last two */
   mmc_bankrom(8, 0x8000, last - 1);
   mmc_bankrom(8, 0xA000, last);
   mmc_bankrom(8, 0xC000, last - 1);
   mmc_bankrom(8, 0xE000, last);

   ppu_mirror(0, 1, 0, 1); /* vertical */

   if (mmc_getinfo()->vrom_banks > 0)
   {
      for (i = 0; i < 8; i++)
         mmc_bankvrom(1, i * 0x400, i);
   }
}

static const map_memwrite map91_memwrite[] =
{
   { 0x6000, 0x7FFF, map91_sram_write },
   {     -1,     -1, NULL }
};

const mapintf_t map91_intf =
{
   91,            /* mapper number */
   "Street Fighter",/* mapper name */
   map91_init,    /* init */
   NULL,          /* vblank */
   NULL,          /* hblank */
   NULL, NULL, NULL,
   map91_memwrite,
   NULL
};
