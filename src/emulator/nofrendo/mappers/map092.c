/*
** map092.c
** Mapper 92 — Jaleco Early Mapper
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static void map92_write(uint32 address, uint8 value)
{
   uint8 data = (uint8)address & 0xFF;
   uint8 prg  = (data & 0x0F) << 1;
   uint8 chr  = data & 0x0F;

   if (address >= 0x9000)
   {
      switch (data & 0xF0)
      {
      case 0xD0:
         mmc_bankrom(8, 0xC000, prg + 0);
         mmc_bankrom(8, 0xE000, prg + 1);
         break;
      case 0xE0:
         chr <<= 3;
         mmc_bankvrom(1, 0x0000, chr + 0);
         mmc_bankvrom(1, 0x0400, chr + 1);
         mmc_bankvrom(1, 0x0800, chr + 2);
         mmc_bankvrom(1, 0x0C00, chr + 3);
         mmc_bankvrom(1, 0x1000, chr + 4);
         mmc_bankvrom(1, 0x1400, chr + 5);
         mmc_bankvrom(1, 0x1800, chr + 6);
         mmc_bankvrom(1, 0x1C00, chr + 7);
         break;
      }
   }
   else
   {
      switch (data & 0xF0)
      {
      case 0xB0:
         mmc_bankrom(8, 0xC000, prg + 0);
         mmc_bankrom(8, 0xE000, prg + 1);
         break;
      case 0x70:
         chr <<= 3;
         mmc_bankvrom(1, 0x0000, chr + 0);
         mmc_bankvrom(1, 0x0400, chr + 1);
         mmc_bankvrom(1, 0x0800, chr + 2);
         mmc_bankvrom(1, 0x0C00, chr + 3);
         mmc_bankvrom(1, 0x1000, chr + 4);
         mmc_bankvrom(1, 0x1400, chr + 5);
         mmc_bankvrom(1, 0x1800, chr + 6);
         mmc_bankvrom(1, 0x1C00, chr + 7);
         break;
      }
   }

   UNUSED(value);
}

static void map92_init(void)
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

static const map_memwrite map92_memwrite[] =
{
   { 0x8000, 0xFFFF, map92_write },
   {     -1,     -1, NULL }
};

const mapintf_t map92_intf =
{
   92,              /* mapper number */
   "Jaleco Early",  /* mapper name */
   map92_init,      /* init */
   NULL,            /* vblank */
   NULL,            /* hblank */
   NULL, NULL, NULL,
   map92_memwrite,
   NULL
};
