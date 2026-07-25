/*
** map249.c
** Mapper 249 — MMC3 with encrypted bank index (Spdata)
**
** When Spdata==0: standard MMC3.
** When Spdata==1: CHR and PRG bank indices are bit-scrambled before use.
** APU write to $5000 sets Spdata (0x00→0, 0x02→1).
** Address mirroring: $8000/$8800 share cmd, $8001/$8801 share data, etc.
** (handled by address & 0xE001 which collapses the +$800 mirror)
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>
#include <nes_rom.h>

static uint8  spdata249;
static uint8  reg249[8];
static uint8  irq249_enable, irq249_counter, irq249_latch, irq249_request;

/* Bit-scramble for CHR banks (0..255) when Spdata==1 */
static uint8 map249_scramble_chr(uint8 d)
{
   uint8 m0 = d & 0x01;
   uint8 m1 = (d >> 1) & 0x01;
   uint8 m2 = (d >> 2) & 0x01;
   uint8 m3 = (d >> 3) & 0x01;
   uint8 m4 = (d >> 4) & 0x01;
   uint8 m5 = (d >> 5) & 0x01;
   uint8 m6 = (d >> 6) & 0x01;
   uint8 m7 = (d >> 7) & 0x01;
   return (uint8)((m5 << 7) | (m4 << 6) | (m2 << 5) | (m6 << 4) |
                  (m7 << 3) | (m3 << 2) | (m1 << 1) | m0);
}

/* Bit-scramble for PRG banks when Spdata==1 */
static uint8 map249_scramble_prg(uint8 d)
{
   uint8 m0, m1, m2, m3, m4, m5, m6, m7;
   if (d < 0x20) {
      m0 = d & 0x01;
      m1 = (d >> 1) & 0x01;
      m2 = (d >> 2) & 0x01;
      m3 = (d >> 3) & 0x01;
      m4 = (d >> 4) & 0x01;
      return (uint8)((m2 << 4) | (m1 << 3) | (m3 << 2) | (m4 << 1) | m0);
   } else {
      d -= 0x20;
      m0 = d & 0x01;
      m1 = (d >> 1) & 0x01;
      m2 = (d >> 2) & 0x01;
      m3 = (d >> 3) & 0x01;
      m4 = (d >> 4) & 0x01;
      m5 = (d >> 5) & 0x01;
      m6 = (d >> 6) & 0x01;
      m7 = (d >> 7) & 0x01;
      return (uint8)((m5 << 7) | (m4 << 6) | (m2 << 5) | (m6 << 4) |
                     (m7 << 3) | (m3 << 2) | (m1 << 1) | m0);
   }
}

static void map249_hblank(int vblank)
{
   if (!vblank) {
      if (irq249_enable && !irq249_request) {
         if (!(irq249_counter--)) {
            irq249_request = 0xFF;
            irq249_counter = irq249_latch;
         }
      }
   }
   if (irq249_request)
      nes_irq();
}

static void map249_apu(uint32 address, uint8 value)
{
   if (address == 0x5000) {
      if      (value == 0x00) spdata249 = 0;
      else if (value == 0x02) spdata249 = 1;
   }
}

static void map249_write(uint32 address, uint8 value)
{
   switch (address & 0xE001) {
   case 0x8000:
      reg249[0] = value;
      break;

   case 0x8001:
      switch (reg249[0] & 0x07) {
      case 0x00: {
         uint8 v = spdata249 ? map249_scramble_chr(value) : value;
         mmc_bankvrom(1, 0x0000, v & 0xFE);
         mmc_bankvrom(1, 0x0400, v | 0x01);
         break;
      }
      case 0x01: {
         uint8 v = spdata249 ? map249_scramble_chr(value) : value;
         mmc_bankvrom(1, 0x0800, v & 0xFE);
         mmc_bankvrom(1, 0x0C00, v | 0x01);
         break;
      }
      case 0x02:
         mmc_bankvrom(1, 0x1000, spdata249 ? map249_scramble_chr(value) : value);
         break;
      case 0x03:
         mmc_bankvrom(1, 0x1400, spdata249 ? map249_scramble_chr(value) : value);
         break;
      case 0x04:
         mmc_bankvrom(1, 0x1800, spdata249 ? map249_scramble_chr(value) : value);
         break;
      case 0x05:
         mmc_bankvrom(1, 0x1C00, spdata249 ? map249_scramble_chr(value) : value);
         break;
      case 0x06:
         mmc_bankrom(8, 0x8000, spdata249 ? map249_scramble_prg(value) : value);
         break;
      case 0x07:
         mmc_bankrom(8, 0xA000, spdata249 ? map249_scramble_prg(value) : value);
         break;
      }
      break;

   case 0xA000:
      reg249[2] = value;
      if (!(mmc_getinfo()->flags & ROM_FLAG_FOURSCREEN)) {
         if (value & 0x01) ppu_mirror(0, 0, 1, 1); /* horizontal */
         else              ppu_mirror(0, 1, 0, 1); /* vertical   */
      }
      break;

   case 0xA001:
      reg249[3] = value;
      break;

   case 0xC000:
      reg249[4]      = value;
      irq249_counter = value;
      irq249_request = 0;
      break;

   case 0xC001:
      reg249[5]      = value;
      irq249_latch   = value;
      irq249_request = 0;
      break;

   case 0xE000:
      reg249[6]      = value;
      irq249_enable  = 0;
      irq249_request = 0;
      break;

   case 0xE001:
      reg249[7]      = value;
      irq249_enable  = 1;
      irq249_request = 0;
      break;
   }
}

static void map249_init(void)
{
   int last = mmc_getinfo()->rom_banks * 2 - 1;
   int i;

   spdata249 = 0;
   for (i = 0; i < 8; i++) reg249[i] = 0;

   mmc_bankrom(8, 0x8000, 0);
   mmc_bankrom(8, 0xA000, 1);
   mmc_bankrom(8, 0xC000, last - 1);
   mmc_bankrom(8, 0xE000, last);

   if (mmc_getinfo()->vrom_banks > 0) {
      mmc_bankvrom(1, 0x0000, 0); mmc_bankvrom(1, 0x0400, 1);
      mmc_bankvrom(1, 0x0800, 2); mmc_bankvrom(1, 0x0C00, 3);
      mmc_bankvrom(1, 0x1000, 4); mmc_bankvrom(1, 0x1400, 5);
      mmc_bankvrom(1, 0x1800, 6); mmc_bankvrom(1, 0x1C00, 7);
   }

   irq249_enable  = 0;
   irq249_counter = 0;
   irq249_latch   = 0;
   irq249_request = 0;
}

static const map_memwrite map249_memwrite[] =
{
   { 0x5000, 0x5FFF, map249_apu  },
   { 0x8000, 0xFFFF, map249_write },
   {     -1,     -1, NULL }
};

const mapintf_t map249_intf =
{
   249,              /* mapper number */
   "MMC3 Encrypted", /* mapper name */
   map249_init,      /* init */
   NULL,             /* vblank */
   map249_hblank,    /* hblank */
   NULL, NULL, NULL,
   map249_memwrite,
   NULL
};
