/*
** map061.c
** Mapper 61 — Multi-game pirate board
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static void map61_write(uint32 address, uint8 value)
{
   uint8 bank;

   switch (address & 0x30)
   {
   case 0x00:
   case 0x30:
      mmc_bankrom(8, 0x8000, ((address & 0x0F) << 2) + 0);
      mmc_bankrom(8, 0xA000, ((address & 0x0F) << 2) + 1);
      mmc_bankrom(8, 0xC000, ((address & 0x0F) << 2) + 2);
      mmc_bankrom(8, 0xE000, ((address & 0x0F) << 2) + 3);
      break;
   case 0x10:
   case 0x20:
      bank = ((address & 0x0F) << 1) | ((address & 0x20) >> 4);
      mmc_bankrom(8, 0x8000, (bank << 1) + 0);
      mmc_bankrom(8, 0xA000, (bank << 1) + 1);
      mmc_bankrom(8, 0xC000, (bank << 1) + 0);
      mmc_bankrom(8, 0xE000, (bank << 1) + 1);
      break;
   }

   if (address & 0x80) ppu_mirror(0, 0, 1, 1); /* horizontal */
   else                ppu_mirror(0, 1, 0, 1); /* vertical */

   UNUSED(value);
}

static void map61_init(void)
{
   mmc_bankrom(8, 0x8000, 0);
   mmc_bankrom(8, 0xA000, 1);
   mmc_bankrom(8, 0xC000, mmc_getinfo()->rom_banks * 2 - 2);
   mmc_bankrom(8, 0xE000, mmc_getinfo()->rom_banks * 2 - 1);
}

static const map_memwrite map61_memwrite[] =
{
   { 0x8000, 0xFFFF, map61_write },
   {     -1,     -1, NULL }
};

const mapintf_t map61_intf =
{
   61,           /* mapper number */
   "61",         /* mapper name */
   map61_init,   /* init */
   NULL,         /* vblank */
   NULL,         /* hblank */
   NULL, NULL, NULL,
   map61_memwrite,
   NULL
};
