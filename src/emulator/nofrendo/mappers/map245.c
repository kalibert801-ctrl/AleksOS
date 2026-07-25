/*
** map245.c
** Mapper 245 — Yong Zhe Dou E Long (MMC3-like)
**
** Similar to MMC3 but reg[0] case 0x00 controls an outer 64-bank block
** (stored in reg[3]) that is OR'd into all PRG bank selects.
** MMC3-style IRQ on hblank.
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>
#include <nes_rom.h>

static uint8  reg245[8];
static uint8  prg245_0, prg245_1;
static uint8  reg245_outer;   /* outer PRG block (from sub-reg 0x00 write) */
static uint8  irq245_enable, irq245_counter, irq245_latch, irq245_request;

static void map245_hblank(int vblank)
{
   if (!vblank) {
      if (irq245_enable && !irq245_request) {
         if (!(irq245_counter--)) {
            irq245_request = 0xFF;
            irq245_counter = irq245_latch;
         }
      }
   }
   if (irq245_request)
      nes_irq();
}

static void map245_write(uint32 address, uint8 value)
{
   switch (address & 0xE001) {
   case 0x8000:
      reg245[0] = value;
      break;

   case 0x8001:
      reg245[1] = value;
      switch (reg245[0] & 0x07) {
      case 0x00:
         /* bit 1 of value selects upper 64-bank block */
         reg245_outer = (value & 0x02) << 5;   /* 0 or 0x40 */
         mmc_bankrom(8, 0xC000, 0x3E | reg245_outer);
         mmc_bankrom(8, 0xE000, 0x3F | reg245_outer);
         break;
      case 0x06:
         prg245_0 = value;
         break;
      case 0x07:
         prg245_1 = value;
         break;
      }
      mmc_bankrom(8, 0x8000, prg245_0 | reg245_outer);
      mmc_bankrom(8, 0xA000, prg245_1 | reg245_outer);
      break;

   case 0xA000:
      reg245[2] = value;
      if (!(mmc_getinfo()->flags & ROM_FLAG_FOURSCREEN)) {
         if (value & 0x01) ppu_mirror(0, 0, 1, 1); /* horizontal */
         else              ppu_mirror(0, 1, 0, 1); /* vertical   */
      }
      break;

   case 0xA001:
      /* SRAM enable/disable — ignored */
      break;

   case 0xC000:
      reg245[4]      = value;
      irq245_counter = value;
      irq245_request = 0;
      break;

   case 0xC001:
      reg245[5]     = value;
      irq245_latch  = value;
      irq245_request = 0;
      break;

   case 0xE000:
      reg245[6]      = value;
      irq245_enable  = 0;
      irq245_request = 0;
      break;

   case 0xE001:
      reg245[7]      = value;
      irq245_enable  = 1;
      irq245_request = 0;
      break;
   }
}

static void map245_init(void)
{
   int last = mmc_getinfo()->rom_banks * 2 - 1;
   int i;

   for (i = 0; i < 8; i++) reg245[i] = 0;

   prg245_0    = 0;
   prg245_1    = 1;
   reg245_outer = 0;

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

   irq245_enable  = 0;
   irq245_counter = 0;
   irq245_latch   = 0;
   irq245_request = 0;
}

static const map_memwrite map245_memwrite[] =
{
   { 0x8000, 0xFFFF, map245_write },
   {     -1,     -1, NULL }
};

const mapintf_t map245_intf =
{
   245,             /* mapper number */
   "Yong Zhe 245",  /* mapper name */
   map245_init,     /* init */
   NULL,            /* vblank */
   map245_hblank,   /* hblank */
   NULL, NULL, NULL,
   map245_memwrite,
   NULL
};
