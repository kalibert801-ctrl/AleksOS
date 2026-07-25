/*
** map236.c
** Mapper 236 — 800-in-1 multicart
**
** Two write regions control two separate register fields:
**   $8000-$BFFF: upper nibble of bank (bits 4-5)
**   $C000-$FFFF: lower nibble of bank (bits 0-2) + mode (bits 4-5)
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint8 bank236, mode236;

static void map236_write(uint32 address, uint8 value)
{
   (void)value;

   if (address <= 0xBFFF) {
      bank236 = (uint8)(((address & 0x03) << 4) | (bank236 & 0x07));
   } else {
      bank236 = (uint8)((address & 0x07) | (bank236 & 0x30));
      mode236 = (uint8)(address & 0x30);
   }

   if (address & 0x20) ppu_mirror(0, 0, 1, 1); /* horizontal */
   else                ppu_mirror(0, 1, 0, 1); /* vertical   */

   switch (mode236) {
   case 0x00: {
      uint8 b = bank236 | 0x08;
      mmc_bankrom(8, 0x8000, (b << 1) + 0);
      mmc_bankrom(8, 0xA000, (b << 1) + 1);
      mmc_bankrom(8, 0xC000, ((b | 0x07) << 1) + 0);
      mmc_bankrom(8, 0xE000, ((b | 0x07) << 1) + 1);
      break;
   }
   case 0x10: {
      uint8 b = bank236 | 0x37;
      mmc_bankrom(8, 0x8000, (b << 1) + 0);
      mmc_bankrom(8, 0xA000, (b << 1) + 1);
      mmc_bankrom(8, 0xC000, ((b | 0x07) << 1) + 0);
      mmc_bankrom(8, 0xE000, ((b | 0x07) << 1) + 1);
      break;
   }
   case 0x20: {
      uint8 b = (bank236 | 0x08) & 0xFE;
      mmc_bankrom(8, 0x8000, (b << 1) + 0);
      mmc_bankrom(8, 0xA000, (b << 1) + 1);
      mmc_bankrom(8, 0xC000, (b << 1) + 2);
      mmc_bankrom(8, 0xE000, (b << 1) + 3);
      break;
   }
   case 0x30: {
      uint8 b = bank236 | 0x08;
      mmc_bankrom(8, 0x8000, (b << 1) + 0);
      mmc_bankrom(8, 0xA000, (b << 1) + 1);
      mmc_bankrom(8, 0xC000, (b << 1) + 0);
      mmc_bankrom(8, 0xE000, (b << 1) + 1);
      break;
   }
   }
}

static void map236_init(void)
{
   int last = mmc_getinfo()->rom_banks * 2 - 1;
   bank236 = 0;
   mode236 = 0;
   mmc_bankrom(8, 0x8000, 0);
   mmc_bankrom(8, 0xA000, 1);
   mmc_bankrom(8, 0xC000, last - 1);
   mmc_bankrom(8, 0xE000, last);
}

static const map_memwrite map236_memwrite[] =
{
   { 0x8000, 0xFFFF, map236_write },
   {     -1,     -1, NULL }
};

const mapintf_t map236_intf =
{
   236,          /* mapper number */
   "800-in-1",   /* mapper name */
   map236_init,  /* init */
   NULL,         /* vblank */
   NULL,         /* hblank */
   NULL, NULL, NULL,
   map236_memwrite,
   NULL
};
