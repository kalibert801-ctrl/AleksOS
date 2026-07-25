/*
** map051.c
** Mapper 51 — 11-in-1 multi-game cart (BMC)
**
** Two registers: bank (from data bits 3-0) and mode (2-bit).
**
** Writes to $6000-$7FFF set both mode bits from the data byte.
** Writes to $8000-$FFFF always set the bank; writes to $C000-$DFFF
**   also update mode bit 1 from data bit 4.
**
** CPU banking (Set_CPU_Banks):
**   Mode 0: vertical mirror, 16KB PRG at $8000 + last 16KB fixed block
**   Mode 1: vertical mirror, 32KB PRG block at $8000 (normal)
**   Mode 2: vertical mirror, different 16KB split
**   Mode 3: horizontal mirror, same 32KB layout as mode 1
**
** CHR: this cart has no CHR ROM — uses CHR RAM (auto-configured).
** $6000-$7FFF also carries a banked ROM page for reads (SRAMBANK).
*/

#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static int bank51;
static int mode51;

/* ------------------------------------------------------------------ */
/* CPU bank helper                                                       */
/* ------------------------------------------------------------------ */
static void map51_set_cpu(void)
{
   switch (mode51)
   {
   case 0:
      ppu_mirror(0, 1, 0, 1); /* vertical */
      mmc_bankrom(8, 0x6000, bank51 | 0x2C | 3);
      mmc_bankrom(8, 0x8000, bank51 | 0x00 | 0);
      mmc_bankrom(8, 0xA000, bank51 | 0x00 | 1);
      mmc_bankrom(8, 0xC000, bank51 | 0x0C | 2);
      mmc_bankrom(8, 0xE000, bank51 | 0x0C | 3);
      break;

   case 1:
      ppu_mirror(0, 1, 0, 1); /* vertical */
      mmc_bankrom(8, 0x6000, bank51 | 0x20 | 3);
      mmc_bankrom(8, 0x8000, bank51 | 0x00 | 0);
      mmc_bankrom(8, 0xA000, bank51 | 0x00 | 1);
      mmc_bankrom(8, 0xC000, bank51 | 0x00 | 2);
      mmc_bankrom(8, 0xE000, bank51 | 0x00 | 3);
      break;

   case 2:
      ppu_mirror(0, 1, 0, 1); /* vertical */
      mmc_bankrom(8, 0x6000, bank51 | 0x2E | 3);
      mmc_bankrom(8, 0x8000, bank51 | 0x02 | 0);
      mmc_bankrom(8, 0xA000, bank51 | 0x02 | 1);
      mmc_bankrom(8, 0xC000, bank51 | 0x0E | 2);
      mmc_bankrom(8, 0xE000, bank51 | 0x0E | 3);
      break;

   case 3:
      ppu_mirror(0, 0, 1, 1); /* horizontal */
      mmc_bankrom(8, 0x6000, bank51 | 0x20 | 3);
      mmc_bankrom(8, 0x8000, bank51 | 0x00 | 0);
      mmc_bankrom(8, 0xA000, bank51 | 0x00 | 1);
      mmc_bankrom(8, 0xC000, bank51 | 0x00 | 2);
      mmc_bankrom(8, 0xE000, bank51 | 0x00 | 3);
      break;
   }
}

/* ------------------------------------------------------------------ */
/* SRAM write ($6000-$7FFF) — sets both mode bits                      */
/* ------------------------------------------------------------------ */
static void map51_sram_write(uint32 address, uint8 value)
{
   (void)address;
   mode51 = ((value & 0x10) >> 3) | ((value & 0x02) >> 1);
   map51_set_cpu();
}

/* ------------------------------------------------------------------ */
/* Main write ($8000-$FFFF)                                             */
/* ------------------------------------------------------------------ */
static void map51_write(uint32 address, uint8 value)
{
   bank51 = (value & 0x0F) << 2;
   if (address >= 0xC000 && address <= 0xDFFF)
   {
      mode51 = (mode51 & 0x01) | ((value & 0x10) >> 3);
   }
   map51_set_cpu();
}

/* ------------------------------------------------------------------ */
/* Init                                                                  */
/* ------------------------------------------------------------------ */
static void map51_init(void)
{
   bank51 = 0;
   mode51 = 1;
   map51_set_cpu();
}

static const map_memwrite map51_memwrite[] =
{
   { 0x6000, 0x7FFF, map51_sram_write },
   { 0x8000, 0xFFFF, map51_write },
   {     -1,     -1, NULL }
};

const mapintf_t map51_intf =
{
   51,            /* mapper number */
   "BMC 11-in-1", /* mapper name */
   map51_init,    /* init */
   NULL,          /* vblank */
   NULL,          /* hblank */
   NULL, NULL, NULL,
   map51_memwrite,
   NULL
};
