/*
** map048.c
** Mapper 48 — Taito TC0190V
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint8 reg48_mirror_lock; /* set by $E000 write */
static uint8 reg48_irq_enable;
static uint8 reg48_irq_cnt;

static void map48_hblank(int vblank)
{
   if (!vblank && reg48_irq_enable)
   {
      if (reg48_irq_cnt == 0xFF)
      {
         nes_irq();
         reg48_irq_enable = 0;
      }
      else
      {
         reg48_irq_cnt++;
      }
   }
}

static void map48_write(uint32 address, uint8 value)
{
   switch (address)
   {
   case 0x8000:
      if (!reg48_mirror_lock)
      {
         if (value & 0x40) ppu_mirror(0, 0, 1, 1); /* horizontal */
         else              ppu_mirror(0, 1, 0, 1); /* vertical */
      }
      mmc_bankrom(8, 0x8000, value);
      break;
   case 0x8001:
      mmc_bankrom(8, 0xA000, value);
      break;
   case 0x8002:
      mmc_bankvrom(1, 0x0000, (value << 1) + 0);
      mmc_bankvrom(1, 0x0400, (value << 1) + 1);
      break;
   case 0x8003:
      mmc_bankvrom(1, 0x0800, (value << 1) + 0);
      mmc_bankvrom(1, 0x0C00, (value << 1) + 1);
      break;
   case 0xA000:
      mmc_bankvrom(1, 0x1000, value);
      break;
   case 0xA001:
      mmc_bankvrom(1, 0x1400, value);
      break;
   case 0xA002:
      mmc_bankvrom(1, 0x1800, value);
      break;
   case 0xA003:
      mmc_bankvrom(1, 0x1C00, value);
      break;
   case 0xC000:
      reg48_irq_cnt = value;
      break;
   case 0xC001:
      reg48_irq_enable = value & 0x01;
      break;
   case 0xE000:
      if (value & 0x40) ppu_mirror(0, 0, 1, 1); /* horizontal */
      else              ppu_mirror(0, 1, 0, 1); /* vertical */
      reg48_mirror_lock = 1;
      break;
   }
}

static void map48_init(void)
{
   int last = mmc_getinfo()->rom_banks * 2 - 1;
   reg48_mirror_lock = 0;
   reg48_irq_enable  = 0;
   reg48_irq_cnt     = 0;
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

static const map_memwrite map48_memwrite[] =
{
   { 0x8000, 0xFFFF, map48_write },
   {     -1,     -1, NULL }
};

const mapintf_t map48_intf =
{
   48,                   /* mapper number */
   "Taito TC0190V",      /* mapper name */
   map48_init,           /* init */
   NULL,                 /* vblank */
   map48_hblank,         /* hblank */
   NULL, NULL, NULL,
   map48_memwrite,
   NULL
};
