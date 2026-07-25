/*
** map072.c
** Mapper 72 (Jaleco JF-17)
**
** Write $8000-$FFFF:
**   bit 7 set → bits 3-0 = 16KB PRG bank at $8000 ($C000 fixed to last)
**   bit 6 set → bits 3-0 = 8KB CHR bank at $0000
** Both bits can be set in the same write.
*/

#include <noftypes.h>
#include <nes_mmc.h>

static void map72_write(uint32 address, uint8 value)
{
   UNUSED(address);
   if (value & 0x80)
      mmc_bankrom(16, 0x8000, value & 0x0F);
   if (value & 0x40)
      mmc_bankvrom(8, 0x0000, value & 0x0F);
}

static void map72_init(void)
{
   mmc_bankrom(16, 0x8000, 0);
   mmc_bankrom(16, 0xC000, mmc_getinfo()->rom_banks - 1);
}

static const map_memwrite map72_memwrite[] =
{
   { 0x8000, 0xFFFF, map72_write },
   {     -1,     -1, NULL }
};

const mapintf_t map72_intf =
{
   72,              /* mapper number */
   "Jaleco JF-17",  /* mapper name */
   map72_init,
   NULL, NULL, NULL, NULL, NULL,
   map72_memwrite,
   NULL
};
