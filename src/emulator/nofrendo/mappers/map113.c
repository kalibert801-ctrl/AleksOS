/*
** map113.c
** Mapper 113 — Hacker/PC-Sachen
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static void map113_do_bank(uint8 value)
{
   uint8 prg = value >> 3;
   uint8 chr;

   if ((mmc_getinfo()->rom_banks * 2 <= 8) && (mmc_getinfo()->vrom_banks * 8 == 128))
      chr = ((value >> 3) & 0x08) + (value & 0x07);
   else
      chr = value & 0x07;

   prg = (prg << 2);
   chr = (chr << 3);

   mmc_bankrom(8, 0x8000, prg + 0);
   mmc_bankrom(8, 0xA000, prg + 1);
   mmc_bankrom(8, 0xC000, prg + 2);
   mmc_bankrom(8, 0xE000, prg + 3);

   mmc_bankvrom(1, 0x0000, chr + 0);
   mmc_bankvrom(1, 0x0400, chr + 1);
   mmc_bankvrom(1, 0x0800, chr + 2);
   mmc_bankvrom(1, 0x0C00, chr + 3);
   mmc_bankvrom(1, 0x1000, chr + 4);
   mmc_bankvrom(1, 0x1400, chr + 5);
   mmc_bankvrom(1, 0x1800, chr + 6);
   mmc_bankvrom(1, 0x1C00, chr + 7);
}

static void map113_apu_write(uint32 address, uint8 value)
{
   /* handles $4100, $4111, $4120, $4900 and nearby addresses */
   UNUSED(address);
   map113_do_bank(value);
}

static void map113_write(uint32 address, uint8 value)
{
   /* handles $8008, $8009 */
   UNUSED(address);
   map113_do_bank(value);
}

static void map113_init(void)
{
   mmc_bankrom(8, 0x8000, 0);
   mmc_bankrom(8, 0xA000, 1);
   mmc_bankrom(8, 0xC000, 2);
   mmc_bankrom(8, 0xE000, 3);
   mmc_bankvrom(1, 0x0000, 0);
   mmc_bankvrom(1, 0x0400, 1);
   mmc_bankvrom(1, 0x0800, 2);
   mmc_bankvrom(1, 0x0C00, 3);
   mmc_bankvrom(1, 0x1000, 4);
   mmc_bankvrom(1, 0x1400, 5);
   mmc_bankvrom(1, 0x1800, 6);
   mmc_bankvrom(1, 0x1C00, 7);
}

static const map_memwrite map113_memwrite[] =
{
   { 0x4100, 0x4FFF, map113_apu_write },
   { 0x8000, 0xFFFF, map113_write },
   {     -1,     -1, NULL }
};

const mapintf_t map113_intf =
{
   113,          /* mapper number */
   "Hacker",     /* mapper name */
   map113_init,  /* init */
   NULL,         /* vblank */
   NULL,         /* hblank */
   NULL, NULL, NULL,
   map113_memwrite,
   NULL
};
