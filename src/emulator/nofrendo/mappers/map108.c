/*
** map108.c
** Mapper 108 — single PRG bank switcher at $6000
** Note: the original mapper maps a ROM bank into the $6000-$7FFF window.
** This simplified port ignores the $6000 window and only handles PRG at $8000+.
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static void map108_write(uint32 address, uint8 value)
{
   /* Original: SRAMBANK = ROMPAGE(value) — maps value into $6000 window.
      Simplified: no-op since Nofrendo does not support ROM-backed SRAM window here. */
   UNUSED(address);
   UNUSED(value);
}

static void map108_init(void)
{
   int banks = mmc_getinfo()->rom_banks * 2;
   mmc_bankrom(8, 0x8000, (0x0C < banks) ? 0x0C : 0);
   mmc_bankrom(8, 0xA000, (0x0D < banks) ? 0x0D : 1);
   mmc_bankrom(8, 0xC000, (0x0E < banks) ? 0x0E : banks - 2);
   mmc_bankrom(8, 0xE000, (0x0F < banks) ? 0x0F : banks - 1);
}

static const map_memwrite map108_memwrite[] =
{
   { 0x8000, 0xFFFF, map108_write },
   {     -1,     -1, NULL }
};

const mapintf_t map108_intf =
{
   108,          /* mapper number */
   "108",        /* mapper name */
   map108_init,  /* init */
   NULL,         /* vblank */
   NULL,         /* hblank */
   NULL, NULL, NULL,
   map108_memwrite,
   NULL
};
