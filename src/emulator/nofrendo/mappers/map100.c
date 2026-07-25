/*
** map100.c
** Mapper 100 — Nestile MMC3 (full 8-bank CHR + 4-bank PRG variant)
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint8 reg100[8];
static uint8 reg100_prg0, reg100_prg1, reg100_prg2, reg100_prg3;
static uint8 reg100_chr[8]; /* chr0..chr7 */
static uint8 reg100_irq_enable;
static uint8 reg100_irq_cnt;
static uint8 reg100_irq_latch;

static void map100_set_cpu(void)
{
   mmc_bankrom(8, 0x8000, reg100_prg0);
   mmc_bankrom(8, 0xA000, reg100_prg1);
   mmc_bankrom(8, 0xC000, reg100_prg2);
   mmc_bankrom(8, 0xE000, reg100_prg3);
}

static void map100_set_ppu(void)
{
   int i;
   for (i = 0; i < 8; i++)
      mmc_bankvrom(1, i * 0x400, reg100_chr[i]);
}

static void map100_hblank(int vblank)
{
   if (!vblank && reg100_irq_enable)
   {
      if (!(reg100_irq_cnt--))
      {
         reg100_irq_cnt = reg100_irq_latch;
         nes_irq();
      }
   }
}

static void map100_write(uint32 address, uint8 value)
{
   switch (address & 0xE001)
   {
   case 0x8000:
      reg100[0] = value;
      break;
   case 0x8001:
      reg100[1] = value;
      switch (reg100[0] & 0xC7)
      {
      case 0x00:
         reg100_chr[0] = value & 0xFE;
         reg100_chr[1] = reg100_chr[0] + 1;
         map100_set_ppu();
         break;
      case 0x01:
         reg100_chr[2] = value & 0xFE;
         reg100_chr[3] = reg100_chr[2] + 1;
         map100_set_ppu();
         break;
      case 0x02: reg100_chr[4] = value; map100_set_ppu(); break;
      case 0x03: reg100_chr[5] = value; map100_set_ppu(); break;
      case 0x04: reg100_chr[6] = value; map100_set_ppu(); break;
      case 0x05: reg100_chr[7] = value; map100_set_ppu(); break;
      case 0x06: reg100_prg0 = value; map100_set_cpu(); break;
      case 0x07: reg100_prg1 = value; map100_set_cpu(); break;
      case 0x46: reg100_prg2 = value; map100_set_cpu(); break;
      case 0x47: reg100_prg3 = value; map100_set_cpu(); break;
      case 0x80:
         reg100_chr[4] = value & 0xFE;
         reg100_chr[5] = reg100_chr[4] + 1;
         map100_set_ppu();
         break;
      case 0x81:
         reg100_chr[6] = value & 0xFE;
         reg100_chr[7] = reg100_chr[6] + 1;
         map100_set_ppu();
         break;
      case 0x82: reg100_chr[0] = value; map100_set_ppu(); break;
      case 0x83: reg100_chr[1] = value; map100_set_ppu(); break;
      case 0x84: reg100_chr[2] = value; map100_set_ppu(); break;
      case 0x85: reg100_chr[3] = value; map100_set_ppu(); break;
      default: break;
      }
      break;
   case 0xA000:
      reg100[2] = value;
      if (value & 0x01) ppu_mirror(0, 0, 1, 1);
      else              ppu_mirror(0, 1, 0, 1);
      break;
   case 0xA001:
      reg100[3] = value;
      break;
   case 0xC000:
      reg100[4] = value;
      reg100_irq_cnt = value;
      break;
   case 0xC001:
      reg100[5] = value;
      reg100_irq_latch = value;
      break;
   case 0xE000:
      reg100[6] = value;
      reg100_irq_enable = 0;
      break;
   case 0xE001:
      reg100[7] = value;
      reg100_irq_enable = 0xFF;
      break;
   }
}

static void map100_init(void)
{
   int i, last;
   for (i = 0; i < 8; i++) reg100[i] = 0;
   last = mmc_getinfo()->rom_banks * 2 - 1;
   reg100_prg0 = 0; reg100_prg1 = 1;
   reg100_prg2 = last - 1; reg100_prg3 = last;
   for (i = 0; i < 8; i++) reg100_chr[i] = i;
   reg100_irq_enable = 0; reg100_irq_cnt = 0; reg100_irq_latch = 0;
   map100_set_cpu();
   map100_set_ppu();
}

static const map_memwrite map100_memwrite[] =
{
   { 0x8000, 0xFFFF, map100_write },
   {     -1,     -1, NULL }
};

const mapintf_t map100_intf =
{
   100,             /* mapper number */
   "Nestile MMC3",  /* mapper name */
   map100_init,     /* init */
   NULL,            /* vblank */
   map100_hblank,   /* hblank */
   NULL, NULL, NULL,
   map100_memwrite,
   NULL
};
