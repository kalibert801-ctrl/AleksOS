/*
** Nofrendo (c) 1998-2000 Matthew Conte (matt@conte.com)
**
** map071.c
** Mapper 71 (Camerica / Codemasters)
**
** PRG: 128KB or 256KB; 16KB switchable at $8000, last 16KB fixed at $C000.
** CHR: ROM or RAM, 8KB fixed (no switching).
** Mirroring: writes to $8000-$BFFF bit 4 select one-screen NT (Fire Hawk only).
*/

#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>

static void map71_write(uint32 address, uint8 value)
{
   if (address < 0xC000)
   {
      /* $8000-$BFFF: one-screen mirroring select (bit 4); Fire Hawk only */
      int m = (value >> 4) & 1;
      ppu_mirror(m, m, m, m);
   }
   else
   {
      /* $C000-$FFFF: select 16KB PRG bank at $8000 */
      mmc_bankrom(16, 0x8000, value & 0x0F);
   }
}

static void map71_init(void)
{
   /* Last 16KB bank fixed at $C000 */
   mmc_bankrom(16, 0xC000, mmc_getinfo()->rom_banks - 1);
   /* Bank 0 at $8000 by default */
   mmc_bankrom(16, 0x8000, 0);
}

static const map_memwrite map71_memwrite[] =
{
   { 0x8000, 0xFFFF, map71_write },
   {     -1,     -1, NULL }
};

const mapintf_t map71_intf =
{
   71,              /* mapper number */
   "Camerica",      /* mapper name */
   map71_init,      /* init routine */
   NULL,            /* vblank callback */
   NULL,            /* hblank callback */
   NULL,            /* get state (snss) */
   NULL,            /* set state (snss) */
   NULL,            /* memory read structure */
   map71_memwrite,  /* memory write structure */
   NULL             /* external sound device */
};
