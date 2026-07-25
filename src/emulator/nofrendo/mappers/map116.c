/*
** map116.c
** Mapper 116 — Somari / multi-mode (MMC3 portion implemented)
**
** Converted from InfoNES_Mapper_116.cpp
** This implements the MMC3-compatible portion of the multi-mode mapper.
** No SRAM write handler — CHR bank high bit is hardwired to 0.
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint8  reg[8];
static uint8  prg0, prg1;
static uint8  chr0, chr1, chr2, chr3;
static uint8  chr4, chr5, chr6, chr7;
static uint8  irq_enable, irq_cnt, irq_latch;

static void map116_set_cpu(void)
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

static void map116_set_ppu(void)
{
   if (reg[0] & 0x80)
   {
      mmc_bankvrom(1, 0x0000, chr4);
      mmc_bankvrom(1, 0x0400, chr5);
      mmc_bankvrom(1, 0x0800, chr6);
      mmc_bankvrom(1, 0x0C00, chr7);
      mmc_bankvrom(1, 0x1000, chr0);
      mmc_bankvrom(1, 0x1400, chr1);
      mmc_bankvrom(1, 0x1800, chr2);
      mmc_bankvrom(1, 0x1C00, chr3);
   }
   else
   {
      mmc_bankvrom(1, 0x0000, chr0);
      mmc_bankvrom(1, 0x0400, chr1);
      mmc_bankvrom(1, 0x0800, chr2);
      mmc_bankvrom(1, 0x0C00, chr3);
      mmc_bankvrom(1, 0x1000, chr4);
      mmc_bankvrom(1, 0x1400, chr5);
      mmc_bankvrom(1, 0x1800, chr6);
      mmc_bankvrom(1, 0x1C00, chr7);
   }
}

static void map116_hblank(int vblank)
{
   if (!vblank && irq_enable)
   {
      if (irq_cnt == 0) { irq_cnt = irq_latch; nes_irq(); }
      else irq_cnt--;
   }
}

static void map116_write(uint32 address, uint8 value)
{
   switch (address & 0xE001)
   {
   case 0x8000:
      reg[0] = value;
      map116_set_cpu();
      map116_set_ppu();
      break;
   case 0x8001:
      reg[1] = value;
      switch (reg[0] & 0x07)
      {
      case 0:
         chr0 = value & 0xFE; chr1 = chr0 + 1;
         map116_set_ppu();
         break;
      case 1:
         chr2 = value & 0xFE; chr3 = chr2 + 1;
         map116_set_ppu();
         break;
      case 2: chr4 = value; map116_set_ppu(); break;
      case 3: chr5 = value; map116_set_ppu(); break;
      case 4: chr6 = value; map116_set_ppu(); break;
      case 5: chr7 = value; map116_set_ppu(); break;
      case 6: prg0 = value; map116_set_cpu(); break;
      case 7: prg1 = value; map116_set_cpu(); break;
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

static void map116_init(void)
{
   int i;
   for (i = 0; i < 8; i++) reg[i] = 0;
   prg0 = 0; prg1 = 1;
   chr0 = 0; chr1 = 1; chr2 = 2; chr3 = 3;
   chr4 = 4; chr5 = 5; chr6 = 6; chr7 = 7;
   irq_enable = 0; irq_cnt = 0; irq_latch = 0;
   map116_set_cpu();
   map116_set_ppu();
}

static const map_memwrite map116_memwrite[] =
{
   { 0x8000, 0xFFFF, map116_write },
   {     -1,     -1, NULL }
};

const mapintf_t map116_intf =
{
   116,          /* mapper number */
   "Somari-MMC3",/* mapper name */
   map116_init,  /* init */
   NULL,         /* vblank */
   map116_hblank,/* hblank */
   NULL, NULL, NULL,
   map116_memwrite,
   NULL
};
