/*
** map114.c
** Mapper 114 — PC-SuperGames (MMC3-like pirate)
**
** Converted from InfoNES_Mapper_114.cpp
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint8  reg[8];
static uint32 prg0, prg1;
static uint32 chr01, chr23, chr4, chr5, chr6, chr7;
static uint8  irq_enable, irq_cnt, irq_latch;

#define MAP114_CHR_SWAP() (reg[0] & 0x80)
#define MAP114_PRG_SWAP() (reg[0] & 0x40)

static void map114_set_cpu(void)
{
   int N = mmc_getinfo()->rom_banks * 2;
   if (MAP114_PRG_SWAP())
   {
      mmc_bankrom(8, 0x8000, N - 2);
      mmc_bankrom(8, 0xA000, prg1);
      mmc_bankrom(8, 0xC000, prg0);
      mmc_bankrom(8, 0xE000, N - 1);
   }
   else
   {
      mmc_bankrom(8, 0x8000, prg0);
      mmc_bankrom(8, 0xA000, prg1);
      mmc_bankrom(8, 0xC000, N - 2);
      mmc_bankrom(8, 0xE000, N - 1);
   }
}

static void map114_set_ppu(void)
{
   if (MAP114_CHR_SWAP())
   {
      mmc_bankvrom(1, 0x0000, chr4);
      mmc_bankvrom(1, 0x0400, chr5);
      mmc_bankvrom(1, 0x0800, chr6);
      mmc_bankvrom(1, 0x0C00, chr7);
      mmc_bankvrom(1, 0x1000, chr01 + 0);
      mmc_bankvrom(1, 0x1400, chr01 + 1);
      mmc_bankvrom(1, 0x1800, chr23 + 0);
      mmc_bankvrom(1, 0x1C00, chr23 + 1);
   }
   else
   {
      mmc_bankvrom(1, 0x0000, chr01 + 0);
      mmc_bankvrom(1, 0x0400, chr01 + 1);
      mmc_bankvrom(1, 0x0800, chr23 + 0);
      mmc_bankvrom(1, 0x0C00, chr23 + 1);
      mmc_bankvrom(1, 0x1000, chr4);
      mmc_bankvrom(1, 0x1400, chr5);
      mmc_bankvrom(1, 0x1800, chr6);
      mmc_bankvrom(1, 0x1C00, chr7);
   }
}

static void map114_hblank(int vblank)
{
   if (!vblank && irq_enable)
   {
      if (irq_cnt == 0) { irq_cnt = irq_latch; nes_irq(); }
      else irq_cnt--;
   }
}

static void map114_sram(uint32 address, uint8 value)
{
   /* Reset all state when $6000 is written with 0x00 */
   if (address == 0x6000 && value == 0x00)
   {
      int i;
      for (i = 0; i < 8; i++) reg[i] = 0;
      prg0 = 0; prg1 = 1;
      chr01 = 0; chr23 = 2;
      chr4 = 4; chr5 = 5; chr6 = 6; chr7 = 7;
      irq_enable = 0; irq_cnt = 0; irq_latch = 0;
      map114_set_cpu();
      map114_set_ppu();
   }
}

static void map114_write(uint32 address, uint8 value)
{
   uint32 banknum;
   switch (address & 0xE001)
   {
   case 0x8000:
      reg[0] = value;
      map114_set_ppu();
      map114_set_cpu();
      break;
   case 0x8001:
      reg[1] = value;
      banknum = value;
      switch (reg[0] & 0x07)
      {
      case 0: chr01 = banknum & 0xFE; map114_set_ppu(); break;
      case 1: chr23 = banknum & 0xFE; map114_set_ppu(); break;
      case 2: chr4  = banknum;        map114_set_ppu(); break;
      case 3: chr5  = banknum;        map114_set_ppu(); break;
      case 4: chr6  = banknum;        map114_set_ppu(); break;
      case 5: chr7  = banknum;        map114_set_ppu(); break;
      case 6: prg0  = banknum;        map114_set_cpu(); break;
      case 7: prg1  = banknum;        map114_set_cpu(); break;
      }
      break;
   case 0xA000:
      reg[2] = value;
      if (0 == (mmc_getinfo()->flags & ROM_FLAG_FOURSCREEN))
      {
         if (value & 0x01) ppu_mirror(0, 0, 1, 1); /* horizontal */
         else              ppu_mirror(0, 1, 0, 1); /* vertical */
      }
      break;
   case 0xA001:
      reg[3] = value;
      break;
   case 0xC000:
      reg[4] = value;
      irq_cnt = value;
      break;
   case 0xC001:
      reg[5] = value;
      irq_latch = value;
      break;
   case 0xE000:
      reg[6] = value;
      irq_enable = 0;
      break;
   case 0xE001:
      reg[7] = value;
      irq_enable = 1;
      break;
   }
}

static void map114_init(void)
{
   int i;
   for (i = 0; i < 8; i++) reg[i] = 0;
   prg0 = 0; prg1 = 1;
   chr01 = 0; chr23 = 2;
   chr4 = 4; chr5 = 5; chr6 = 6; chr7 = 7;
   irq_enable = 0; irq_cnt = 0; irq_latch = 0;
   map114_set_cpu();
   map114_set_ppu();
}

static const map_memwrite map114_memwrite[] =
{
   { 0x6000, 0x7FFF, map114_sram  },
   { 0x8000, 0xFFFF, map114_write },
   {     -1,     -1, NULL }
};

const mapintf_t map114_intf =
{
   114,             /* mapper number */
   "PC-SuperGames", /* mapper name */
   map114_init,     /* init */
   NULL,            /* vblank */
   map114_hblank,   /* hblank */
   NULL, NULL, NULL,
   map114_memwrite,
   NULL
};
