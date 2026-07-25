/*
** map077.c
** Mapper 77 — Irem Early Mapper
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static void map77_write(uint32 address, uint8 value)
{
   uint8 prg = (value & 0x07) << 2;
   uint8 chr = ((value & 0xF0) >> 4) << 1;

   mmc_bankrom(8, 0x8000, prg + 0);
   mmc_bankrom(8, 0xA000, prg + 1);
   mmc_bankrom(8, 0xC000, prg + 2);
   mmc_bankrom(8, 0xE000, prg + 3);

   mmc_bankvrom(1, 0x0000, chr + 0);
   mmc_bankvrom(1, 0x0400, chr + 1);

   UNUSED(address);
}

static void map77_init(void)
{
   mmc_bankrom(8, 0x8000, 0);
   mmc_bankrom(8, 0xA000, 1);
   mmc_bankrom(8, 0xC000, 2);
   mmc_bankrom(8, 0xE000, 3);
}

static const map_memwrite map77_memwrite[] =
{
   { 0x8000, 0xFFFF, map77_write },
   {     -1,     -1, NULL }
};

const mapintf_t map77_intf =
{
   77,                 /* mapper number */
   "Irem Early",       /* mapper name */
   map77_init,         /* init */
   NULL,               /* vblank */
   NULL,               /* hblank */
   NULL, NULL, NULL,
   map77_memwrite,
   NULL
};
