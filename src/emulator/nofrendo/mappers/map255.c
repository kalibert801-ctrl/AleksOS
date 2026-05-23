/*
** map255.c
**
** Mapper 255 (110-in-1 / 401-in-1 multicart)
**
** Ported from InfoNES (InfoNES_Mapper_255.cpp) to nofrendo mapintf_t API.
**
** PRG: 8KB × 4 switchable pages at $8000-$FFFF
** CHR: 1KB × 8 switchable pages at $0000-$1FFF
** Extra regs: $5800-$5FFF (r/w, 4 × 4-bit)
*/

#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>

static uint8 map255_reg[4];

/*-------------------------------------------------------------------*/
/* $8000-$FFFF write — PRG/CHR bank switch                          */
/*-------------------------------------------------------------------*/
static void map255_write(uint32 address, uint8 value)
{
   UNUSED(value);   /* data byte is unused; all info is in the address */

   uint8 prg   = (uint8)((address & 0x0F80) >> 7);   /* bits 7-11 */
   int   chr   = (int)  (address & 0x003F);           /* bits 0-5  */
   int   outer = (int)  ((address & 0x4000) >> 14);   /* bit 14    */

   /* Mirroring */
   if (address & 0x2000)
      ppu_mirror(0, 0, 1, 1);  /* horizontal */
   else
      ppu_mirror(0, 1, 0, 1);  /* vertical   */

   /* PRG banking — 8KB pages */
   int base = 0x80 * outer + (int)prg * 4;
   if (address & 0x1000) {
      if (address & 0x0040) {
         /* 16KB fixed to upper half, mirrored */
         mmc_bankrom(8, 0x8000, base + 2);
         mmc_bankrom(8, 0xA000, base + 3);
         mmc_bankrom(8, 0xC000, base + 2);
         mmc_bankrom(8, 0xE000, base + 3);
      } else {
         /* 16KB fixed to lower half, mirrored */
         mmc_bankrom(8, 0x8000, base + 0);
         mmc_bankrom(8, 0xA000, base + 1);
         mmc_bankrom(8, 0xC000, base + 0);
         mmc_bankrom(8, 0xE000, base + 1);
      }
   } else {
      /* 32KB sequential */
      mmc_bankrom(8, 0x8000, base + 0);
      mmc_bankrom(8, 0xA000, base + 1);
      mmc_bankrom(8, 0xC000, base + 2);
      mmc_bankrom(8, 0xE000, base + 3);
   }

   /* CHR banking — 1KB pages */
   int cbase = 0x200 * outer + chr * 8;
   mmc_bankvrom(1, 0x0000, cbase + 0);
   mmc_bankvrom(1, 0x0400, cbase + 1);
   mmc_bankvrom(1, 0x0800, cbase + 2);
   mmc_bankvrom(1, 0x0C00, cbase + 3);
   mmc_bankvrom(1, 0x1000, cbase + 4);
   mmc_bankvrom(1, 0x1400, cbase + 5);
   mmc_bankvrom(1, 0x1800, cbase + 6);
   mmc_bankvrom(1, 0x1C00, cbase + 7);
}

/*-------------------------------------------------------------------*/
/* $5800-$5FFF register write                                        */
/*-------------------------------------------------------------------*/
static void map255_writereg(uint32 address, uint8 value)
{
   map255_reg[address & 0x03] = value & 0x0F;
}

/*-------------------------------------------------------------------*/
/* $5800-$5FFF register read                                         */
/*-------------------------------------------------------------------*/
static uint8 map255_readreg(uint32 address)
{
   if (address >= 0x5800)
      return map255_reg[address & 0x03] & 0x0F;
   return (uint8)(address >> 8);
}

/*-------------------------------------------------------------------*/
/* Init                                                              */
/*-------------------------------------------------------------------*/
static void map255_init(void)
{
   map255_reg[0] = map255_reg[1] = map255_reg[2] = map255_reg[3] = 0;

   mmc_bankrom(8, 0x8000, 0);
   mmc_bankrom(8, 0xA000, 1);
   mmc_bankrom(8, 0xC000, 2);
   mmc_bankrom(8, 0xE000, 3);
   mmc_bankvrom(8, 0x0000, 0);

   ppu_mirror(0, 1, 0, 1);  /* vertical */
}

/*-------------------------------------------------------------------*/
/* Memory maps                                                       */
/*-------------------------------------------------------------------*/
static map_memread map255_memread[] =
{
   { 0x5800, 0x5FFF, map255_readreg },
   {     -1,     -1, NULL }
};

static map_memwrite map255_memwrite[] =
{
   { 0x5800, 0x5FFF, map255_writereg },
   { 0x8000, 0xFFFF, map255_write    },
   {     -1,     -1, NULL }
};

mapintf_t map255_intf =
{
   255,               /* mapper number */
   "110-in-1",        /* mapper name   */
   map255_init,       /* init routine  */
   NULL,              /* vblank callback */
   NULL,              /* hblank callback */
   NULL,              /* get state (snss) */
   NULL,              /* set state (snss) */
   map255_memread,    /* memory read structure  */
   map255_memwrite,   /* memory write structure */
   NULL               /* external sound device  */
};
