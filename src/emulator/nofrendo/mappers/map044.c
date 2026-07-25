/*
** map044.c
** Mapper 44 — Nin1 (MMC3-like with bank-block multiplier)
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint8  reg44[8];
static uint32 reg44_rom_bank;
static uint32 reg44_prg0, reg44_prg1;
static uint32 reg44_chr01, reg44_chr23;
static uint32 reg44_chr4, reg44_chr5, reg44_chr6, reg44_chr7;
static uint8  reg44_irq_enable;
static uint8  reg44_irq_cnt;
static uint8  reg44_irq_latch;

#define MAP44_CHR_SWAP() (reg44[0] & 0x80)
#define MAP44_PRG_SWAP() (reg44[0] & 0x40)

static void map44_set_cpu(void)
{
   if (MAP44_PRG_SWAP())
   {
      mmc_bankrom(8, 0x8000, ((reg44_rom_bank << 4) + 14));
      mmc_bankrom(8, 0xA000, ((reg44_rom_bank << 4) + reg44_prg1));
      mmc_bankrom(8, 0xC000, ((reg44_rom_bank << 4) + reg44_prg0));
      mmc_bankrom(8, 0xE000, ((reg44_rom_bank << 4) + 15));
   }
   else
   {
      mmc_bankrom(8, 0x8000, ((reg44_rom_bank << 4) + reg44_prg0));
      mmc_bankrom(8, 0xA000, ((reg44_rom_bank << 4) + reg44_prg1));
      mmc_bankrom(8, 0xC000, ((reg44_rom_bank << 4) + 14));
      mmc_bankrom(8, 0xE000, ((reg44_rom_bank << 4) + 15));
   }
}

static void map44_set_ppu(void)
{
   if (MAP44_CHR_SWAP())
   {
      mmc_bankvrom(1, 0x0000, (reg44_rom_bank << 7) + reg44_chr4);
      mmc_bankvrom(1, 0x0400, (reg44_rom_bank << 7) + reg44_chr5);
      mmc_bankvrom(1, 0x0800, (reg44_rom_bank << 7) + reg44_chr6);
      mmc_bankvrom(1, 0x0C00, (reg44_rom_bank << 7) + reg44_chr7);
      mmc_bankvrom(1, 0x1000, (reg44_rom_bank << 7) + reg44_chr01 + 0);
      mmc_bankvrom(1, 0x1400, (reg44_rom_bank << 7) + reg44_chr01 + 1);
      mmc_bankvrom(1, 0x1800, (reg44_rom_bank << 7) + reg44_chr23 + 0);
      mmc_bankvrom(1, 0x1C00, (reg44_rom_bank << 7) + reg44_chr23 + 1);
   }
   else
   {
      mmc_bankvrom(1, 0x0000, (reg44_rom_bank << 7) + reg44_chr01 + 0);
      mmc_bankvrom(1, 0x0400, (reg44_rom_bank << 7) + reg44_chr01 + 1);
      mmc_bankvrom(1, 0x0800, (reg44_rom_bank << 7) + reg44_chr23 + 0);
      mmc_bankvrom(1, 0x0C00, (reg44_rom_bank << 7) + reg44_chr23 + 1);
      mmc_bankvrom(1, 0x1000, (reg44_rom_bank << 7) + reg44_chr4);
      mmc_bankvrom(1, 0x1400, (reg44_rom_bank << 7) + reg44_chr5);
      mmc_bankvrom(1, 0x1800, (reg44_rom_bank << 7) + reg44_chr6);
      mmc_bankvrom(1, 0x1C00, (reg44_rom_bank << 7) + reg44_chr7);
   }
}

static void map44_hblank(int vblank)
{
   if (!vblank && reg44_irq_enable)
   {
      if (!(reg44_irq_cnt--))
      {
         reg44_irq_cnt = reg44_irq_latch;
         nes_irq();
      }
   }
}

static void map44_write(uint32 address, uint8 value)
{
   switch (address & 0xE001)
   {
   case 0x8000:
      reg44[0] = value;
      map44_set_ppu();
      map44_set_cpu();
      break;
   case 0x8001:
      reg44[1] = value;
      switch (reg44[0] & 0x07)
      {
      case 0: reg44_chr01 = value & 0xFE; map44_set_ppu(); break;
      case 1: reg44_chr23 = value & 0xFE; map44_set_ppu(); break;
      case 2: reg44_chr4  = value;        map44_set_ppu(); break;
      case 3: reg44_chr5  = value;        map44_set_ppu(); break;
      case 4: reg44_chr6  = value;        map44_set_ppu(); break;
      case 5: reg44_chr7  = value;        map44_set_ppu(); break;
      case 6: reg44_prg0  = value;        map44_set_cpu(); break;
      case 7: reg44_prg1  = value;        map44_set_cpu(); break;
      }
      break;
   case 0xA000:
      reg44[2] = value;
      if (value & 0x01) ppu_mirror(0, 0, 1, 1);
      else              ppu_mirror(0, 1, 0, 1);
      break;
   case 0xA001:
      {
         uint32 rb = value & 0x07;
         if (rb == 7) rb = 6;
         reg44_rom_bank = rb;
         map44_set_cpu();
         map44_set_ppu();
      }
      break;
   case 0xC000:
      reg44[4] = value;
      reg44_irq_cnt = value;
      break;
   case 0xC001:
      reg44[5] = value;
      reg44_irq_latch = value;
      break;
   case 0xE000:
      reg44[6] = value;
      reg44_irq_enable = 0;
      break;
   case 0xE001:
      reg44[7] = value;
      reg44_irq_enable = 1;
      break;
   }
}

static void map44_init(void)
{
   int i;
   for (i = 0; i < 8; i++) reg44[i] = 0;
   reg44_rom_bank = 0;
   reg44_prg0 = 0; reg44_prg1 = 1;
   reg44_chr01 = 0; reg44_chr23 = 2;
   reg44_chr4 = 4; reg44_chr5 = 5; reg44_chr6 = 6; reg44_chr7 = 7;
   reg44_irq_enable = 0; reg44_irq_cnt = 0; reg44_irq_latch = 0;
   map44_set_cpu();
   map44_set_ppu();
}

static const map_memwrite map44_memwrite[] =
{
   { 0x8000, 0xFFFF, map44_write },
   {     -1,     -1, NULL }
};

const mapintf_t map44_intf =
{
   44,            /* mapper number */
   "Nin1",        /* mapper name */
   map44_init,    /* init */
   NULL,          /* vblank */
   map44_hblank,  /* hblank */
   NULL, NULL, NULL,
   map44_memwrite,
   NULL
};
