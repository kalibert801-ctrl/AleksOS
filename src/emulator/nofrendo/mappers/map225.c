/*
** map225.c
** Mapper 225 — 72-in-1
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static void map225_write(uint32 address, uint8 value)
{
   uint8 prg = (address & 0x0F80) >> 7;
   uint8 chr = address & 0x003F;

   UNUSED(value);

   mmc_bankvrom(1, 0x0000, (chr << 3) + 0);
   mmc_bankvrom(1, 0x0400, (chr << 3) + 1);
   mmc_bankvrom(1, 0x0800, (chr << 3) + 2);
   mmc_bankvrom(1, 0x0C00, (chr << 3) + 3);
   mmc_bankvrom(1, 0x1000, (chr << 3) + 4);
   mmc_bankvrom(1, 0x1400, (chr << 3) + 5);
   mmc_bankvrom(1, 0x1800, (chr << 3) + 6);
   mmc_bankvrom(1, 0x1C00, (chr << 3) + 7);

   if (address & 0x2000)
      ppu_mirror(0, 0, 1, 1); /* horizontal */
   else
      ppu_mirror(0, 1, 0, 1); /* vertical */

   if (address & 0x1000)
   {
      /* 16KB mode */
      if (address & 0x0040)
      {
         mmc_bankrom(8, 0x8000, (prg << 2) + 2);
         mmc_bankrom(8, 0xA000, (prg << 2) + 3);
         mmc_bankrom(8, 0xC000, (prg << 2) + 2);
         mmc_bankrom(8, 0xE000, (prg << 2) + 3);
      }
      else
      {
         mmc_bankrom(8, 0x8000, (prg << 2) + 0);
         mmc_bankrom(8, 0xA000, (prg << 2) + 1);
         mmc_bankrom(8, 0xC000, (prg << 2) + 0);
         mmc_bankrom(8, 0xE000, (prg << 2) + 1);
      }
   }
   else
   {
      /* 32KB mode */
      mmc_bankrom(8, 0x8000, (prg << 2) + 0);
      mmc_bankrom(8, 0xA000, (prg << 2) + 1);
      mmc_bankrom(8, 0xC000, (prg << 2) + 2);
      mmc_bankrom(8, 0xE000, (prg << 2) + 3);
   }
}

static void map225_init(void)
{
   mmc_bankrom(8, 0x8000, 0);
   mmc_bankrom(8, 0xA000, 1);
   mmc_bankrom(8, 0xC000, 2);
   mmc_bankrom(8, 0xE000, 3);
   mmc_bankvrom(1, 0x0000, 0);
   mmc_bankvrom(1, 0x0400, 1);
   mmc_bankvrom(1, 0x0800, 2);
   mmc_bankvrom(1, 0x0C00, 3);
   mmc_bankvrom(1, 0x1000, 4);
   mmc_bankvrom(1, 0x1400, 5);
   mmc_bankvrom(1, 0x1800, 6);
   mmc_bankvrom(1, 0x1C00, 7);
}

static const map_memwrite map225_memwrite[] =
{
   { 0x8000, 0xFFFF, map225_write },
   {     -1,     -1, NULL }
};

const mapintf_t map225_intf =
{
   225,          /* mapper number */
   "72-in-1",    /* mapper name */
   map225_init,  /* init */
   NULL,         /* vblank */
   NULL,         /* hblank */
   NULL, NULL, NULL,
   map225_memwrite,
   NULL
};
