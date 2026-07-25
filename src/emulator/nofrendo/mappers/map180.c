/*
** map180.c
** Mapper 180 (UNROM variant — Crazy Climber)
**
** Opposite of standard UNROM:
**   $8000-$BFFF: fixed to bank 0
**   $C000-$FFFF: switchable via write bits 2-0 to $8000-$FFFF
*/

#include <noftypes.h>
#include <nes_mmc.h>

static void map180_write(uint32 address, uint8 value)
{
   UNUSED(address);
   mmc_bankrom(16, 0xC000, value & 0x07);
}

static void map180_init(void)
{
   mmc_bankrom(16, 0x8000, 0);
   mmc_bankrom(16, 0xC000, 0);
}

static const map_memwrite map180_memwrite[] =
{
   { 0x8000, 0xFFFF, map180_write },
   {     -1,     -1, NULL }
};

const mapintf_t map180_intf =
{
   180,                      /* mapper number */
   "UNROM (Crazy Climber)",  /* mapper name */
   map180_init,
   NULL, NULL, NULL, NULL, NULL,
   map180_memwrite,
   NULL
};
