/*
** map117.c
** Mapper 117 — PC-Future (Future Media)
**
** Converted from InfoNES_Mapper_117.cpp
** IRQ fires when the internal scanline counter matches the programmed line.
** Two enable flags: irq_enable1 (set by $C001-$C003, cleared on fire) and
** irq_enable2 (bit 0 of $E000 write).
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint8 irq_line;
static uint8 irq_enable1;
static uint8 irq_enable2;
static int   scan;   /* visible-scanline counter (0-239), reset on vblank */

static void map117_hblank(int vblank)
{
   if (vblank)
   {
      scan = 0;
      return;
   }
   if (irq_enable1 && irq_enable2)
   {
      if (scan == (int)irq_line)
      {
         irq_enable1 = 0;
         nes_irq();
      }
   }
   scan++;
}

static void map117_write(uint32 address, uint8 value)
{
   int N = mmc_getinfo()->rom_banks * 2;
   switch (address)
   {
   /* PRG bank selects — ROMBANK0-2 only; ROMBANK3 fixed to last page */
   case 0x8000:
      mmc_bankrom(8, 0x8000, value);
      break;
   case 0x8001:
      mmc_bankrom(8, 0xA000, value);
      break;
   case 0x8002:
      mmc_bankrom(8, 0xC000, value);
      break;

   /* CHR bank selects */
   case 0xA000: mmc_bankvrom(1, 0x0000, value); break;
   case 0xA001: mmc_bankvrom(1, 0x0400, value); break;
   case 0xA002: mmc_bankvrom(1, 0x0800, value); break;
   case 0xA003: mmc_bankvrom(1, 0x0C00, value); break;
   case 0xA004: mmc_bankvrom(1, 0x1000, value); break;
   case 0xA005: mmc_bankvrom(1, 0x1400, value); break;
   case 0xA006: mmc_bankvrom(1, 0x1800, value); break;
   case 0xA007: mmc_bankvrom(1, 0x1C00, value); break;

   /* IRQ control */
   case 0xC001:
   case 0xC002:
   case 0xC003:
      irq_enable1 = value;
      irq_line    = value;
      break;
   case 0xE000:
      irq_enable2 = value & 0x01;
      break;

   default:
      (void)N;
      break;
   }
}

static void map117_init(void)
{
   int N = mmc_getinfo()->rom_banks * 2;
   int i;

   mmc_bankrom(8, 0x8000, 0);
   mmc_bankrom(8, 0xA000, 1);
   mmc_bankrom(8, 0xC000, N - 2);
   mmc_bankrom(8, 0xE000, N - 1);

   for (i = 0; i < 8; i++)
      mmc_bankvrom(1, i * 0x400, i);

   irq_line    = 0;
   irq_enable1 = 0;
   irq_enable2 = 1;   /* enabled by default per InfoNES source */
   scan        = 0;
}

static const map_memwrite map117_memwrite[] =
{
   { 0x8000, 0xFFFF, map117_write },
   {     -1,     -1, NULL }
};

const mapintf_t map117_intf =
{
   117,          /* mapper number */
   "PC-Future",  /* mapper name */
   map117_init,  /* init */
   NULL,         /* vblank */
   map117_hblank,/* hblank */
   NULL, NULL, NULL,
   map117_memwrite,
   NULL
};
