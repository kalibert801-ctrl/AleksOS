/*
** map202.c
** Mapper 202 — 150-in-1
** APU and Write handlers share the same logic; use $4100-$FFFF range.
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static void map202_write(uint32 address, uint8 value)
{
   uint8 bank = (address >> 1) & 0x07;
   uint8 chr  = bank << 3;

   UNUSED(value);

   mmc_bankrom(8, 0x8000, (bank << 1) + 0);
   mmc_bankrom(8, 0xA000, (bank << 1) + 1);

   if ((address & 0x0C) == 0x0C)
   {
      mmc_bankrom(8, 0xC000, ((bank + 1) << 1) + 0);
      mmc_bankrom(8, 0xE000, ((bank + 1) << 1) + 1);
   }
   else
   {
      mmc_bankrom(8, 0xC000, (bank << 1) + 0);
      mmc_bankrom(8, 0xE000, (bank << 1) + 1);
   }

   mmc_bankvrom(1, 0x0000, chr + 0);
   mmc_bankvrom(1, 0x0400, chr + 1);
   mmc_bankvrom(1, 0x0800, chr + 2);
   mmc_bankvrom(1, 0x0C00, chr + 3);
   mmc_bankvrom(1, 0x1000, chr + 4);
   mmc_bankvrom(1, 0x1400, chr + 5);
   mmc_bankvrom(1, 0x1800, chr + 6);
   mmc_bankvrom(1, 0x1C00, chr + 7);

   if (address & 0x01)
      ppu_mirror(0, 0, 1, 1); /* horizontal */
   else
      ppu_mirror(0, 1, 0, 1); /* vertical */
}

static void map202_init(void)
{
   mmc_bankrom(8, 0x8000, 12);
   mmc_bankrom(8, 0xA000, 13);
   mmc_bankrom(8, 0xC000, 14);
   mmc_bankrom(8, 0xE000, 15);
   mmc_bankvrom(1, 0x0000, 0);
   mmc_bankvrom(1, 0x0400, 1);
   mmc_bankvrom(1, 0x0800, 2);
   mmc_bankvrom(1, 0x0C00, 3);
   mmc_bankvrom(1, 0x1000, 4);
   mmc_bankvrom(1, 0x1400, 5);
   mmc_bankvrom(1, 0x1800, 6);
   mmc_bankvrom(1, 0x1C00, 7);
}

static const map_memwrite map202_memwrite[] =
{
   { 0x4100, 0xFFFF, map202_write },
   {     -1,     -1, NULL }
};

const mapintf_t map202_intf =
{
   202,          /* mapper number */
   "150-in-1",   /* mapper name */
   map202_init,  /* init */
   NULL,         /* vblank */
   NULL,         /* hblank */
   NULL, NULL, NULL,
   map202_memwrite,
   NULL
};
