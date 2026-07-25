/*
** map193.c
** Mapper 193 — MEGA SOFT (NTDEC) Fighting Hero
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static void map193_write(uint32 address, uint8 value)
{
   switch (address)
   {
   case 0x6000:
      mmc_bankvrom(1, 0x0000, (value & 0xFC) + 0);
      mmc_bankvrom(1, 0x0400, (value & 0xFC) + 1);
      mmc_bankvrom(1, 0x0800, (value & 0xFC) + 2);
      mmc_bankvrom(1, 0x0C00, (value & 0xFC) + 3);
      break;
   case 0x6001:
      mmc_bankvrom(1, 0x1000, value + 0);
      mmc_bankvrom(1, 0x1400, value + 1);
      break;
   case 0x6002:
      mmc_bankvrom(1, 0x1800, value + 0);
      mmc_bankvrom(1, 0x1C00, value + 1);
      break;
   case 0x6003:
      mmc_bankrom(8, 0x8000, (value << 2) + 0);
      mmc_bankrom(8, 0xA000, (value << 2) + 1);
      mmc_bankrom(8, 0xC000, (value << 2) + 2);
      mmc_bankrom(8, 0xE000, (value << 2) + 3);
      break;
   }
}

static void map193_init(void)
{
   int banks = mmc_getinfo()->rom_banks * 2;
   mmc_bankrom(8, 0x8000, banks - 4);
   mmc_bankrom(8, 0xA000, banks - 3);
   mmc_bankrom(8, 0xC000, banks - 2);
   mmc_bankrom(8, 0xE000, banks - 1);
   mmc_bankvrom(1, 0x0000, 0);
   mmc_bankvrom(1, 0x0400, 1);
   mmc_bankvrom(1, 0x0800, 2);
   mmc_bankvrom(1, 0x0C00, 3);
   mmc_bankvrom(1, 0x1000, 4);
   mmc_bankvrom(1, 0x1400, 5);
   mmc_bankvrom(1, 0x1800, 6);
   mmc_bankvrom(1, 0x1C00, 7);
}

static const map_memwrite map193_memwrite[] =
{
   { 0x6000, 0x7FFF, map193_write },
   {     -1,     -1, NULL }
};

const mapintf_t map193_intf =
{
   193,               /* mapper number */
   "NTDEC",           /* mapper name */
   map193_init,       /* init */
   NULL,              /* vblank */
   NULL,              /* hblank */
   NULL, NULL, NULL,
   map193_memwrite,
   NULL
};
