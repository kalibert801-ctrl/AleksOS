/*
** map230.c
** Mapper 230 — 22-in-1
** Toggle-based ROM switch: alternates mode on each reset/init call.
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint8 reg230_rom_sw;

static void map230_write(uint32 address, uint8 value)
{
   UNUSED(address);

   if (reg230_rom_sw)
   {
      mmc_bankrom(8, 0x8000, ((value & 0x07) << 1) + 0);
      mmc_bankrom(8, 0xA000, ((value & 0x07) << 1) + 1);
   }
   else
   {
      if (value & 0x20)
      {
         mmc_bankrom(8, 0x8000, ((value & 0x1F) << 1) + 16);
         mmc_bankrom(8, 0xA000, ((value & 0x1F) << 1) + 17);
         mmc_bankrom(8, 0xC000, ((value & 0x1F) << 1) + 16);
         mmc_bankrom(8, 0xE000, ((value & 0x1F) << 1) + 17);
      }
      else
      {
         mmc_bankrom(8, 0x8000, ((value & 0x1E) << 1) + 16);
         mmc_bankrom(8, 0xA000, ((value & 0x1E) << 1) + 17);
         mmc_bankrom(8, 0xC000, ((value & 0x1E) << 1) + 18);
         mmc_bankrom(8, 0xE000, ((value & 0x1E) << 1) + 19);
      }

      if (value & 0x40)
         ppu_mirror(0, 1, 0, 1); /* vertical */
      else
         ppu_mirror(0, 0, 1, 1); /* horizontal */
   }
}

static void map230_init(void)
{
   int last = mmc_getinfo()->rom_banks * 2 - 1;

   /* toggle mode on each init */
   reg230_rom_sw ^= 1;

   if (reg230_rom_sw)
   {
      mmc_bankrom(8, 0x8000,  0);
      mmc_bankrom(8, 0xA000,  1);
      mmc_bankrom(8, 0xC000, 14);
      mmc_bankrom(8, 0xE000, 15);
   }
   else
   {
      mmc_bankrom(8, 0x8000, 16);
      mmc_bankrom(8, 0xA000, 17);
      mmc_bankrom(8, 0xC000, last - 1);
      mmc_bankrom(8, 0xE000, last);
   }
}

static const map_memwrite map230_memwrite[] =
{
   { 0x8000, 0xFFFF, map230_write },
   {     -1,     -1, NULL }
};

const mapintf_t map230_intf =
{
   230,          /* mapper number */
   "22-in-1",    /* mapper name */
   map230_init,  /* init */
   NULL,         /* vblank */
   NULL,         /* hblank */
   NULL, NULL, NULL,
   map230_memwrite,
   NULL
};
