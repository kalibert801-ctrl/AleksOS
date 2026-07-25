/*
** map026.c
** Mapper 26 — Konami VRC 6V
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint8 reg26_irq_enable;
static uint8 reg26_irq_cnt;
static uint8 reg26_irq_latch;

static void map26_hblank(int vblank)
{
   UNUSED(vblank);
   if (reg26_irq_enable & 0x03)
   {
      if (reg26_irq_cnt >= 0xFE)
      {
         nes_irq();
         reg26_irq_cnt    = reg26_irq_latch;
         reg26_irq_enable = 0;
      }
      else
      {
         reg26_irq_cnt++;
      }
   }
}

static void map26_write(uint32 address, uint8 value)
{
   uint8 n;

   switch (address)
   {
   case 0x8000:
      n = (value << 1);
      mmc_bankrom(8, 0x8000, n + 0);
      mmc_bankrom(8, 0xA000, n + 1);
      break;
   case 0xB003:
      switch (value & 0x7F)
      {
      case 0x08:
      case 0x2C: ppu_mirror(0, 0, 0, 0); break; /* NT0 */
      case 0x20: ppu_mirror(0, 1, 0, 1); break; /* vertical */
      case 0x24: ppu_mirror(0, 0, 1, 1); break; /* horizontal */
      case 0x28: ppu_mirror(1, 1, 1, 1); break; /* NT1 */
      default: break;
      }
      break;
   case 0xC000:
      mmc_bankrom(8, 0xC000, value);
      break;
   case 0xD000: mmc_bankvrom(1, 0x0000, value); break;
   case 0xD002: mmc_bankvrom(1, 0x0400, value); break;
   case 0xD001: mmc_bankvrom(1, 0x0800, value); break;
   case 0xD003: mmc_bankvrom(1, 0x0C00, value); break;
   case 0xE000: mmc_bankvrom(1, 0x1000, value); break;
   case 0xE002: mmc_bankvrom(1, 0x1400, value); break;
   case 0xE001: mmc_bankvrom(1, 0x1800, value); break;
   case 0xE003: mmc_bankvrom(1, 0x1C00, value); break;
   case 0xF000:
      reg26_irq_latch = value;
      break;
   case 0xF001:
      reg26_irq_enable = value & 0x01;
      break;
   case 0xF002:
      reg26_irq_enable = value & 0x03;
      if (reg26_irq_enable & 0x02)
         reg26_irq_cnt = reg26_irq_latch;
      break;
   }
}

static void map26_init(void)
{
   int last = mmc_getinfo()->rom_banks * 2 - 1;
   reg26_irq_enable = 0;
   reg26_irq_cnt    = 0;
   reg26_irq_latch  = 0;
   mmc_bankrom(8, 0x8000, 0);
   mmc_bankrom(8, 0xA000, 1);
   mmc_bankrom(8, 0xC000, last - 1);
   mmc_bankrom(8, 0xE000, last);
   mmc_bankvrom(1, 0x0000, 0);
   mmc_bankvrom(1, 0x0400, 1);
   mmc_bankvrom(1, 0x0800, 2);
   mmc_bankvrom(1, 0x0C00, 3);
   mmc_bankvrom(1, 0x1000, 4);
   mmc_bankvrom(1, 0x1400, 5);
   mmc_bankvrom(1, 0x1800, 6);
   mmc_bankvrom(1, 0x1C00, 7);
}

static const map_memwrite map26_memwrite[] =
{
   { 0x8000, 0xFFFF, map26_write },
   {     -1,     -1, NULL }
};

const mapintf_t map26_intf =
{
   26,               /* mapper number */
   "Konami VRC 6V",  /* mapper name */
   map26_init,       /* init */
   NULL,             /* vblank */
   map26_hblank,     /* hblank */
   NULL, NULL, NULL,
   map26_memwrite,
   NULL
};
