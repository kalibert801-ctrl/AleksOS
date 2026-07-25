/*
** map112.c
** Mapper 112 — Pirates (MMC3-like, command at $8000, data at $A000)
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint8  reg112[8];
static uint32 reg112_prg0, reg112_prg1;
static uint32 reg112_chr01, reg112_chr23;
static uint32 reg112_chr4, reg112_chr5, reg112_chr6, reg112_chr7;
static uint8  reg112_irq_enable;
static uint8  reg112_irq_cnt;
static uint8  reg112_irq_latch;

#define MAP112_CHR_SWAP() (reg112[0] & 0x80)
#define MAP112_PRG_SWAP() (reg112[0] & 0x40)

static void map112_set_cpu(void)
{
   int last = mmc_getinfo()->rom_banks * 2 - 1;
   if (MAP112_PRG_SWAP())
   {
      mmc_bankrom(8, 0x8000, last - 1);
      mmc_bankrom(8, 0xA000, reg112_prg1);
      mmc_bankrom(8, 0xC000, reg112_prg0);
      mmc_bankrom(8, 0xE000, last);
   }
   else
   {
      mmc_bankrom(8, 0x8000, reg112_prg0);
      mmc_bankrom(8, 0xA000, reg112_prg1);
      mmc_bankrom(8, 0xC000, last - 1);
      mmc_bankrom(8, 0xE000, last);
   }
}

static void map112_set_ppu(void)
{
   if (MAP112_CHR_SWAP())
   {
      mmc_bankvrom(1, 0x0000, reg112_chr4);
      mmc_bankvrom(1, 0x0400, reg112_chr5);
      mmc_bankvrom(1, 0x0800, reg112_chr6);
      mmc_bankvrom(1, 0x0C00, reg112_chr7);
      mmc_bankvrom(1, 0x1000, reg112_chr01 + 0);
      mmc_bankvrom(1, 0x1400, reg112_chr01 + 1);
      mmc_bankvrom(1, 0x1800, reg112_chr23 + 0);
      mmc_bankvrom(1, 0x1C00, reg112_chr23 + 1);
   }
   else
   {
      mmc_bankvrom(1, 0x0000, reg112_chr01 + 0);
      mmc_bankvrom(1, 0x0400, reg112_chr01 + 1);
      mmc_bankvrom(1, 0x0800, reg112_chr23 + 0);
      mmc_bankvrom(1, 0x0C00, reg112_chr23 + 1);
      mmc_bankvrom(1, 0x1000, reg112_chr4);
      mmc_bankvrom(1, 0x1400, reg112_chr5);
      mmc_bankvrom(1, 0x1800, reg112_chr6);
      mmc_bankvrom(1, 0x1C00, reg112_chr7);
   }
}

static void map112_hblank(int vblank)
{
   if (!vblank && reg112_irq_enable)
   {
      if (!(reg112_irq_cnt--))
      {
         reg112_irq_cnt = reg112_irq_latch;
         nes_irq();
      }
   }
}

static void map112_write(uint32 address, uint8 value)
{
   switch (address & 0xE001)
   {
   case 0x8000:
      reg112[0] = value;
      map112_set_ppu();
      map112_set_cpu();
      break;
   case 0xA000:
      reg112[1] = value;
      switch (reg112[0] & 0x07)
      {
      case 0: reg112_prg0  = value;        map112_set_cpu(); break;
      case 1: reg112_prg1  = value;        map112_set_cpu(); break;
      case 2: reg112_chr01 = value & 0xFE; map112_set_ppu(); break;
      case 3: reg112_chr23 = value & 0xFE; map112_set_ppu(); break;
      case 4: reg112_chr4  = value;        map112_set_ppu(); break;
      case 5: reg112_chr5  = value;        map112_set_ppu(); break;
      case 6: reg112_chr6  = value;        map112_set_ppu(); break;
      case 7: reg112_chr7  = value;        map112_set_ppu(); break;
      }
      break;
   case 0x8001:
      reg112[2] = value;
      if (value & 0x01) ppu_mirror(0, 1, 0, 1); /* vertical */
      else              ppu_mirror(0, 0, 1, 1); /* horizontal */
      break;
   case 0xA001:
      reg112[3] = value;
      break;
   case 0xC000:
      reg112[4] = value;
      reg112_irq_cnt = value;
      break;
   case 0xC001:
      reg112[5] = value;
      reg112_irq_latch = value;
      break;
   case 0xE000:
      reg112[6] = value;
      reg112_irq_enable = 0;
      if (value) ppu_mirror(0, 0, 1, 1); /* horizontal */
      else       ppu_mirror(0, 1, 0, 1); /* vertical */
      break;
   case 0xE001:
      reg112[7] = value;
      reg112_irq_enable = 1;
      break;
   }
}

static void map112_init(void)
{
   int i;
   for (i = 0; i < 8; i++) reg112[i] = 0;
   reg112_prg0 = 0; reg112_prg1 = 1;
   reg112_chr01 = 0; reg112_chr23 = 2;
   reg112_chr4 = 4; reg112_chr5 = 5; reg112_chr6 = 6; reg112_chr7 = 7;
   reg112_irq_enable = 0; reg112_irq_cnt = 0; reg112_irq_latch = 0;
   map112_set_cpu();
   map112_set_ppu();
}

static const map_memwrite map112_memwrite[] =
{
   { 0x8000, 0xFFFF, map112_write },
   {     -1,     -1, NULL }
};

const mapintf_t map112_intf =
{
   112,            /* mapper number */
   "Pirates",      /* mapper name */
   map112_init,    /* init */
   NULL,           /* vblank */
   map112_hblank,  /* hblank */
   NULL, NULL, NULL,
   map112_memwrite,
   NULL
};
