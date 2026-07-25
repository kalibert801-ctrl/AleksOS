/*
** map082.c
** Mapper 82 — Taito X1-17
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint8 reg82_swap; /* CHR bank swap flag from $7EF6 bit 1 */

static void map082_write(uint32 address, uint8 value)
{
   switch (address)
   {
   case 0x7EF0:
      value &= 0xFE;
      if (reg82_swap)
      {
         mmc_bankvrom(1, 0x1000, value + 0);
         mmc_bankvrom(1, 0x1400, value + 1);
      }
      else
      {
         mmc_bankvrom(1, 0x0000, value + 0);
         mmc_bankvrom(1, 0x0400, value + 1);
      }
      break;
   case 0x7EF1:
      value &= 0xFE;
      if (reg82_swap)
      {
         mmc_bankvrom(1, 0x1800, value + 0);
         mmc_bankvrom(1, 0x1C00, value + 1);
      }
      else
      {
         mmc_bankvrom(1, 0x0800, value + 0);
         mmc_bankvrom(1, 0x0C00, value + 1);
      }
      break;
   case 0x7EF2:
      if (!reg82_swap) mmc_bankvrom(1, 0x1000, value);
      else             mmc_bankvrom(1, 0x0000, value);
      break;
   case 0x7EF3:
      if (!reg82_swap) mmc_bankvrom(1, 0x1400, value);
      else             mmc_bankvrom(1, 0x0400, value);
      break;
   case 0x7EF4:
      if (!reg82_swap) mmc_bankvrom(1, 0x1800, value);
      else             mmc_bankvrom(1, 0x0800, value);
      break;
   case 0x7EF5:
      if (!reg82_swap) mmc_bankvrom(1, 0x1C00, value);
      else             mmc_bankvrom(1, 0x0C00, value);
      break;
   case 0x7EF6:
      reg82_swap = value & 0x02;
      if (value & 0x01)
         ppu_mirror(0, 1, 0, 1); /* vertical */
      else
         ppu_mirror(0, 0, 1, 1); /* horizontal */
      /* fall through */
   case 0x7EFA:
      mmc_bankrom(8, 0x8000, value >> 2);
      break;
   case 0x7EFB:
      mmc_bankrom(8, 0xA000, value >> 2);
      break;
   case 0x7EFC:
      mmc_bankrom(8, 0xC000, value >> 2);
      break;
   }
}

static void map082_init(void)
{
   int last = mmc_getinfo()->rom_banks * 2 - 1;
   reg82_swap = 0;
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

static const map_memwrite map082_memwrite[] =
{
   { 0x6000, 0x7FFF, map082_write },
   {     -1,     -1, NULL }
};

const mapintf_t map82_intf =
{
   82,              /* mapper number */
   "Taito X1-17",   /* mapper name */
   map082_init,     /* init */
   NULL,            /* vblank */
   NULL,            /* hblank */
   NULL, NULL, NULL,
   map082_memwrite,
   NULL
};
