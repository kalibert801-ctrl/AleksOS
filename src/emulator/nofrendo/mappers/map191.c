/*
** map191.c
** Mapper 191 — SACHEN Super Cartridge / Q-BOY
**
** All banking controlled via $4100/$4101 register pair.
** $4100: register index select
** $4101: register value
**   0-3: CHR banks 0-3 (3-bit, select 4KB CHR blocks within Highbank)
**   4:   CHR high-bank select (3-bit, upper bits of CHR address)
**   5:   PRG bank select (3-bit, 32KB block)
**   7:   mirroring (bit 1: 1=horizontal, 0=vertical)
**
** PRG:  prg0 is a 32KB block index; prg0<<2 + 0..3 gives four 8KB page indices.
** CHR:  (highbank<<3 + chrN) is a 6-bit 4KB CHR bank index;
**       multiply by 4 and add sub-page offset to get 1KB VROM page.
*/

#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>

static uint8 reg[8];
static uint8 prg0;
static uint8 chr0, chr1, chr2, chr3;
static uint8 highbank;

static void map191_set_cpu(void)
{
   mmc_bankrom(8, 0x8000, (prg0 << 2) + 0);
   mmc_bankrom(8, 0xA000, (prg0 << 2) + 1);
   mmc_bankrom(8, 0xC000, (prg0 << 2) + 2);
   mmc_bankrom(8, 0xE000, (prg0 << 2) + 3);
}

static void map191_set_ppu(void)
{
   mmc_bankvrom(1, 0x0000, ((highbank << 3) + chr0) * 4 + 0);
   mmc_bankvrom(1, 0x0400, ((highbank << 3) + chr0) * 4 + 1);
   mmc_bankvrom(1, 0x0800, ((highbank << 3) + chr1) * 4 + 2);
   mmc_bankvrom(1, 0x0C00, ((highbank << 3) + chr1) * 4 + 3);
   mmc_bankvrom(1, 0x1000, ((highbank << 3) + chr2) * 4 + 0);
   mmc_bankvrom(1, 0x1400, ((highbank << 3) + chr2) * 4 + 1);
   mmc_bankvrom(1, 0x1800, ((highbank << 3) + chr3) * 4 + 2);
   mmc_bankvrom(1, 0x1C00, ((highbank << 3) + chr3) * 4 + 3);
}

static void map191_apu_write(uint32 address, uint8 value)
{
   switch (address)
   {
   case 0x4100:
      reg[0] = value;
      break;
   case 0x4101:
      reg[1] = value;
      switch (reg[0])
      {
      case 0: chr0     = value & 7; map191_set_ppu(); break;
      case 1: chr1     = value & 7; map191_set_ppu(); break;
      case 2: chr2     = value & 7; map191_set_ppu(); break;
      case 3: chr3     = value & 7; map191_set_ppu(); break;
      case 4: highbank = value & 7; map191_set_ppu(); break;
      case 5: prg0     = value & 7; map191_set_cpu(); break;
      case 7:
         if (value & 0x02)
            ppu_mirror(0, 0, 1, 1); /* horizontal */
         else
            ppu_mirror(0, 1, 0, 1); /* vertical */
         break;
      }
      break;
   }
}

static void map191_init(void)
{
   int i;
   for (i = 0; i < 8; i++) reg[i] = 0;
   prg0 = chr0 = chr1 = chr2 = chr3 = highbank = 0;
   map191_set_cpu();
   map191_set_ppu();
}

static const map_memwrite map191_memwrite[] =
{
   { 0x4100, 0x4FFF, map191_apu_write },
   {     -1,     -1, NULL }
};

const mapintf_t map191_intf =
{
   191,
   "SACHEN/Q-BOY",
   map191_init,
   NULL, NULL, NULL, NULL, NULL,
   map191_memwrite,
   NULL
};
