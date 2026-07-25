/*
** map047.c
** Mapper 47 — MMC (bank-block multiplier, SRAM selects block)
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint8  reg47[8];
static uint32 reg47_rom_bank;
static uint32 reg47_prg0, reg47_prg1;
static uint32 reg47_chr01, reg47_chr23;
static uint32 reg47_chr4, reg47_chr5, reg47_chr6, reg47_chr7;
static uint8  reg47_irq_enable;
static uint8  reg47_irq_cnt;
static uint8  reg47_irq_latch;

#define MAP47_CHR_SWAP() (reg47[0] & 0x80)
#define MAP47_PRG_SWAP() (reg47[0] & 0x40)

static void map47_set_cpu(void)
{
   if (MAP47_PRG_SWAP())
   {
      mmc_bankrom(8, 0x8000, (reg47_rom_bank << 4) + 14);
      mmc_bankrom(8, 0xA000, (reg47_rom_bank << 4) + reg47_prg1);
      mmc_bankrom(8, 0xC000, (reg47_rom_bank << 4) + reg47_prg0);
      mmc_bankrom(8, 0xE000, (reg47_rom_bank << 4) + 15);
   }
   else
   {
      mmc_bankrom(8, 0x8000, (reg47_rom_bank << 4) + reg47_prg0);
      mmc_bankrom(8, 0xA000, (reg47_rom_bank << 4) + reg47_prg1);
      mmc_bankrom(8, 0xC000, (reg47_rom_bank << 4) + 14);
      mmc_bankrom(8, 0xE000, (reg47_rom_bank << 4) + 15);
   }
}

static void map47_set_ppu(void)
{
   if (MAP47_CHR_SWAP())
   {
      mmc_bankvrom(1, 0x0000, (reg47_rom_bank << 7) + reg47_chr4);
      mmc_bankvrom(1, 0x0400, (reg47_rom_bank << 7) + reg47_chr5);
      mmc_bankvrom(1, 0x0800, (reg47_rom_bank << 7) + reg47_chr6);
      mmc_bankvrom(1, 0x0C00, (reg47_rom_bank << 7) + reg47_chr7);
      mmc_bankvrom(1, 0x1000, (reg47_rom_bank << 7) + reg47_chr01 + 0);
      mmc_bankvrom(1, 0x1400, (reg47_rom_bank << 7) + reg47_chr01 + 1);
      mmc_bankvrom(1, 0x1800, (reg47_rom_bank << 7) + reg47_chr23 + 0);
      mmc_bankvrom(1, 0x1C00, (reg47_rom_bank << 7) + reg47_chr23 + 1);
   }
   else
   {
      mmc_bankvrom(1, 0x0000, (reg47_rom_bank << 7) + reg47_chr01 + 0);
      mmc_bankvrom(1, 0x0400, (reg47_rom_bank << 7) + reg47_chr01 + 1);
      mmc_bankvrom(1, 0x0800, (reg47_rom_bank << 7) + reg47_chr23 + 0);
      mmc_bankvrom(1, 0x0C00, (reg47_rom_bank << 7) + reg47_chr23 + 1);
      mmc_bankvrom(1, 0x1000, (reg47_rom_bank << 7) + reg47_chr4);
      mmc_bankvrom(1, 0x1400, (reg47_rom_bank << 7) + reg47_chr5);
      mmc_bankvrom(1, 0x1800, (reg47_rom_bank << 7) + reg47_chr6);
      mmc_bankvrom(1, 0x1C00, (reg47_rom_bank << 7) + reg47_chr7);
   }
}

static void map47_hblank(int vblank)
{
   if (!vblank && reg47_irq_enable)
   {
      if (!(--reg47_irq_cnt))
      {
         reg47_irq_cnt = reg47_irq_latch;
         nes_irq();
      }
   }
}

static void map47_sram_write(uint32 address, uint8 value)
{
   if (address == 0x6000)
   {
      reg47_rom_bank = value & 0x01;
      map47_set_cpu();
      map47_set_ppu();
   }
}

static void map47_write(uint32 address, uint8 value)
{
   switch (address & 0xE001)
   {
   case 0x8000:
      reg47[0] = value;
      map47_set_ppu();
      map47_set_cpu();
      break;
   case 0x8001:
      reg47[1] = value;
      switch (reg47[0] & 0x07)
      {
      case 0: reg47_chr01 = value & 0xFE; map47_set_ppu(); break;
      case 1: reg47_chr23 = value & 0xFE; map47_set_ppu(); break;
      case 2: reg47_chr4  = value;        map47_set_ppu(); break;
      case 3: reg47_chr5  = value;        map47_set_ppu(); break;
      case 4: reg47_chr6  = value;        map47_set_ppu(); break;
      case 5: reg47_chr7  = value;        map47_set_ppu(); break;
      case 6: reg47_prg0  = value;        map47_set_cpu(); break;
      case 7: reg47_prg1  = value;        map47_set_cpu(); break;
      }
      break;
   case 0xC000:
      reg47_irq_cnt = value;
      break;
   case 0xC001:
      reg47_irq_latch = value;
      break;
   case 0xE000:
      reg47_irq_enable = 0;
      break;
   case 0xE001:
      reg47_irq_enable = 1;
      break;
   }
}

static void map47_init(void)
{
   int i;
   for (i = 0; i < 8; i++) reg47[i] = 0;
   reg47_rom_bank = 0;
   reg47_prg0 = 0; reg47_prg1 = 1;
   reg47_chr01 = 0; reg47_chr23 = 2;
   reg47_chr4 = 4; reg47_chr5 = 5; reg47_chr6 = 6; reg47_chr7 = 7;
   reg47_irq_enable = 0; reg47_irq_cnt = 0; reg47_irq_latch = 0;
   map47_set_cpu();
   map47_set_ppu();
}

static const map_memwrite map47_memwrite[] =
{
   { 0x6000, 0x7FFF, map47_sram_write },
   { 0x8000, 0xFFFF, map47_write },
   {     -1,     -1, NULL }
};

const mapintf_t map47_intf =
{
   47,            /* mapper number */
   "MMC",         /* mapper name */
   map47_init,    /* init */
   NULL,          /* vblank */
   map47_hblank,  /* hblank */
   NULL, NULL, NULL,
   map47_memwrite,
   NULL
};
