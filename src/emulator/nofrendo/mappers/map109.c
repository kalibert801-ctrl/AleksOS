/*
** map109.c
** Mapper 109 — SACHEN SA-019 (The Great Wall)
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint8 reg109;
static uint8 chr0, chr1, chr2, chr3;
static uint8 chrmode0, chrmode1;

static void map109_set_ppu(void)
{
   mmc_bankvrom(1, 0x0000, chr0);
   mmc_bankvrom(1, 0x0400, chr1 | ((chrmode1 << 3) & 0x8));
   mmc_bankvrom(1, 0x0800, chr2 | ((chrmode1 << 2) & 0x8));
   mmc_bankvrom(1, 0x0C00, chr3 | ((chrmode1 << 1) & 0x8) | (chrmode0 * 0x10));
   mmc_bankvrom(1, 0x1000, mmc_getinfo()->vrom_banks * 8 - 4);
   mmc_bankvrom(1, 0x1400, mmc_getinfo()->vrom_banks * 8 - 3);
   mmc_bankvrom(1, 0x1800, mmc_getinfo()->vrom_banks * 8 - 2);
   mmc_bankvrom(1, 0x1C00, mmc_getinfo()->vrom_banks * 8 - 1);
}

static void map109_write(uint32 address, uint8 value)
{
   switch (address)
   {
   case 0x4100:
      reg109 = value;
      break;
   case 0x4101:
      switch (reg109)
      {
      case 0: chr0 = value;            map109_set_ppu(); break;
      case 1: chr1 = value;            map109_set_ppu(); break;
      case 2: chr2 = value;            map109_set_ppu(); break;
      case 3: chr3 = value;            map109_set_ppu(); break;
      case 4: chrmode0 = value & 0x01; map109_set_ppu(); break;
      case 5:
         mmc_bankrom(8, 0x8000, ((value & 0x07) + 0));
         mmc_bankrom(8, 0xA000, ((value & 0x07) + 1));
         mmc_bankrom(8, 0xC000, ((value & 0x07) + 2));
         mmc_bankrom(8, 0xE000, ((value & 0x07) + 3));
         break;
      case 6: chrmode1 = value & 0x07; map109_set_ppu(); break;
      case 7:
         if (value & 0x01) ppu_mirror(0, 0, 1, 1); /* horizontal */
         else              ppu_mirror(0, 1, 0, 1); /* vertical */
         break;
      }
      break;
   }
}

static void map109_init(void)
{
   reg109 = chr0 = chr1 = chr2 = chr3 = 0;
   chrmode0 = chrmode1 = 0;
   mmc_bankrom(8, 0x8000, 0);
   mmc_bankrom(8, 0xA000, 1);
   mmc_bankrom(8, 0xC000, 2);
   mmc_bankrom(8, 0xE000, 3);
   map109_set_ppu();
}

static const map_memwrite map109_memwrite[] =
{
   { 0x4100, 0x4FFF, map109_write },
   {     -1,     -1, NULL }
};

const mapintf_t map109_intf =
{
   109,            /* mapper number */
   "SACHEN SA-019",/* mapper name */
   map109_init,    /* init */
   NULL,           /* vblank */
   NULL,           /* hblank */
   NULL, NULL, NULL,
   map109_memwrite,
   NULL
};
