/*
** map049.c
** Mapper 49 — Nin1 (MMC3-based with outer bank block select via SRAM write)
**
** $6000-$7FFF write: sets outer bank select register when bit 7 of the
**   SRAM-control register ($A001) is set.
** $8000-$FFFF: MMC3-compatible PRG/CHR/IRQ registers.
**
** PRG banking:
**   When reg[1] bit 0 is clear: NROM mode — 32KB block selected by
**     bits 4-6 of reg[1], shifted to give a base offset of 0/4/8/…/28.
**   When reg[1] bit 0 is set: MMC3 mode — bits 6-7 select a 16-bank (128KB)
**     outer block; PRG0/PRG1 select 8KB banks within that block.
**
** CHR banking: same as MMC3, with bits 6-7 of reg[1] adding a 128-bank
**   (128KB) outer offset.
*/

#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint8  reg49[3];   /* [0]=command, [1]=block/bank-select, [2]=sram-ctrl */
static uint32 prg49_0, prg49_1;
static uint32 chr49_01, chr49_23;
static uint32 chr49_4, chr49_5, chr49_6, chr49_7;
static uint8  irq49_enable;
static uint8  irq49_cnt;
static uint8  irq49_latch;

/* ------------------------------------------------------------------ */
/* CPU bank helper                                                       */
/* ------------------------------------------------------------------ */
static void map49_set_cpu(void)
{
   uint32 block = (reg49[1] & 0xC0) >> 2;   /* outer block offset: 0,16,32,48 */
   uint32 last  = (uint32)(mmc_getinfo()->rom_banks * 2 - 1);

   if (reg49[1] & 0x01)
   {
      /* MMC3 game-select mode */
      if (reg49[0] & 0x40)
      {
         /* PRG swap: fixed bank at $8000/$E000, variable at $A000/$C000 */
         mmc_bankrom(8, 0x8000, (last & 0x0E) | block);
         mmc_bankrom(8, 0xA000, (prg49_1 & 0x0F) | block);
         mmc_bankrom(8, 0xC000, (prg49_0 & 0x0F) | block);
         mmc_bankrom(8, 0xE000, (last & 0x0F) | block);
      }
      else
      {
         /* Normal: variable at $8000/$A000, fixed at $C000/$E000 */
         mmc_bankrom(8, 0x8000, (prg49_0 & 0x0F) | block);
         mmc_bankrom(8, 0xA000, (prg49_1 & 0x0F) | block);
         mmc_bankrom(8, 0xC000, (last & 0x0E) | block);
         mmc_bankrom(8, 0xE000, (last & 0x0F) | block);
      }
   }
   else
   {
      /* NROM mode: 32KB block selected by bits 4-6 of reg[1] */
      uint32 b = (reg49[1] & 0x70) >> 2;
      mmc_bankrom(8, 0x8000, b | 0);
      mmc_bankrom(8, 0xA000, b | 1);
      mmc_bankrom(8, 0xC000, b | 2);
      mmc_bankrom(8, 0xE000, b | 3);
   }
}

/* ------------------------------------------------------------------ */
/* PPU bank helper                                                       */
/* ------------------------------------------------------------------ */
static void map49_set_ppu(void)
{
   uint32 cblk = (reg49[1] & 0xC0) << 1;   /* outer CHR block: 0,128,256,384 */
   uint32 c01  = chr49_01 | cblk;
   uint32 c23  = chr49_23 | cblk;
   uint32 c4   = chr49_4  | cblk;
   uint32 c5   = chr49_5  | cblk;
   uint32 c6   = chr49_6  | cblk;
   uint32 c7   = chr49_7  | cblk;

   if (reg49[0] & 0x80)
   {
      /* CHR swap: 2KB banks at $0000, 1KB banks at $1000 */
      mmc_bankvrom(1, 0x0000, c4);
      mmc_bankvrom(1, 0x0400, c5);
      mmc_bankvrom(1, 0x0800, c6);
      mmc_bankvrom(1, 0x0C00, c7);
      mmc_bankvrom(1, 0x1000, c01 + 0);
      mmc_bankvrom(1, 0x1400, c01 + 1);
      mmc_bankvrom(1, 0x1800, c23 + 0);
      mmc_bankvrom(1, 0x1C00, c23 + 1);
   }
   else
   {
      /* Normal: 2KB banks at $1000, 1KB banks at $0000 */
      mmc_bankvrom(1, 0x0000, c01 + 0);
      mmc_bankvrom(1, 0x0400, c01 + 1);
      mmc_bankvrom(1, 0x0800, c23 + 0);
      mmc_bankvrom(1, 0x0C00, c23 + 1);
      mmc_bankvrom(1, 0x1000, c4);
      mmc_bankvrom(1, 0x1400, c5);
      mmc_bankvrom(1, 0x1800, c6);
      mmc_bankvrom(1, 0x1C00, c7);
   }
}

/* ------------------------------------------------------------------ */
/* HBlank callback — MMC3-style IRQ counter                            */
/* ------------------------------------------------------------------ */
static void map49_hblank(int vblank)
{
   if (!vblank && irq49_enable)
   {
      if (!(irq49_cnt--))
      {
         irq49_cnt = irq49_latch;
         nes_irq();
      }
   }
}

/* ------------------------------------------------------------------ */
/* SRAM write ($6000-$7FFF) — outer bank-block select                  */
/* ------------------------------------------------------------------ */
static void map49_sram_write(uint32 address, uint8 value)
{
   (void)address;
   if (reg49[2] & 0x80)
   {
      reg49[1] = value;
      map49_set_cpu();
      map49_set_ppu();
   }
}

/* ------------------------------------------------------------------ */
/* Main write ($8000-$FFFF)                                             */
/* ------------------------------------------------------------------ */
static void map49_write(uint32 address, uint8 value)
{
   switch (address & 0xE001)
   {
   case 0x8000:
      {
         uint8 old = reg49[0];
         reg49[0] = value;
         if ((value ^ old) & 0x40)   /* PRG swap bit changed */
            map49_set_cpu();
         if ((value ^ old) & 0x80)   /* CHR swap bit changed */
            map49_set_ppu();
      }
      break;

   case 0x8001:
      switch (reg49[0] & 0x07)
      {
      case 0: chr49_01 = value & 0xFE; map49_set_ppu(); break;
      case 1: chr49_23 = value & 0xFE; map49_set_ppu(); break;
      case 2: chr49_4  = value;         map49_set_ppu(); break;
      case 3: chr49_5  = value;         map49_set_ppu(); break;
      case 4: chr49_6  = value;         map49_set_ppu(); break;
      case 5: chr49_7  = value;         map49_set_ppu(); break;
      case 6: prg49_0  = value;         map49_set_cpu(); break;
      case 7: prg49_1  = value;         map49_set_cpu(); break;
      }
      break;

   case 0xA000:
      if (0 == (mmc_getinfo()->flags & ROM_FLAG_FOURSCREEN))
      {
         if (value & 0x01) ppu_mirror(0, 0, 1, 1); /* horizontal */
         else              ppu_mirror(0, 1, 0, 1); /* vertical */
      }
      break;

   case 0xA001:
      reg49[2] = value;   /* SRAM enable / control */
      break;

   case 0xC000:
      irq49_cnt = value;
      break;

   case 0xC001:
      irq49_latch = value;
      break;

   case 0xE000:
      irq49_enable = 0;
      break;

   case 0xE001:
      irq49_enable = 1;
      break;
   }
}

/* ------------------------------------------------------------------ */
/* Init                                                                  */
/* ------------------------------------------------------------------ */
static void map49_init(void)
{
   reg49[0] = reg49[1] = reg49[2] = 0;
   prg49_0 = 0; prg49_1 = 1;
   chr49_01 = 0; chr49_23 = 2;
   chr49_4 = 4; chr49_5 = 5; chr49_6 = 6; chr49_7 = 7;
   irq49_enable = 0; irq49_cnt = 0; irq49_latch = 0;

   mmc_bankrom(8, 0x8000, 0);
   mmc_bankrom(8, 0xA000, 1);
   mmc_bankrom(8, 0xC000, 2);
   mmc_bankrom(8, 0xE000, 3);

   mmc_bankvrom(1, 0x0000, 0);
   mmc_bankvrom(1, 0x0400, 1);
   mmc_bankvrom(1, 0x0800, 2);
   mmc_bankvrom(1, 0x0C00, 3);
   mmc_bankvrom(1, 0x1000, 4);
   mmc_bankvrom(1, 0x1400, 5);
   mmc_bankvrom(1, 0x1800, 6);
   mmc_bankvrom(1, 0x1C00, 7);
}

static const map_memwrite map49_memwrite[] =
{
   { 0x6000, 0x7FFF, map49_sram_write },
   { 0x8000, 0xFFFF, map49_write },
   {     -1,     -1, NULL }
};

const mapintf_t map49_intf =
{
   49,            /* mapper number */
   "Nin1",        /* mapper name */
   map49_init,    /* init */
   NULL,          /* vblank */
   map49_hblank,  /* hblank */
   NULL, NULL, NULL,
   map49_memwrite,
   NULL
};
