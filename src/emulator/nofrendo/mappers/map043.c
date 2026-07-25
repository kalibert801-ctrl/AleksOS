/*
** map043.c
** Mapper 43 — SMB2J (bootleg)
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint32 reg43_irq_cnt;
static uint8  reg43_irq_enable;

static void map43_hblank(int vblank)
{
   UNUSED(vblank);
   if (reg43_irq_enable)
   {
      reg43_irq_cnt += 114;
      if (reg43_irq_cnt >= 4096)
      {
         reg43_irq_cnt -= 4096;
         nes_irq();
      }
   }
}

static void map43_apu_write(uint32 address, uint8 value)
{
   if ((address & 0xF0FF) == 0x4022)
   {
      switch (value & 0x07)
      {
      case 0x00:
      case 0x02:
      case 0x03:
      case 0x04: mmc_bankrom(8, 0xC000, 4); break;
      case 0x01: mmc_bankrom(8, 0xC000, 3); break;
      case 0x05: mmc_bankrom(8, 0xC000, 7); break;
      case 0x06: mmc_bankrom(8, 0xC000, 5); break;
      case 0x07: mmc_bankrom(8, 0xC000, 6); break;
      }
   }
}

static void map43_write(uint32 address, uint8 value)
{
   if (address == 0x8122)
   {
      if (value & 0x03)
         reg43_irq_enable = 1;
      else
      {
         reg43_irq_cnt    = 0;
         reg43_irq_enable = 0;
      }
   }
}

static void map43_init(void)
{
   reg43_irq_enable = 1;
   reg43_irq_cnt    = 0;
   mmc_bankrom(8, 0x8000, 1);
   mmc_bankrom(8, 0xA000, 0);
   mmc_bankrom(8, 0xC000, 4);
   mmc_bankrom(8, 0xE000, 9);
   mmc_bankvrom(1, 0x0000, 0);
   mmc_bankvrom(1, 0x0400, 1);
   mmc_bankvrom(1, 0x0800, 2);
   mmc_bankvrom(1, 0x0C00, 3);
   mmc_bankvrom(1, 0x1000, 4);
   mmc_bankvrom(1, 0x1400, 5);
   mmc_bankvrom(1, 0x1800, 6);
   mmc_bankvrom(1, 0x1C00, 7);
}

static const map_memwrite map43_memwrite[] =
{
   { 0x4100, 0x5FFF, map43_apu_write },
   { 0x8000, 0xFFFF, map43_write },
   {     -1,     -1, NULL }
};

const mapintf_t map43_intf =
{
   43,            /* mapper number */
   "SMB2J",       /* mapper name */
   map43_init,    /* init */
   NULL,          /* vblank */
   map43_hblank,  /* hblank */
   NULL, NULL, NULL,
   map43_memwrite,
   NULL
};
