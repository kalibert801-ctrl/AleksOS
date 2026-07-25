/*
** map013.c
** Mapper 13 (CPROM)
**
** PRG: 32KB fixed (no switching).
** CHR: 16KB RAM split into 4KB pages.
**   $0000-$0FFF: always bank 0 (fixed).
**   $1000-$1FFF: switchable via write bits 1-0 to $8000-$FFFF.
*/

#include <noftypes.h>
#include <nes_mmc.h>

static void map13_write(uint32 address, uint8 value)
{
   UNUSED(address);
   mmc_bankvrom(4, 0x1000, value & 0x03);
}

static void map13_init(void)
{
   mmc_bankvrom(4, 0x0000, 0);
   mmc_bankvrom(4, 0x1000, 0);
}

static const map_memwrite map13_memwrite[] =
{
   { 0x8000, 0xFFFF, map13_write },
   {     -1,     -1, NULL }
};

const mapintf_t map13_intf =
{
   13,            /* mapper number */
   "CPROM",       /* mapper name */
   map13_init,    /* init routine */
   NULL, NULL, NULL, NULL, NULL,
   map13_memwrite,
   NULL
};
