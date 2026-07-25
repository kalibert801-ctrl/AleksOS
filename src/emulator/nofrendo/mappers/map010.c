/*
** map010.c
** Mapper 10 (MMC4)
**
** Like MMC2 (mapper 9) but with 16KB switchable PRG at $8000
** and the same FD/FE CHR-latch mechanism for $0000 and $1000.
*/

#include <string.h>
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <libsnss.h>

static uint8 latch[2];
static uint8 regs[4];

static void mmc4_latchfunc(uint32 address, uint8 value)
{
   if (value == 0xFD || value == 0xFE)
   {
      int reg;
      if (address)
      {
         latch[1] = value;
         reg = 2 + (value - 0xFD);
      }
      else
      {
         latch[0] = value;
         reg = value - 0xFD;
      }
      mmc_bankvrom(4, address ? 0x1000 : 0x0000, regs[reg]);
   }
}

static void map10_write(uint32 address, uint8 value)
{
   switch ((address & 0xF000) >> 12)
   {
   case 0xA:
      /* 16KB PRG at $8000 */
      mmc_bankrom(16, 0x8000, value & 0x0F);
      break;

   case 0xB:
      regs[0] = value;
      if (latch[0] == 0xFD)
         mmc_bankvrom(4, 0x0000, value);
      break;

   case 0xC:
      regs[1] = value;
      if (latch[0] == 0xFE)
         mmc_bankvrom(4, 0x0000, value);
      break;

   case 0xD:
      regs[2] = value;
      if (latch[1] == 0xFD)
         mmc_bankvrom(4, 0x1000, value);
      break;

   case 0xE:
      regs[3] = value;
      if (latch[1] == 0xFE)
         mmc_bankvrom(4, 0x1000, value);
      break;

   case 0xF:
      if (value & 1)
         ppu_mirror(0, 0, 1, 1); /* horizontal */
      else
         ppu_mirror(0, 1, 0, 1); /* vertical */
      break;

   default:
      break;
   }
}

static void map10_init(void)
{
   memset(regs, 0, sizeof(regs));

   mmc_bankrom(16, 0x8000, 0);
   mmc_bankrom(16, 0xC000, mmc_getinfo()->rom_banks - 1);

   latch[0] = 0xFE;
   latch[1] = 0xFE;

   ppu_setlatchfunc(mmc4_latchfunc);
}

static const map_memwrite map10_memwrite[] =
{
   { 0x8000, 0xFFFF, map10_write },
   {     -1,     -1, NULL }
};

const mapintf_t map10_intf =
{
   10,            /* mapper number */
   "MMC4",        /* mapper name */
   map10_init,    /* init routine */
   NULL,          /* vblank callback */
   NULL,          /* hblank callback */
   NULL,          /* get state (snss) */
   NULL,          /* set state (snss) */
   NULL,          /* memory read structure */
   map10_memwrite,/* memory write structure */
   NULL           /* external sound device */
};
