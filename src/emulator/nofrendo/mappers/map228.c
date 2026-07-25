/*
** map228.c
** Mapper 228 — Action 52
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static void map228_write(uint32 address, uint8 value)
{
   uint8 prg = (address & 0x0780) >> 7;
   uint8 chr  = ((address & 0x000F) << 2) | (value & 0x03);

   switch ((address & 0x1800) >> 11)
   {
   case 1: prg |= 0x10; break;
   case 3: prg |= 0x20; break;
   default: break;
   }

   if (address & 0x0020)
   {
      prg <<= 1;
      if (address & 0x0040) prg++;
      mmc_bankrom(8, 0x8000, (prg << 2) + 0);
      mmc_bankrom(8, 0xA000, (prg << 2) + 1);
      mmc_bankrom(8, 0xC000, (prg << 2) + 0);
      mmc_bankrom(8, 0xE000, (prg << 2) + 1);
   }
   else
   {
      mmc_bankrom(8, 0x8000, (prg << 2) + 0);
      mmc_bankrom(8, 0xA000, (prg << 2) + 1);
      mmc_bankrom(8, 0xC000, (prg << 2) + 2);
      mmc_bankrom(8, 0xE000, (prg << 2) + 3);
   }

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
}

static void map228_init(void)
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

static const map_memwrite map228_memwrite[] =
{
   { 0x8000, 0xFFFF, map228_write },
   {     -1,     -1, NULL }
};

const mapintf_t map228_intf =
{
   228,           /* mapper number */
   "Action 52",   /* mapper name */
   map228_init,   /* init */
   NULL,          /* vblank */
   NULL,          /* hblank */
   NULL, NULL, NULL,
   map228_memwrite,
   NULL
};
