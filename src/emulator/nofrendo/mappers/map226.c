/*
** map226.c
** Mapper 226 — 76-in-1
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint8 reg226[2];

static void map226_write(uint32 address, uint8 value)
{
   uint8 bank;

   if (address & 0x0001)
      reg226[1] = value;
   else
      reg226[0] = value;

   if (reg226[0] & 0x40)
      ppu_mirror(0, 1, 0, 1); /* vertical */
   else
      ppu_mirror(0, 0, 1, 1); /* horizontal */

   bank = ((reg226[0] & 0x1E) >> 1) |
          ((reg226[0] & 0x80) >> 3) |
          ((reg226[1] & 0x01) << 5);

   if (reg226[0] & 0x20)
   {
      /* 16KB mode */
      if (reg226[0] & 0x01)
      {
         mmc_bankrom(8, 0x8000, (bank << 2) + 2);
         mmc_bankrom(8, 0xA000, (bank << 2) + 3);
         mmc_bankrom(8, 0xC000, (bank << 2) + 2);
         mmc_bankrom(8, 0xE000, (bank << 2) + 3);
      }
      else
      {
         mmc_bankrom(8, 0x8000, (bank << 2) + 0);
         mmc_bankrom(8, 0xA000, (bank << 2) + 1);
         mmc_bankrom(8, 0xC000, (bank << 2) + 0);
         mmc_bankrom(8, 0xE000, (bank << 2) + 1);
      }
   }
   else
   {
      /* 32KB mode */
      mmc_bankrom(8, 0x8000, (bank << 2) + 0);
      mmc_bankrom(8, 0xA000, (bank << 2) + 1);
      mmc_bankrom(8, 0xC000, (bank << 2) + 2);
      mmc_bankrom(8, 0xE000, (bank << 2) + 3);
   }
}

static void map226_init(void)
{
   reg226[0] = 0;
   reg226[1] = 0;
   mmc_bankrom(8, 0x8000, 0);
   mmc_bankrom(8, 0xA000, 1);
   mmc_bankrom(8, 0xC000, 2);
   mmc_bankrom(8, 0xE000, 3);
}

static const map_memwrite map226_memwrite[] =
{
   { 0x8000, 0xFFFF, map226_write },
   {     -1,     -1, NULL }
};

const mapintf_t map226_intf =
{
   226,          /* mapper number */
   "76-in-1",    /* mapper name */
   map226_init,  /* init */
   NULL,         /* vblank */
   NULL,         /* hblank */
   NULL, NULL, NULL,
   map226_memwrite,
   NULL
};
