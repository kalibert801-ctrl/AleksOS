/*
** map246.c
** Mapper 246 — Phone Serm Berm
**
** PRG and CHR banking is controlled by writes to the SRAM region:
**   $6000: PRG bank 0 (32KB block select → 8KB pages 0..3 → $8000)
**   $6001: PRG bank 1 ($A000)
**   $6002: PRG bank 2 ($C000)
**   $6003: PRG bank 3 ($E000)
**   $6004: CHR bank for PPU $0000-$07FF (2×1KB)
**   $6005: CHR bank for PPU $0800-$0FFF (2×1KB)
**   $6006: CHR bank for PPU $1000-$17FF (2×1KB)
**   $6007: CHR bank for PPU $1800-$1FFF (2×1KB)
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static void map246_sram(uint32 address, uint8 value)
{
   switch (address) {
   case 0x6000:
      mmc_bankrom(8, 0x8000, (value << 2) + 0);
      break;
   case 0x6001:
      mmc_bankrom(8, 0xA000, (value << 2) + 1);
      break;
   case 0x6002:
      mmc_bankrom(8, 0xC000, (value << 2) + 2);
      break;
   case 0x6003:
      mmc_bankrom(8, 0xE000, (value << 2) + 3);
      break;
   case 0x6004:
      mmc_bankvrom(1, 0x0000, (value << 1) + 0);
      mmc_bankvrom(1, 0x0400, (value << 1) + 1);
      break;
   case 0x6005:
      mmc_bankvrom(1, 0x0800, (value << 1) + 0);
      mmc_bankvrom(1, 0x0C00, (value << 1) + 1);
      break;
   case 0x6006:
      mmc_bankvrom(1, 0x1000, (value << 1) + 0);
      mmc_bankvrom(1, 0x1400, (value << 1) + 1);
      break;
   case 0x6007:
      mmc_bankvrom(1, 0x1800, (value << 1) + 0);
      mmc_bankvrom(1, 0x1C00, (value << 1) + 1);
      break;
   default:
      /* Other SRAM addresses: pass-through (battery-backed save) */
      break;
   }
}

static void map246_init(void)
{
   int last = mmc_getinfo()->rom_banks * 2 - 1;
   mmc_bankrom(8, 0x8000, 0);
   mmc_bankrom(8, 0xA000, 1);
   mmc_bankrom(8, 0xC000, last - 1);
   mmc_bankrom(8, 0xE000, last);
}

static const map_memwrite map246_memwrite[] =
{
   { 0x6000, 0x7FFF, map246_sram },
   {     -1,     -1, NULL }
};

const mapintf_t map246_intf =
{
   246,               /* mapper number */
   "Phone Serm Berm", /* mapper name */
   map246_init,       /* init */
   NULL,              /* vblank */
   NULL,              /* hblank */
   NULL, NULL, NULL,
   map246_memwrite,
   NULL
};
