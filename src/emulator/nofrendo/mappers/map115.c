/*
** map115.c
** Mapper 115 — CartSaint / Yong Zhe Dou E Long (MMC3-like + extended banking)
**
** Converted from InfoNES_Mapper_115.cpp
** ExPrgSwitch at $6000 selects a 32K ROM block when bit 7 is set.
** ExChrSwitch at $6001 adds bit 8 to all CHR bank numbers.
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint8  reg[8];
static uint8  prg0l, prg1l;           /* last MMC3-written PRG banks */
static uint8  chr0, chr1, chr2, chr3;
static uint8  chr4, chr5, chr6, chr7;
static uint8  irq_enable, irq_cnt, irq_latch;
static uint8  ex_prg_switch;          /* $6000 — bit 7 selects 32K block */
static uint8  ex_chr_switch;          /* $6001 — bit 0 is CHR bank high bit */

static void map115_set_cpu(void)
{
   int N = mmc_getinfo()->rom_banks * 2;
   if (ex_prg_switch & 0x80)
   {
      uint8 p0 = (uint8)((ex_prg_switch << 1) & 0x1E);
      uint8 p1 = p0 + 1;
      mmc_bankrom(8, 0x8000, p0);
      mmc_bankrom(8, 0xA000, p1);
      mmc_bankrom(8, 0xC000, p0 + 2);
      mmc_bankrom(8, 0xE000, p1 + 2);
   }
   else
   {
      if (reg[0] & 0x40)
      {
         mmc_bankrom(8, 0x8000, N - 2);
         mmc_bankrom(8, 0xA000, prg1l);
         mmc_bankrom(8, 0xC000, prg0l);
         mmc_bankrom(8, 0xE000, N - 1);
      }
      else
      {
         mmc_bankrom(8, 0x8000, prg0l);
         mmc_bankrom(8, 0xA000, prg1l);
         mmc_bankrom(8, 0xC000, N - 2);
         mmc_bankrom(8, 0xE000, N - 1);
      }
   }
}

static void map115_set_ppu(void)
{
   uint32 hi = (uint32)ex_chr_switch << 8;
   if (reg[0] & 0x80)
   {
      mmc_bankvrom(1, 0x0000, hi + chr4);
      mmc_bankvrom(1, 0x0400, hi + chr5);
      mmc_bankvrom(1, 0x0800, hi + chr6);
      mmc_bankvrom(1, 0x0C00, hi + chr7);
      mmc_bankvrom(1, 0x1000, hi + chr0);
      mmc_bankvrom(1, 0x1400, hi + chr1);
      mmc_bankvrom(1, 0x1800, hi + chr2);
      mmc_bankvrom(1, 0x1C00, hi + chr3);
   }
   else
   {
      mmc_bankvrom(1, 0x0000, hi + chr0);
      mmc_bankvrom(1, 0x0400, hi + chr1);
      mmc_bankvrom(1, 0x0800, hi + chr2);
      mmc_bankvrom(1, 0x0C00, hi + chr3);
      mmc_bankvrom(1, 0x1000, hi + chr4);
      mmc_bankvrom(1, 0x1400, hi + chr5);
      mmc_bankvrom(1, 0x1800, hi + chr6);
      mmc_bankvrom(1, 0x1C00, hi + chr7);
   }
}

static void map115_hblank(int vblank)
{
   if (!vblank && irq_enable)
   {
      if (irq_cnt == 0) { irq_cnt = irq_latch; nes_irq(); }
      else irq_cnt--;
   }
}

static void map115_sram(uint32 address, uint8 value)
{
   switch (address)
   {
   case 0x6000:
      ex_prg_switch = value;
      map115_set_cpu();
      break;
   case 0x6001:
      ex_chr_switch = value & 0x01;
      map115_set_ppu();
      break;
   }
}

static void map115_write(uint32 address, uint8 value)
{
   switch (address & 0xE001)
   {
   case 0x8000:
      reg[0] = value;
      map115_set_cpu();
      map115_set_ppu();
      break;
   case 0x8001:
      reg[1] = value;
      switch (reg[0] & 0x07)
      {
      case 0:
         chr0 = value & 0xFE; chr1 = chr0 + 1;
         map115_set_ppu();
         break;
      case 1:
         chr2 = value & 0xFE; chr3 = chr2 + 1;
         map115_set_ppu();
         break;
      case 2: chr4 = value; map115_set_ppu(); break;
      case 3: chr5 = value; map115_set_ppu(); break;
      case 4: chr6 = value; map115_set_ppu(); break;
      case 5: chr7 = value; map115_set_ppu(); break;
      case 6: prg0l = value; map115_set_cpu(); break;
      case 7: prg1l = value; map115_set_cpu(); break;
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
      irq_enable = 0xFF;   /* writing $C000 also enables IRQ */
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
      irq_enable = 0xFF;
      break;
   }
}

static void map115_init(void)
{
   int i;
   for (i = 0; i < 8; i++) reg[i] = 0;
   prg0l = 0; prg1l = 1;
   chr0 = 0; chr1 = 1; chr2 = 2; chr3 = 3;
   chr4 = 4; chr5 = 5; chr6 = 6; chr7 = 7;
   ex_prg_switch = 0;
   ex_chr_switch = 0;
   irq_enable = 0; irq_cnt = 0; irq_latch = 0;
   map115_set_cpu();
   map115_set_ppu();
}

static const map_memwrite map115_memwrite[] =
{
   { 0x6000, 0x7FFF, map115_sram  },
   { 0x8000, 0xFFFF, map115_write },
   {     -1,     -1, NULL }
};

const mapintf_t map115_intf =
{
   115,          /* mapper number */
   "CartSaint",  /* mapper name */
   map115_init,  /* init */
   NULL,         /* vblank */
   map115_hblank,/* hblank */
   NULL, NULL, NULL,
   map115_memwrite,
   NULL
};
