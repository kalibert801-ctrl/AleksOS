/*
** map122.c
** Mapper 122 — Sunsoft (simple CHR bank select via SRAM)
**
** Converted from InfoNES_Mapper_122.cpp
** PRG ROM is fixed (banks 0-3). A write to $6000 selects two 4K CHR banks:
**   bits 2:0 → lower 4 × 1K CHR pages (0x0000-0x0FFF)
**   bits 6:4 → upper 4 × 1K CHR pages (0x1000-0x1FFF)
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static void map122_sram(uint32 address, uint8 value)
{
   if (address == 0x6000)
   {
      uint8 bank0 = (uint8)((value & 0x07) << 2);
      uint8 bank1 = (uint8)(((value & 0x70) >> 4) << 2);

      mmc_bankvrom(1, 0x0000, bank0 + 0);
      mmc_bankvrom(1, 0x0400, bank0 + 1);
      mmc_bankvrom(1, 0x0800, bank0 + 2);
      mmc_bankvrom(1, 0x0C00, bank0 + 3);
      mmc_bankvrom(1, 0x1000, bank1 + 0);
      mmc_bankvrom(1, 0x1400, bank1 + 1);
      mmc_bankvrom(1, 0x1800, bank1 + 2);
      mmc_bankvrom(1, 0x1C00, bank1 + 3);
   }
}

static void map122_init(void)
{
   mmc_bankrom(8, 0x8000, 0);
   mmc_bankrom(8, 0xA000, 1);
   mmc_bankrom(8, 0xC000, 2);
   mmc_bankrom(8, 0xE000, 3);
}

static const map_memwrite map122_memwrite[] =
{
   { 0x6000, 0x7FFF, map122_sram },
   {     -1,     -1, NULL }
};

const mapintf_t map122_intf =
{
   122,          /* mapper number */
   "Sunsoft",    /* mapper name */
   map122_init,  /* init */
   NULL,         /* vblank */
   NULL,         /* hblank */
   NULL, NULL, NULL,
   map122_memwrite,
   NULL
};
