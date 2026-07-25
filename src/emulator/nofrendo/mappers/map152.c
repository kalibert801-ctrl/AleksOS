/*
** map152.c
** Mapper 152 (Irem discrete)
**
** Write $8000-$FFFF:
**   bits 6-4: 16KB PRG bank at $8000 ($C000 fixed to last)
**   bits 3-0: 8KB CHR bank at $0000
**   bit 7:    one-screen mirroring (0 = NT0, 1 = NT1)
*/

#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>

static void map152_write(uint32 address, uint8 value)
{
   UNUSED(address);
   mmc_bankrom(16, 0x8000, (value >> 4) & 0x07);
   mmc_bankvrom(8,  0x0000, value & 0x0F);
   int m = (value >> 7) & 1;
   ppu_mirror(m, m, m, m);
}

static void map152_init(void)
{
   mmc_bankrom(16, 0x8000, 0);
   mmc_bankrom(16, 0xC000, mmc_getinfo()->rom_banks - 1);
}

static const map_memwrite map152_memwrite[] =
{
   { 0x8000, 0xFFFF, map152_write },
   {     -1,     -1, NULL }
};

const mapintf_t map152_intf =
{
   152,            /* mapper number */
   "Irem 152",     /* mapper name */
   map152_init,
   NULL, NULL, NULL, NULL, NULL,
   map152_memwrite,
   NULL
};
