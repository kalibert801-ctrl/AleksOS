/*
** map068.c
** Mapper 68 (Sunsoft-4 / After Burner)
**
** CHR: two 4KB pages via $8000-$9FFF.
** PRG: 16KB switchable at $8000 via $D000, $C000 fixed to last.
** Mirroring: $C000 bits 1-0 (0=vert, 1=horiz, 2=NT0, 3=NT1).
** Note: ROM-based nametable feature ($A000/$B000) not implemented.
*/

#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>

static void map68_write(uint32 address, uint8 value)
{
   switch (address & 0xF000)
   {
   case 0x8000:
      mmc_bankvrom(4, 0x0000, value & 0x1F);
      break;

   case 0x9000:
      mmc_bankvrom(4, 0x1000, value & 0x1F);
      break;

   /* $A000, $B000: ROM-based NT — not supported */

   case 0xC000:
      switch (value & 0x03)
      {
      case 0: ppu_mirror(0, 1, 0, 1); break; /* vertical   */
      case 1: ppu_mirror(0, 0, 1, 1); break; /* horizontal */
      case 2: ppu_mirror(0, 0, 0, 0); break; /* one-screen NT0 */
      case 3: ppu_mirror(1, 1, 1, 1); break; /* one-screen NT1 */
      }
      break;

   case 0xD000:
      mmc_bankrom(16, 0x8000, value & 0x0F);
      break;

   default:
      break;
   }
}

static void map68_init(void)
{
   mmc_bankrom(16, 0x8000, 0);
   mmc_bankrom(16, 0xC000, mmc_getinfo()->rom_banks - 1);
}

static const map_memwrite map68_memwrite[] =
{
   { 0x8000, 0xFFFF, map68_write },
   {     -1,     -1, NULL }
};

const mapintf_t map68_intf =
{
   68,             /* mapper number */
   "Sunsoft-4",    /* mapper name */
   map68_init,
   NULL, NULL, NULL, NULL, NULL,
   map68_memwrite,
   NULL
};
