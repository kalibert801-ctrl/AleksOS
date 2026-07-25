/*
** map233.c
** Mapper 233 — 42-in-1
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static void map233_write(uint32 address, uint8 value)
{
   int total = mmc_getinfo()->rom_banks * 2;
   int bank;
   if (value & 0x20) {
      bank = (value & 0x1F) << 1;
      mmc_bankrom(8, 0x8000, bank     % total);
      mmc_bankrom(8, 0xA000, (bank+1) % total);
      mmc_bankrom(8, 0xC000, bank     % total);
      mmc_bankrom(8, 0xE000, (bank+1) % total);
   } else {
      bank = (value & 0x1E) << 1;
      mmc_bankrom(8, 0x8000, bank     % total);
      mmc_bankrom(8, 0xA000, (bank+1) % total);
      mmc_bankrom(8, 0xC000, (bank+2) % total);
      mmc_bankrom(8, 0xE000, (bank+3) % total);
   }
   switch (value & 0xC0) {
   case 0x40: ppu_mirror(0,1,0,1); break;
   case 0x80: ppu_mirror(0,0,1,1); break;
   case 0xC0: ppu_mirror(0,0,0,0); break;
   default:   ppu_mirror(0,1,0,1); break;
   }
}

static void map233_init(void)
{
   mmc_bankrom(8, 0x8000, 0);
   mmc_bankrom(8, 0xA000, 1);
   mmc_bankrom(8, 0xC000, 2);
   mmc_bankrom(8, 0xE000, 3);
}

static const map_memwrite map233_memwrite[] =
{
   { 0x8000, 0xFFFF, map233_write },
   {     -1,     -1, NULL }
};

const mapintf_t map233_intf =
{
   233,         /* mapper number */
   "42-in-1",   /* mapper name */
   map233_init, /* init */
   NULL, NULL, NULL, NULL, NULL,
   map233_memwrite,
   NULL
};
