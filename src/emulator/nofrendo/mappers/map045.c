/*
** map045.c
** Mapper 45 — MMC3 Pirates variant (complex CHR/PRG masking)
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

/* reg45[0..3] = outer bank data (cycled via $6000)
** reg45[4]    = unused
** reg45[5]    = write index (0..3, cycles on each $6000 write)
** reg45[6]    = MMC3 command register                          */
static uint8  reg45[7];

/* raw PRG bank numbers (before outer-bank masking) */
static uint32 prg45_0, prg45_1, prg45_2, prg45_3;
/* effective PRG bank numbers (after masking, stored for swap logic) */
static uint32 prg45_P[4];

/* raw CHR bank numbers */
static uint32 chr45_0, chr45_1, chr45_2, chr45_3;
static uint32 chr45_4, chr45_5, chr45_6, chr45_7;

/* IRQ */
static uint8  irq45_enable, irq45_cnt, irq45_latch;

static void map45_set_cpu_bank4(uint8 data)
{
   data &= (reg45[3] & 0x3F) ^ 0xFF;
   data &= 0x3F;
   data |= reg45[1];
   mmc_bankrom(8, 0x8000, data);
   prg45_P[0] = data;
}

static void map45_set_cpu_bank5(uint8 data)
{
   data &= (reg45[3] & 0x3F) ^ 0xFF;
   data &= 0x3F;
   data |= reg45[1];
   mmc_bankrom(8, 0xA000, data);
   prg45_P[1] = data;
}

static void map45_set_cpu_bank6(uint8 data)
{
   data &= (reg45[3] & 0x3F) ^ 0xFF;
   data &= 0x3F;
   data |= reg45[1];
   mmc_bankrom(8, 0xC000, data);
   prg45_P[2] = data;
}

static void map45_set_cpu_bank7(uint8 data)
{
   data &= (reg45[3] & 0x3F) ^ 0xFF;
   data &= 0x3F;
   data |= reg45[1];
   mmc_bankrom(8, 0xE000, data);
   prg45_P[3] = data;
}

static void map45_set_ppu(void)
{
   static const uint8 mask_table[16] = {
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F, 0xFF
   };
   uint32 raw[8];
   uint32 c[8];
   uint32 outer = reg45[0];
   uint8  mask  = mask_table[reg45[2] & 0x0F];
   uint32 hi    = (reg45[2] & 0x10) ? 0x100u : 0u;
   int i;

   raw[0] = chr45_0; raw[1] = chr45_1; raw[2] = chr45_2; raw[3] = chr45_3;
   raw[4] = chr45_4; raw[5] = chr45_5; raw[6] = chr45_6; raw[7] = chr45_7;

   for (i = 0; i < 8; i++) {
      c[i] = (raw[i] & mask) | outer;
      c[i] += hi;
   }

   if (reg45[6] & 0x80) {
      mmc_bankvrom(1, 0x0000, c[4]);
      mmc_bankvrom(1, 0x0400, c[5]);
      mmc_bankvrom(1, 0x0800, c[6]);
      mmc_bankvrom(1, 0x0C00, c[7]);
      mmc_bankvrom(1, 0x1000, c[0]);
      mmc_bankvrom(1, 0x1400, c[1]);
      mmc_bankvrom(1, 0x1800, c[2]);
      mmc_bankvrom(1, 0x1C00, c[3]);
   } else {
      mmc_bankvrom(1, 0x0000, c[0]);
      mmc_bankvrom(1, 0x0400, c[1]);
      mmc_bankvrom(1, 0x0800, c[2]);
      mmc_bankvrom(1, 0x0C00, c[3]);
      mmc_bankvrom(1, 0x1000, c[4]);
      mmc_bankvrom(1, 0x1400, c[5]);
      mmc_bankvrom(1, 0x1800, c[6]);
      mmc_bankvrom(1, 0x1C00, c[7]);
   }
}

static void map45_hblank(int vblank)
{
   if (!vblank && irq45_enable) {
      if (!(irq45_cnt--)) {
         irq45_cnt = irq45_latch;
         nes_irq();
      }
   }
}

static void map45_sram(uint32 address, uint8 value)
{
   if (address == 0x6000) {
      reg45[reg45[5]] = value;
      reg45[5] = (reg45[5] + 1) & 0x03;
      map45_set_cpu_bank4((uint8)prg45_0);
      map45_set_cpu_bank5((uint8)prg45_1);
      map45_set_cpu_bank6((uint8)prg45_2);
      map45_set_cpu_bank7((uint8)prg45_3);
      map45_set_ppu();
   }
}

static void map45_write(uint32 address, uint8 value)
{
   uint32 tmp;

   switch (address & 0xE001) {
   case 0x8000:
      if ((value & 0x40) != (reg45[6] & 0x40)) {
         tmp = prg45_0; prg45_0 = prg45_2; prg45_2 = tmp;
         tmp = prg45_P[0]; prg45_P[0] = prg45_P[2]; prg45_P[2] = tmp;
         mmc_bankrom(8, 0x8000, prg45_P[0]);
         mmc_bankrom(8, 0xC000, prg45_P[2]);
      }
      if ((value & 0x80) != (reg45[6] & 0x80)) {
         tmp = chr45_0; chr45_0 = chr45_4; chr45_4 = tmp;
         tmp = chr45_1; chr45_1 = chr45_5; chr45_5 = tmp;
         tmp = chr45_2; chr45_2 = chr45_6; chr45_6 = tmp;
         tmp = chr45_3; chr45_3 = chr45_7; chr45_7 = tmp;
         map45_set_ppu();
      }
      reg45[6] = value;
      break;

   case 0x8001:
      switch (reg45[6] & 0x07) {
      case 0x00:
         chr45_0 = (value & 0xFE) + 0;
         chr45_1 = (value & 0xFE) + 1;
         map45_set_ppu();
         break;
      case 0x01:
         chr45_2 = (value & 0xFE) + 0;
         chr45_3 = (value & 0xFE) + 1;
         map45_set_ppu();
         break;
      case 0x02:
         chr45_4 = value;
         map45_set_ppu();
         break;
      case 0x03:
         chr45_5 = value;
         map45_set_ppu();
         break;
      case 0x04:
         chr45_6 = value;
         map45_set_ppu();
         break;
      case 0x05:
         chr45_7 = value;
         map45_set_ppu();
         break;
      case 0x06:
         if (reg45[6] & 0x40) {
            prg45_2 = value & 0x3F;
            map45_set_cpu_bank6(value);
         } else {
            prg45_0 = value & 0x3F;
            map45_set_cpu_bank4(value);
         }
         break;
      case 0x07:
         prg45_1 = value & 0x3F;
         map45_set_cpu_bank5(value);
         break;
      }
      break;

   case 0xA000:
      if (value & 0x01) ppu_mirror(0, 0, 1, 1); /* horizontal */
      else              ppu_mirror(0, 1, 0, 1);  /* vertical   */
      break;

   case 0xC000:
      irq45_cnt = value;
      break;

   case 0xC001:
      irq45_latch = value;
      break;

   case 0xE000:
      irq45_enable = 0;
      break;

   case 0xE001:
      irq45_enable = 1;
      break;
   }
}

static void map45_init(void)
{
   int last = mmc_getinfo()->rom_banks * 2 - 1;
   int i;

   for (i = 0; i < 7; i++) reg45[i] = 0;

   prg45_0 = 0;
   prg45_1 = 1;
   prg45_2 = (uint32)(last - 1);
   prg45_3 = (uint32)last;

   mmc_bankrom(8, 0x8000, prg45_0);   prg45_P[0] = prg45_0;
   mmc_bankrom(8, 0xA000, prg45_1);   prg45_P[1] = prg45_1;
   mmc_bankrom(8, 0xC000, prg45_2);   prg45_P[2] = prg45_2;
   mmc_bankrom(8, 0xE000, prg45_3);   prg45_P[3] = prg45_3;

   chr45_0 = 0; chr45_1 = 1; chr45_2 = 2; chr45_3 = 3;
   chr45_4 = 4; chr45_5 = 5; chr45_6 = 6; chr45_7 = 7;
   mmc_bankvrom(1, 0x0000, 0);
   mmc_bankvrom(1, 0x0400, 1);
   mmc_bankvrom(1, 0x0800, 2);
   mmc_bankvrom(1, 0x0C00, 3);
   mmc_bankvrom(1, 0x1000, 4);
   mmc_bankvrom(1, 0x1400, 5);
   mmc_bankvrom(1, 0x1800, 6);
   mmc_bankvrom(1, 0x1C00, 7);

   irq45_enable = 0;
   irq45_cnt    = 0;
   irq45_latch  = 0;
}

static const map_memwrite map45_memwrite[] =
{
   { 0x6000, 0x7FFF, map45_sram  },
   { 0x8000, 0xFFFF, map45_write },
   {     -1,     -1, NULL }
};

const mapintf_t map45_intf =
{
   45,           /* mapper number */
   "Pirates 45", /* mapper name */
   map45_init,   /* init */
   NULL,         /* vblank */
   map45_hblank, /* hblank */
   NULL, NULL, NULL,
   map45_memwrite,
   NULL
};
