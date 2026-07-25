/*
** map097.c
** Mapper 97 — Irem 74161/32
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static void map97_write(uint32 address, uint8 value)
{
   if (address < 0xC000)
   {
      uint8 prg = (value & 0x0F) << 1;

      mmc_bankrom(8, 0xC000, prg + 0);
      mmc_bankrom(8, 0xE000, prg + 1);

      if ((value & 0x80) == 0) ppu_mirror(0, 0, 1, 1); /* horizontal */
      else                     ppu_mirror(0, 1, 0, 1); /* vertical */
   }
}

static void map97_init(void)
{
   mmc_bankrom(8, 0x8000, mmc_getinfo()->rom_banks * 2 - 2);
   mmc_bankrom(8, 0xA000, mmc_getinfo()->rom_banks * 2 - 1);
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

static const map_memwrite map97_memwrite[] =
{
   { 0x8000, 0xFFFF, map97_write },
   {     -1,     -1, NULL }
};

const mapintf_t map97_intf =
{
   97,              /* mapper number */
   "Irem 74161",    /* mapper name */
   map97_init,      /* init */
   NULL,            /* vblank */
   NULL,            /* hblank */
   NULL, NULL, NULL,
   map97_memwrite,
   NULL
};
