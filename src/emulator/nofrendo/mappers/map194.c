/*
** map194.c
** Mapper 194 — Meikyuu Jiin Dababa (simplified)
** Note: original maps ROM into the $6000 SRAM window; simplified here.
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static void map194_write(uint32 address, uint8 value)
{
   /* Original: SRAMBANK = ROMPAGE(value) — SRAM window backed by ROM.
      Simplified: no-op. */
   UNUSED(address);
   UNUSED(value);
}

static void map194_init(void)
{
   int banks = mmc_getinfo()->rom_banks * 2;
   mmc_bankrom(8, 0x8000, banks - 4);
   mmc_bankrom(8, 0xA000, banks - 3);
   mmc_bankrom(8, 0xC000, banks - 2);
   mmc_bankrom(8, 0xE000, banks - 1);
}

static const map_memwrite map194_memwrite[] =
{
   { 0x8000, 0xFFFF, map194_write },
   {     -1,     -1, NULL }
};

const mapintf_t map194_intf =
{
   194,                    /* mapper number */
   "Meikyuu Jiin Dababa",  /* mapper name */
   map194_init,            /* init */
   NULL,                   /* vblank */
   NULL,                   /* hblank */
   NULL, NULL, NULL,
   map194_memwrite,
   NULL
};
