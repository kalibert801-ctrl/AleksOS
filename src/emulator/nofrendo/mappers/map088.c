/*
** map088.c
** Mapper 88 — Namco 118
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint8 reg88;

static void map88_write(uint32 address, uint8 value)
{
   switch (address)
   {
   case 0x8000:
      reg88 = value;
      break;

   case 0x8001:
      switch (reg88 & 0x07)
      {
      case 0x00:
         mmc_bankvrom(1, 0x0000, (value & 0xFE) + 0);
         mmc_bankvrom(1, 0x0400, (value & 0xFE) + 1);
         break;
      case 0x01:
         mmc_bankvrom(1, 0x0800, (value & 0xFE) + 0);
         mmc_bankvrom(1, 0x0C00, (value & 0xFE) + 1);
         break;
      case 0x02:
         mmc_bankvrom(1, 0x1000, value + 0x40);
         break;
      case 0x03:
         mmc_bankvrom(1, 0x1400, value + 0x40);
         break;
      case 0x04:
         mmc_bankvrom(1, 0x1800, value + 0x40);
         break;
      case 0x05:
         mmc_bankvrom(1, 0x1C00, value + 0x40);
         break;
      case 0x06:
         mmc_bankrom(8, 0x8000, value);
         break;
      case 0x07:
         mmc_bankrom(8, 0xA000, value);
         break;
      }
      break;

   case 0xC000:
      if (value) ppu_mirror(0, 0, 0, 0); /* one-screen NT0 */
      else       ppu_mirror(1, 1, 1, 1); /* one-screen NT1 */
      break;
   }
}

static void map88_init(void)
{
   reg88 = 0;
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

static const map_memwrite map88_memwrite[] =
{
   { 0x8000, 0xFFFF, map88_write },
   {     -1,     -1, NULL }
};

const mapintf_t map88_intf =
{
   88,           /* mapper number */
   "Namco 118",  /* mapper name */
   map88_init,   /* init */
   NULL,         /* vblank */
   NULL,         /* hblank */
   NULL, NULL, NULL,
   map88_memwrite,
   NULL
};
