/*
** map222.c
** Mapper 222
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static void map222_write(uint32 address, uint8 value)
{
   switch (address & 0xF003)
   {
   case 0x8000:
      mmc_bankrom(8, 0x8000, value);
      break;
   case 0xA000:
      mmc_bankrom(8, 0xA000, value);
      break;
   case 0xB000:
      mmc_bankvrom(1, 0x0000, value);
      break;
   case 0xB002:
      mmc_bankvrom(1, 0x0400, value);
      break;
   case 0xC000:
      mmc_bankvrom(1, 0x0800, value);
      break;
   case 0xC002:
      mmc_bankvrom(1, 0x0C00, value);
      break;
   case 0xD000:
      mmc_bankvrom(1, 0x1000, value);
      break;
   case 0xD002:
      mmc_bankvrom(1, 0x1400, value);
      break;
   case 0xE000:
      mmc_bankvrom(1, 0x1800, value);
      break;
   case 0xE002:
      mmc_bankvrom(1, 0x1C00, value);
      break;
   }
}

static void map222_init(void)
{
   int last = mmc_getinfo()->rom_banks * 2 - 1;
   mmc_bankrom(8, 0x8000, 0);
   mmc_bankrom(8, 0xA000, 1);
   mmc_bankrom(8, 0xC000, last - 1);
   mmc_bankrom(8, 0xE000, last);
   mmc_bankvrom(1, 0x0000, 0);
   mmc_bankvrom(1, 0x0400, 1);
   mmc_bankvrom(1, 0x0800, 2);
   mmc_bankvrom(1, 0x0C00, 3);
   mmc_bankvrom(1, 0x1000, 4);
   mmc_bankvrom(1, 0x1400, 5);
   mmc_bankvrom(1, 0x1800, 6);
   mmc_bankvrom(1, 0x1C00, 7);
   ppu_mirror(0, 1, 0, 1); /* vertical */
}

static const map_memwrite map222_memwrite[] =
{
   { 0x8000, 0xFFFF, map222_write },
   {     -1,     -1, NULL }
};

const mapintf_t map222_intf =
{
   222,          /* mapper number */
   "222",        /* mapper name */
   map222_init,  /* init */
   NULL,         /* vblank */
   NULL,         /* hblank */
   NULL, NULL, NULL,
   map222_memwrite,
   NULL
};
