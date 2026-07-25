/*
** map248.c
** Mapper 248 — Bao Qing Tian (MMC3-like with SRAM/APU bank-override)
**
** Standard MMC3 layout with:
**   - SRAM write ($6000-$7FFF): switches entire 16KB PRG window
**   - APU write  ($4100-$4FFF): same as SRAM write
**   - ROM write  ($8000-$FFFF): MMC3-style CHR/PRG select with
**     fixed IRQ reload values (0xBE) on $C000/$C001.
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>
#include <nes_rom.h>

static uint8  reg248[8];
static uint8  prg248_0, prg248_1;
static uint8  chr248_01, chr248_23;
static uint8  chr248_4, chr248_5, chr248_6, chr248_7;
static uint8  irq248_enable, irq248_counter, irq248_latch, irq248_request;

static void map248_set_cpu(void)
{
   int last = mmc_getinfo()->rom_banks * 2 - 1;
   if (reg248[0] & 0x40) {
      mmc_bankrom(8, 0x8000, last - 1);
      mmc_bankrom(8, 0xA000, prg248_1);
      mmc_bankrom(8, 0xC000, prg248_0);
      mmc_bankrom(8, 0xE000, last);
   } else {
      mmc_bankrom(8, 0x8000, prg248_0);
      mmc_bankrom(8, 0xA000, prg248_1);
      mmc_bankrom(8, 0xC000, last - 1);
      mmc_bankrom(8, 0xE000, last);
   }
}

static void map248_set_ppu(void)
{
   if (reg248[0] & 0x80) {
      mmc_bankvrom(1, 0x0000, chr248_4);
      mmc_bankvrom(1, 0x0400, chr248_5);
      mmc_bankvrom(1, 0x0800, chr248_6);
      mmc_bankvrom(1, 0x0C00, chr248_7);
      mmc_bankvrom(1, 0x1000, chr248_01 + 0);
      mmc_bankvrom(1, 0x1400, chr248_01 + 1);
      mmc_bankvrom(1, 0x1800, chr248_23 + 0);
      mmc_bankvrom(1, 0x1C00, chr248_23 + 1);
   } else {
      mmc_bankvrom(1, 0x0000, chr248_01 + 0);
      mmc_bankvrom(1, 0x0400, chr248_01 + 1);
      mmc_bankvrom(1, 0x0800, chr248_23 + 0);
      mmc_bankvrom(1, 0x0C00, chr248_23 + 1);
      mmc_bankvrom(1, 0x1000, chr248_4);
      mmc_bankvrom(1, 0x1400, chr248_5);
      mmc_bankvrom(1, 0x1800, chr248_6);
      mmc_bankvrom(1, 0x1C00, chr248_7);
   }
}

static void map248_hblank(int vblank)
{
   if (!vblank && irq248_enable) {
      if (!(irq248_counter--)) {
         irq248_counter = irq248_latch;
         nes_irq();
      }
   }
}

/* SRAM and APU both perform the same 16KB switch */
static void map248_sram(uint32 address, uint8 value)
{
   int last = mmc_getinfo()->rom_banks * 2 - 1;
   (void)address;
   mmc_bankrom(8, 0x8000, (value << 1) + 0);
   mmc_bankrom(8, 0xA000, (value << 1) + 1);
   mmc_bankrom(8, 0xC000, last - 1);
   mmc_bankrom(8, 0xE000, last);
}

static void map248_write(uint32 address, uint8 value)
{
   switch (address & 0xE001) {
   case 0x8000:
      reg248[0] = value;
      map248_set_cpu();
      map248_set_ppu();
      break;

   case 0x8001:
      reg248[1] = value;
      switch (reg248[0] & 0x07) {
      case 0x00: chr248_01 = value & 0xFE; map248_set_ppu(); break;
      case 0x01: chr248_23 = value & 0xFE; map248_set_ppu(); break;
      case 0x02: chr248_4  = value;        map248_set_ppu(); break;
      case 0x03: chr248_5  = value;        map248_set_ppu(); break;
      case 0x04: chr248_6  = value;        map248_set_ppu(); break;
      case 0x05: chr248_7  = value;        map248_set_ppu(); break;
      case 0x06: prg248_0  = value;        map248_set_cpu(); break;
      case 0x07: prg248_1  = value;        map248_set_cpu(); break;
      }
      break;

   case 0xA000:
      reg248[2] = value;
      if (!(mmc_getinfo()->flags & ROM_FLAG_FOURSCREEN)) {
         if (value & 0x01) ppu_mirror(0, 0, 1, 1); /* horizontal */
         else              ppu_mirror(0, 1, 0, 1); /* vertical   */
      }
      break;

   case 0xC000:
      irq248_enable  = 0;
      irq248_latch   = 0xBE;
      irq248_counter = 0xBE;
      break;

   case 0xC001:
      irq248_enable  = 1;
      irq248_latch   = 0xBE;
      irq248_counter = 0xBE;
      break;
   }
}

static void map248_init(void)
{
   int i;
   for (i = 0; i < 8; i++) reg248[i] = 0;

   prg248_0  = 0;  prg248_1  = 1;
   chr248_01 = 0;  chr248_23 = 2;
   chr248_4  = 4;  chr248_5  = 5;  chr248_6 = 6;  chr248_7 = 7;

   map248_set_cpu();
   map248_set_ppu();

   irq248_enable  = 0;
   irq248_counter = 0;
   irq248_latch   = 0;
   irq248_request = 0;
}

static const map_memwrite map248_memwrite[] =
{
   { 0x4100, 0x4FFF, map248_sram  },
   { 0x6000, 0x7FFF, map248_sram  },
   { 0x8000, 0xFFFF, map248_write },
   {     -1,     -1, NULL }
};

const mapintf_t map248_intf =
{
   248,              /* mapper number */
   "Bao Qing Tian",  /* mapper name */
   map248_init,      /* init */
   NULL,             /* vblank */
   map248_hblank,    /* hblank */
   NULL, NULL, NULL,
   map248_memwrite,
   NULL
};
