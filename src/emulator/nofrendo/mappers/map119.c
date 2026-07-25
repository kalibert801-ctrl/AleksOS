/*
** map119.c
** Mapper 119 — TQ-ROM (MMC3 with CHR-RAM banks)
**
** Converted from InfoNES_Mapper_119.cpp
** CHR bank registers use bit 6 to select RAM vs ROM.
** Simplification: mmc_bankvrom() is used for both RAM and ROM pages.
**   bit 6 set   → CHR-RAM bank (bank = chrX & 0x07)
**   bit 6 clear → CHR-ROM bank (bank = chrX)
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint8 reg[8];
static uint8 prg0, prg1;
static uint8 chr01, chr23, chr4, chr5, chr6, chr7;
static uint8 irq_enable, irq_cnt, irq_latch;

/* Select CHR bank: bit 6 → RAM (low 3 bits), else ROM */
#define CHRBANK(v) ((uint32)(((v) & 0x40) ? ((v) & 0x07) : (v)))

static void map119_set_cpu(void)
{
   int N = mmc_getinfo()->rom_banks * 2;
   if (reg[0] & 0x40)
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

static void map119_set_ppu(void)
{
   if (reg[0] & 0x80)
   {
      mmc_bankvrom(1, 0x0000, CHRBANK(chr4));
      mmc_bankvrom(1, 0x0400, CHRBANK(chr5));
      mmc_bankvrom(1, 0x0800, CHRBANK(chr6));
      mmc_bankvrom(1, 0x0C00, CHRBANK(chr7));
      mmc_bankvrom(1, 0x1000, CHRBANK(chr01 + 0));
      mmc_bankvrom(1, 0x1400, CHRBANK(chr01 + 1));
      mmc_bankvrom(1, 0x1800, CHRBANK(chr23 + 0));
      mmc_bankvrom(1, 0x1C00, CHRBANK(chr23 + 1));
   }
   else
   {
      mmc_bankvrom(1, 0x0000, CHRBANK(chr01 + 0));
      mmc_bankvrom(1, 0x0400, CHRBANK(chr01 + 1));
      mmc_bankvrom(1, 0x0800, CHRBANK(chr23 + 0));
      mmc_bankvrom(1, 0x0C00, CHRBANK(chr23 + 1));
      mmc_bankvrom(1, 0x1000, CHRBANK(chr4));
      mmc_bankvrom(1, 0x1400, CHRBANK(chr5));
      mmc_bankvrom(1, 0x1800, CHRBANK(chr6));
      mmc_bankvrom(1, 0x1C00, CHRBANK(chr7));
   }
}

static void map119_hblank(int vblank)
{
   if (!vblank && irq_enable)
   {
      if (irq_cnt == 0) { irq_cnt = irq_latch; nes_irq(); }
      else irq_cnt--;
   }
}

static void map119_write(uint32 address, uint8 value)
{
   switch (address & 0xE001)
   {
   case 0x8000:
      reg[0] = value;
      map119_set_cpu();
      map119_set_ppu();
      break;
   case 0x8001:
      reg[1] = value;
      switch (reg[0] & 0x07)
      {
      case 0: chr01 = value & 0xFE; map119_set_ppu(); break;
      case 1: chr23 = value & 0xFE; map119_set_ppu(); break;
      case 2: chr4  = value;        map119_set_ppu(); break;
      case 3: chr5  = value;        map119_set_ppu(); break;
      case 4: chr6  = value;        map119_set_ppu(); break;
      case 5: chr7  = value;        map119_set_ppu(); break;
      case 6: prg0  = value;        map119_set_cpu(); break;
      case 7: prg1  = value;        map119_set_cpu(); break;
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
      irq_cnt = irq_latch;   /* reload counter on disable (standard MMC3) */
      break;
   case 0xE001:
      reg[7] = value;
      irq_enable = 1;
      break;
   }
}

static void map119_init(void)
{
   int i;
   for (i = 0; i < 8; i++) reg[i] = 0;
   prg0 = 0; prg1 = 1;
   chr01 = 0; chr23 = 2;
   chr4 = 4; chr5 = 5; chr6 = 6; chr7 = 7;
   irq_enable = 0; irq_cnt = 0; irq_latch = 0;
   map119_set_cpu();
   map119_set_ppu();
}

static const map_memwrite map119_memwrite[] =
{
   { 0x8000, 0xFFFF, map119_write },
   {     -1,     -1, NULL }
};

const mapintf_t map119_intf =
{
   119,          /* mapper number */
   "TQ-ROM",     /* mapper name */
   map119_init,  /* init */
   NULL,         /* vblank */
   map119_hblank,/* hblank */
   NULL, NULL, NULL,
   map119_memwrite,
   NULL
};
