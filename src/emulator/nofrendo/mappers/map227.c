/*
** map227.c
**
** Mapper 227 (1200-in-1 multicart)
**
** Ported from InfoNES (InfoNES_Mapper_227.cpp) to nofrendo mapintf_t API.
**
** PRG: 8KB × 4 switchable pages at $8000-$FFFF
** CHR: VRAM (no CHR ROM in multicart)
**
** Register: write to $8000-$FFFF, data byte ignored — all info in address bus.
**
**   addr[0]     (A0)  = 1: 32KB sequential  /  0: 16KB mirrored
**   addr[1]     (A1)  = 1: horizontal mirror / 0: vertical mirror
**   addr[2]     (A2)  = 1: upper 16KB half   / 0: lower 16KB half (when A0=0)
**   addr[3..6]  (A3-A6) = bank bits 0-3
**   addr[7]     (A7)  = 1: inner bank mode (A12/A13 use full byBank)
**                       0: outer bank mode — upper 16KB fixed to outer block
**   addr[8]     (A8)  = bank bit 4
**   addr[9]     (A9)  = 1: upper-half outer / 0: lower-half outer (when A7=0)
*/

#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>

/*-------------------------------------------------------------------*/
/* $8000-$FFFF write — PRG bank switch                              */
/*-------------------------------------------------------------------*/
static void map227_write(uint32 address, uint8 value)
{
   UNUSED(value);   /* data byte unused; all info is in the address */

   /* 5-bit bank: {A8, A6, A5, A4, A3} */
   int bank = (int)(((address & 0x0100) >> 4) | ((address & 0x0078) >> 3));

   if (address & 0x0001)
   {
      /* 32KB sequential mode */
      mmc_bankrom(8, 0x8000, (bank << 2) + 0);
      mmc_bankrom(8, 0xA000, (bank << 2) + 1);
      mmc_bankrom(8, 0xC000, (bank << 2) + 2);
      mmc_bankrom(8, 0xE000, (bank << 2) + 3);
   }
   else
   {
      if (address & 0x0004)
      {
         /* 16KB upper half mirrored ($C000-$FFFF pages) */
         mmc_bankrom(8, 0x8000, (bank << 2) + 2);
         mmc_bankrom(8, 0xA000, (bank << 2) + 3);
         mmc_bankrom(8, 0xC000, (bank << 2) + 2);
         mmc_bankrom(8, 0xE000, (bank << 2) + 3);
      }
      else
      {
         /* 16KB lower half mirrored ($8000-$BFFF pages) */
         mmc_bankrom(8, 0x8000, (bank << 2) + 0);
         mmc_bankrom(8, 0xA000, (bank << 2) + 1);
         mmc_bankrom(8, 0xC000, (bank << 2) + 0);
         mmc_bankrom(8, 0xE000, (bank << 2) + 1);
      }
   }

   /* Outer bank override: when A7=0 the upper 16KB ($C000-$FFFF) is
   ** forced to the beginning (or end) of the outer 128KB block         */
   if (!(address & 0x0080))
   {
      int outer = (bank & 0x1C) << 2;   /* bits [4:2] of bank × 4 */
      if (address & 0x0200)
      {
         /* end of outer block */
         mmc_bankrom(8, 0xC000, outer + 14);
         mmc_bankrom(8, 0xE000, outer + 15);
      }
      else
      {
         /* start of outer block */
         mmc_bankrom(8, 0xC000, outer + 0);
         mmc_bankrom(8, 0xE000, outer + 1);
      }
   }

   /* Mirroring */
   if (address & 0x0002)
      ppu_mirror(0, 0, 1, 1);   /* horizontal */
   else
      ppu_mirror(0, 1, 0, 1);   /* vertical   */
}

/*-------------------------------------------------------------------*/
/* Init                                                              */
/*-------------------------------------------------------------------*/
static void map227_init(void)
{
   /* Power-on state: first 16KB mirrored (bank 0, lower half) */
   mmc_bankrom(8, 0x8000, 0);
   mmc_bankrom(8, 0xA000, 1);
   mmc_bankrom(8, 0xC000, 0);
   mmc_bankrom(8, 0xE000, 1);

   ppu_mirror(0, 1, 0, 1);   /* vertical */
}

/*-------------------------------------------------------------------*/
/* Memory maps                                                       */
/*-------------------------------------------------------------------*/
static map_memwrite map227_memwrite[] =
{
   { 0x8000, 0xFFFF, map227_write },
   {     -1,     -1, NULL }
};

mapintf_t map227_intf =
{
   227,               /* mapper number */
   "1200-in-1",       /* mapper name   */
   map227_init,       /* init routine  */
   NULL,              /* vblank callback */
   NULL,              /* hblank callback */
   NULL,              /* get state (snss) */
   NULL,              /* set state (snss) */
   NULL,              /* memory read structure  */
   map227_memwrite,   /* memory write structure */
   NULL               /* external sound device  */
};
