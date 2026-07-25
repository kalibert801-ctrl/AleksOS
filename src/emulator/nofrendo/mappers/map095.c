/*
** map095.c
** Mapper 95 — Namco 1xx (MMC3-like, no IRQ)
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint8 reg95_cmd;
static uint32 reg95_prg0, reg95_prg1;
static uint32 reg95_chr01, reg95_chr23;
static uint32 reg95_chr4, reg95_chr5, reg95_chr6, reg95_chr7;

static void map95_set_cpu(void)
{
   int last = mmc_getinfo()->rom_banks * 2 - 1;
   if (reg95_cmd & 0x40)
   {
      mmc_bankrom(8, 0x8000, last - 1);
      mmc_bankrom(8, 0xA000, reg95_prg1);
      mmc_bankrom(8, 0xC000, reg95_prg0);
      mmc_bankrom(8, 0xE000, last);
   }
   else
   {
      mmc_bankrom(8, 0x8000, reg95_prg0);
      mmc_bankrom(8, 0xA000, reg95_prg1);
      mmc_bankrom(8, 0xC000, last - 1);
      mmc_bankrom(8, 0xE000, last);
   }
}

static void map95_set_ppu(void)
{
   if (reg95_cmd & 0x80)
   {
      mmc_bankvrom(1, 0x0000, reg95_chr4);
      mmc_bankvrom(1, 0x0400, reg95_chr5);
      mmc_bankvrom(1, 0x0800, reg95_chr6);
      mmc_bankvrom(1, 0x0C00, reg95_chr7);
      mmc_bankvrom(1, 0x1000, reg95_chr01 + 0);
      mmc_bankvrom(1, 0x1400, reg95_chr01 + 1);
      mmc_bankvrom(1, 0x1800, reg95_chr23 + 0);
      mmc_bankvrom(1, 0x1C00, reg95_chr23 + 1);
   }
   else
   {
      mmc_bankvrom(1, 0x0000, reg95_chr01 + 0);
      mmc_bankvrom(1, 0x0400, reg95_chr01 + 1);
      mmc_bankvrom(1, 0x0800, reg95_chr23 + 0);
      mmc_bankvrom(1, 0x0C00, reg95_chr23 + 1);
      mmc_bankvrom(1, 0x1000, reg95_chr4);
      mmc_bankvrom(1, 0x1400, reg95_chr5);
      mmc_bankvrom(1, 0x1800, reg95_chr6);
      mmc_bankvrom(1, 0x1C00, reg95_chr7);
   }
}

static void map95_write(uint32 address, uint8 value)
{
   uint32 bank;

   switch (address & 0xE001)
   {
   case 0x8000:
      reg95_cmd = value;
      map95_set_ppu();
      map95_set_cpu();
      break;
   case 0x8001:
      if ((reg95_cmd & 0x07) <= 5)
      {
         if (value & 0x20)
            ppu_mirror(0, 0, 0, 0); /* NT0 */
         else
            ppu_mirror(1, 1, 1, 1); /* NT1 */
         value &= 0x1F;
      }
      bank = value;
      switch (reg95_cmd & 0x07)
      {
      case 0: reg95_chr01 = bank & 0xFE; map95_set_ppu(); break;
      case 1: reg95_chr23 = bank & 0xFE; map95_set_ppu(); break;
      case 2: reg95_chr4  = bank;        map95_set_ppu(); break;
      case 3: reg95_chr5  = bank;        map95_set_ppu(); break;
      case 4: reg95_chr6  = bank;        map95_set_ppu(); break;
      case 5: reg95_chr7  = bank;        map95_set_ppu(); break;
      case 6: reg95_prg0  = bank;        map95_set_cpu(); break;
      case 7: reg95_prg1  = bank;        map95_set_cpu(); break;
      }
      break;
   }
}

static void map95_init(void)
{
   reg95_cmd  = 0;
   reg95_prg0 = 0; reg95_prg1 = 1;
   reg95_chr01 = 0; reg95_chr23 = 2;
   reg95_chr4 = 4; reg95_chr5 = 5; reg95_chr6 = 6; reg95_chr7 = 7;
   map95_set_cpu();
   map95_set_ppu();
}

static const map_memwrite map95_memwrite[] =
{
   { 0x8000, 0xFFFF, map95_write },
   {     -1,     -1, NULL }
};

const mapintf_t map95_intf =
{
   95,           /* mapper number */
   "Namco 1xx",  /* mapper name */
   map95_init,   /* init */
   NULL,         /* vblank */
   NULL,         /* hblank */
   NULL, NULL, NULL,
   map95_memwrite,
   NULL
};
