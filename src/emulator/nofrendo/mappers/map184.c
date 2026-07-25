/*
** map184.c
** Mapper 184 (Sunsoft-1)
**
** PRG: 32KB fixed (no switching).
** CHR: 8KB split via write to $6000-$7FFF:
**   bits 2-0: 4KB CHR bank at $0000
**   bits 6-4: 4KB CHR bank at $1000
*/

#include <noftypes.h>
#include <nes_mmc.h>

static void map184_write(uint32 address, uint8 value)
{
   UNUSED(address);
   mmc_bankvrom(4, 0x0000, value & 0x07);
   mmc_bankvrom(4, 0x1000, (value >> 4) & 0x07);
}

static const map_memwrite map184_memwrite[] =
{
   { 0x6000, 0x7FFF, map184_write },
   {     -1,     -1, NULL }
};

const mapintf_t map184_intf =
{
   184,           /* mapper number */
   "Sunsoft-1",   /* mapper name */
   NULL,
   NULL, NULL, NULL, NULL, NULL,
   map184_memwrite,
   NULL
};
