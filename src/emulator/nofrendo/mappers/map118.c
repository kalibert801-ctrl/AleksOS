/*
** map118.c
** Mapper 118 — Others (MMC3-like with per-bank mirroring)
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint8  reg118[8];
static uint32 reg118_prg0, reg118_prg1;
static uint32 reg118_chr[8]; /* chr0..chr7 */
static uint8  reg118_irq_enable;
static uint8  reg118_irq_cnt;
static uint8  reg118_irq_latch;

#define MAP118_CHR_SWAP() (reg118[0] & 0x80)
#define MAP118_PRG_SWAP() (reg118[0] & 0x40)

static void map118_set_cpu(void)
{
   int last = mmc_getinfo()->rom_banks * 2 - 1;
   if (MAP118_PRG_SWAP())
   {
      mmc_bankrom(8, 0x8000, last - 1);
      mmc_bankrom(8, 0xA000, reg118_prg1);
      mmc_bankrom(8, 0xC000, reg118_prg0);
      mmc_bankrom(8, 0xE000, last);
   }
   else
   {
      mmc_bankrom(8, 0x8000, reg118_prg0);
      mmc_bankrom(8, 0xA000, reg118_prg1);
      mmc_bankrom(8, 0xC000, last - 1);
      mmc_bankrom(8, 0xE000, last);
   }
}

static void map118_set_ppu(void)
{
   if (MAP118_CHR_SWAP())
   {
      mmc_bankvrom(1, 0x0000, reg118_chr[4]);
      mmc_bankvrom(1, 0x0400, reg118_chr[5]);
      mmc_bankvrom(1, 0x0800, reg118_chr[6]);
      mmc_bankvrom(1, 0x0C00, reg118_chr[7]);
      mmc_bankvrom(1, 0x1000, reg118_chr[0]);
      mmc_bankvrom(1, 0x1400, reg118_chr[1]);
      mmc_bankvrom(1, 0x1800, reg118_chr[2]);
      mmc_bankvrom(1, 0x1C00, reg118_chr[3]);
   }
   else
   {
      mmc_bankvrom(1, 0x0000, reg118_chr[0]);
      mmc_bankvrom(1, 0x0400, reg118_chr[1]);
      mmc_bankvrom(1, 0x0800, reg118_chr[2]);
      mmc_bankvrom(1, 0x0C00, reg118_chr[3]);
      mmc_bankvrom(1, 0x1000, reg118_chr[4]);
      mmc_bankvrom(1, 0x1400, reg118_chr[5]);
      mmc_bankvrom(1, 0x1800, reg118_chr[6]);
      mmc_bankvrom(1, 0x1C00, reg118_chr[7]);
   }
}

static void map118_hblank(int vblank)
{
   if (!vblank && reg118_irq_enable)
   {
      if (!(reg118_irq_cnt--))
      {
         reg118_irq_cnt = reg118_irq_latch;
         nes_irq();
      }
   }
}

static void map118_write(uint32 address, uint8 value)
{
   switch (address & 0xE001)
   {
   case 0x8000:
      reg118[0] = value;
      map118_set_ppu();
      map118_set_cpu();
      break;
   case 0x8001:
      reg118[1] = value;
      /* per-bank mirroring: bit 7 of CHR value controls single-screen mode */
      if ((reg118[0] & 0x07) < 6)
      {
         if (value & 0x80) ppu_mirror(1, 1, 1, 1); /* NT1 */
         else              ppu_mirror(0, 0, 0, 0); /* NT0 */
      }
      switch (reg118[0] & 0x07)
      {
      case 0:
         reg118_chr[0] = value & 0xFE;
         reg118_chr[1] = reg118_chr[0] + 1;
         map118_set_ppu();
         break;
      case 1:
         reg118_chr[2] = value & 0xFE;
         reg118_chr[3] = reg118_chr[2] + 1;
         map118_set_ppu();
         break;
      case 2: reg118_chr[4] = value; map118_set_ppu(); break;
      case 3: reg118_chr[5] = value; map118_set_ppu(); break;
      case 4: reg118_chr[6] = value; map118_set_ppu(); break;
      case 5: reg118_chr[7] = value; map118_set_ppu(); break;
      case 6: reg118_prg0   = value; map118_set_cpu(); break;
      case 7: reg118_prg1   = value; map118_set_cpu(); break;
      }
      break;
   case 0xC000:
      reg118[4] = value;
      reg118_irq_cnt = value;
      break;
   case 0xC001:
      reg118[5] = value;
      reg118_irq_latch = value;
      break;
   case 0xE000:
      reg118[6] = value;
      reg118_irq_enable = 0;
      break;
   case 0xE001:
      reg118[7] = value;
      reg118_irq_enable = 1;
      break;
   }
}

static void map118_init(void)
{
   int i;
   for (i = 0; i < 8; i++) { reg118[i] = 0; reg118_chr[i] = i; }
   reg118_prg0 = 0; reg118_prg1 = 1;
   reg118_irq_enable = 0; reg118_irq_cnt = 0; reg118_irq_latch = 0;
   map118_set_cpu();
   map118_set_ppu();
}

static const map_memwrite map118_memwrite[] =
{
   { 0x8000, 0xFFFF, map118_write },
   {     -1,     -1, NULL }
};

const mapintf_t map118_intf =
{
   118,            /* mapper number */
   "Others",       /* mapper name */
   map118_init,    /* init */
   NULL,           /* vblank */
   map118_hblank,  /* hblank */
   NULL, NULL, NULL,
   map118_memwrite,
   NULL
};
