/*
** map243.c
** Mapper 243 — Pirates (APU-port controlled)
**
** reg[0] = index register (written to $4100)
** reg[1] = mirror bit
** reg[2] = CHR bank bits from cases 0x00/0x04/0x07
** reg[3] = CHR bank bits from case 0x06
**
** PRG/CHR are set by writes to $4101 indexed by reg[0] & 0x07.
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint8 reg243[4];

static void map243_set_ppu(void)
{
   uint8 chr_bank;
   int   base;

   if (mmc_getinfo()->vrom_banks <= 8) {
      /* <= 64 1KB pages */
      chr_bank = (reg243[2] + reg243[3]) >> 1;
   } else {
      chr_bank = reg243[1] + reg243[2] + reg243[3];
   }

   base = chr_bank * 8;
   mmc_bankvrom(1, 0x0000, base + 0);
   mmc_bankvrom(1, 0x0400, base + 1);
   mmc_bankvrom(1, 0x0800, base + 2);
   mmc_bankvrom(1, 0x0C00, base + 3);
   mmc_bankvrom(1, 0x1000, base + 4);
   mmc_bankvrom(1, 0x1400, base + 5);
   mmc_bankvrom(1, 0x1800, base + 6);
   mmc_bankvrom(1, 0x1C00, base + 7);
}

static void map243_apu(uint32 address, uint8 value)
{
   if (address == 0x4100) {
      reg243[0] = value;
   } else if (address == 0x4101) {
      switch (reg243[0] & 0x07) {
      case 0x02:
         reg243[1] = value & 0x01;
         break;
      case 0x00:
      case 0x04:
      case 0x07:
         reg243[2] = (value & 0x01) << 1;
         break;
      case 0x05:
         mmc_bankrom(8, 0x8000, value * 4 + 0);
         mmc_bankrom(8, 0xA000, value * 4 + 1);
         mmc_bankrom(8, 0xC000, value * 4 + 2);
         mmc_bankrom(8, 0xE000, value * 4 + 3);
         break;
      case 0x06:
         reg243[3] = (value & 0x03) << 2;
         break;
      }
      map243_set_ppu();
   }
}

static void map243_init(void)
{
   reg243[0] = reg243[1] = reg243[2] = reg243[3] = 0;
   mmc_bankrom(8, 0x8000, 0);
   mmc_bankrom(8, 0xA000, 1);
   mmc_bankrom(8, 0xC000, 2);
   mmc_bankrom(8, 0xE000, 3);
   if (mmc_getinfo()->vrom_banks > 0) {
      mmc_bankvrom(1, 0x0000, 0); mmc_bankvrom(1, 0x0400, 1);
      mmc_bankvrom(1, 0x0800, 2); mmc_bankvrom(1, 0x0C00, 3);
      mmc_bankvrom(1, 0x1000, 4); mmc_bankvrom(1, 0x1400, 5);
      mmc_bankvrom(1, 0x1800, 6); mmc_bankvrom(1, 0x1C00, 7);
   }
}

static const map_memwrite map243_memwrite[] =
{
   { 0x4100, 0x4FFF, map243_apu },
   {     -1,     -1, NULL }
};

const mapintf_t map243_intf =
{
   243,           /* mapper number */
   "Pirates 243", /* mapper name */
   map243_init,   /* init */
   NULL,          /* vblank */
   NULL,          /* hblank */
   NULL, NULL, NULL,
   map243_memwrite,
   NULL
};
