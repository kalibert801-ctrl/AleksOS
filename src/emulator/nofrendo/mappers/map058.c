/*
** map058.c
** Mapper 58 — Multi-game pirate board
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static void map58_write(uint32 address, uint8 value)
{
   if (address & 0x40)
   {
      mmc_bankrom(8, 0x8000, ((address & 0x07) << 1) + 0);
      mmc_bankrom(8, 0xA000, ((address & 0x07) << 1) + 1);
      mmc_bankrom(8, 0xC000, ((address & 0x07) << 1) + 0);
      mmc_bankrom(8, 0xE000, ((address & 0x07) << 1) + 1);
   }
   else
   {
      mmc_bankrom(8, 0x8000, ((address & 0x06) << 1) + 0);
      mmc_bankrom(8, 0xA000, ((address & 0x06) << 1) + 1);
      mmc_bankrom(8, 0xC000, ((address & 0x06) << 1) + 2);
      mmc_bankrom(8, 0xE000, ((address & 0x06) << 1) + 3);
   }

   mmc_bankvrom(1, 0x0000, (address & 0x38) + 0);
   mmc_bankvrom(1, 0x0400, (address & 0x38) + 1);
   mmc_bankvrom(1, 0x0800, (address & 0x38) + 2);
   mmc_bankvrom(1, 0x0C00, (address & 0x38) + 3);
   mmc_bankvrom(1, 0x1000, (address & 0x38) + 4);
   mmc_bankvrom(1, 0x1400, (address & 0x38) + 5);
   mmc_bankvrom(1, 0x1800, (address & 0x38) + 6);
   mmc_bankvrom(1, 0x1C00, (address & 0x38) + 7);

   if (value & 0x02) ppu_mirror(0, 1, 0, 1); /* vertical */
   else              ppu_mirror(0, 0, 1, 1); /* horizontal */
}

static void map58_init(void)
{
   mmc_bankrom(8, 0x8000, 0);
   mmc_bankrom(8, 0xA000, 1);
   mmc_bankrom(8, 0xC000, 0);
   mmc_bankrom(8, 0xE000, 1);
}

static const map_memwrite map58_memwrite[] =
{
   { 0x8000, 0xFFFF, map58_write },
   {     -1,     -1, NULL }
};

const mapintf_t map58_intf =
{
   58,           /* mapper number */
   "58",         /* mapper name */
   map58_init,   /* init */
   NULL,         /* vblank */
   NULL,         /* hblank */
   NULL, NULL, NULL,
   map58_memwrite,
   NULL
};
