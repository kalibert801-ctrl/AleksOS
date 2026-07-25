/*
** map074.c
** Mapper 74 — Metal Max (MMC3-like with complex IRQ)
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint8  reg74[8];
static uint32 reg74_prg0, reg74_prg1;
static uint32 reg74_chr01, reg74_chr23;
static uint32 reg74_chr4, reg74_chr5, reg74_chr6, reg74_chr7;

static uint8 reg74_irq_enable;
static uint8 reg74_irq_cnt;
static uint8 reg74_irq_latch;
static uint8 reg74_irq_request;
static uint8 reg74_irq_present;

#define MAP74_CHR_SWAP() (reg74[0] & 0x80)
#define MAP74_PRG_SWAP() (reg74[0] & 0x40)

static void map74_set_cpu(void)
{
   int last = mmc_getinfo()->rom_banks * 2 - 1;
   if (MAP74_PRG_SWAP())
   {
      mmc_bankrom(8, 0x8000, last - 1);
      mmc_bankrom(8, 0xA000, reg74_prg1);
      mmc_bankrom(8, 0xC000, reg74_prg0);
      mmc_bankrom(8, 0xE000, last);
   }
   else
   {
      mmc_bankrom(8, 0x8000, reg74_prg0);
      mmc_bankrom(8, 0xA000, reg74_prg1);
      mmc_bankrom(8, 0xC000, last - 1);
      mmc_bankrom(8, 0xE000, last);
   }
}

static void map74_set_ppu(void)
{
   if (MAP74_CHR_SWAP())
   {
      mmc_bankvrom(1, 0x0000, reg74_chr4);
      mmc_bankvrom(1, 0x0400, reg74_chr5);
      mmc_bankvrom(1, 0x0800, reg74_chr6);
      mmc_bankvrom(1, 0x0C00, reg74_chr7);
      mmc_bankvrom(1, 0x1000, reg74_chr01 + 0);
      mmc_bankvrom(1, 0x1400, reg74_chr01 + 1);
      mmc_bankvrom(1, 0x1800, reg74_chr23 + 0);
      mmc_bankvrom(1, 0x1C00, reg74_chr23 + 1);
   }
   else
   {
      mmc_bankvrom(1, 0x0000, reg74_chr01 + 0);
      mmc_bankvrom(1, 0x0400, reg74_chr01 + 1);
      mmc_bankvrom(1, 0x0800, reg74_chr23 + 0);
      mmc_bankvrom(1, 0x0C00, reg74_chr23 + 1);
      mmc_bankvrom(1, 0x1000, reg74_chr4);
      mmc_bankvrom(1, 0x1400, reg74_chr5);
      mmc_bankvrom(1, 0x1800, reg74_chr6);
      mmc_bankvrom(1, 0x1C00, reg74_chr7);
   }
}

static void map74_hblank(int vblank)
{
   if (!vblank)
   {
      if (reg74_irq_present)
      {
         reg74_irq_cnt     = reg74_irq_latch;
         reg74_irq_present = 0;
      }
      else if (reg74_irq_cnt > 0)
      {
         reg74_irq_cnt--;
      }

      if (reg74_irq_cnt == 0)
      {
         if (reg74_irq_enable)
            reg74_irq_request = 0xFF;
         reg74_irq_present = 0xFF;
      }
   }
   if (reg74_irq_request)
      nes_irq();
}

static void map74_write(uint32 address, uint8 value)
{
   switch (address & 0xE001)
   {
   case 0x8000:
      reg74[0] = value;
      map74_set_ppu();
      map74_set_cpu();
      break;
   case 0x8001:
      reg74[1] = value;
      switch (reg74[0] & 0x07)
      {
      case 0: reg74_chr01 = value & 0xFE; map74_set_ppu(); break;
      case 1: reg74_chr23 = value & 0xFE; map74_set_ppu(); break;
      case 2: reg74_chr4  = value;        map74_set_ppu(); break;
      case 3: reg74_chr5  = value;        map74_set_ppu(); break;
      case 4: reg74_chr6  = value;        map74_set_ppu(); break;
      case 5: reg74_chr7  = value;        map74_set_ppu(); break;
      case 6: reg74_prg0  = value;        map74_set_cpu(); break;
      case 7: reg74_prg1  = value;        map74_set_cpu(); break;
      }
      break;
   case 0xA000:
      reg74[2] = value;
      if (value & 0x01) ppu_mirror(0, 0, 1, 1);
      else              ppu_mirror(0, 1, 0, 1);
      break;
   case 0xA001:
      reg74[3] = value;
      break;
   case 0xC000:
      reg74[4]       = value;
      reg74_irq_latch = value;
      break;
   case 0xC001:
      reg74[5] = value;
      reg74_irq_cnt     |= 0x80;
      reg74_irq_present  = 0xFF;
      break;
   case 0xE000:
      reg74[6]          = value;
      reg74_irq_enable   = 0;
      reg74_irq_request  = 0;
      break;
   case 0xE001:
      reg74[7]          = value;
      reg74_irq_enable   = 1;
      reg74_irq_request  = 0;
      break;
   }
}

static void map74_init(void)
{
   int i;
   for (i = 0; i < 8; i++) reg74[i] = 0;
   reg74_prg0 = 0; reg74_prg1 = 1;
   reg74_chr01 = 0; reg74_chr23 = 2;
   reg74_chr4 = 4; reg74_chr5 = 5; reg74_chr6 = 6; reg74_chr7 = 7;
   reg74_irq_enable  = 0; reg74_irq_cnt = 0; reg74_irq_latch  = 0;
   reg74_irq_request = 0; reg74_irq_present = 0;
   map74_set_cpu();
   map74_set_ppu();
}

static const map_memwrite map74_memwrite[] =
{
   { 0x8000, 0xFFFF, map74_write },
   {     -1,     -1, NULL }
};

const mapintf_t map74_intf =
{
   74,            /* mapper number */
   "Metal Max",   /* mapper name */
   map74_init,    /* init */
   NULL,          /* vblank */
   map74_hblank,  /* hblank */
   NULL, NULL, NULL,
   map74_memwrite,
   NULL
};
