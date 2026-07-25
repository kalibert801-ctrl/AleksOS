/*
** map110.c
** Mapper 110
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint8 reg110_cmd;
static uint8 reg110_chr;

static void map110_set_ppu(void)
{
   mmc_bankvrom(1, 0x0000, (reg110_chr << 3) + 0);
   mmc_bankvrom(1, 0x0400, (reg110_chr << 3) + 1);
   mmc_bankvrom(1, 0x0800, (reg110_chr << 3) + 2);
   mmc_bankvrom(1, 0x0C00, (reg110_chr << 3) + 3);
   mmc_bankvrom(1, 0x1000, (reg110_chr << 3) + 4);
   mmc_bankvrom(1, 0x1400, (reg110_chr << 3) + 5);
   mmc_bankvrom(1, 0x1800, (reg110_chr << 3) + 6);
   mmc_bankvrom(1, 0x1C00, (reg110_chr << 3) + 7);
}

static void map110_write(uint32 address, uint8 value)
{
   switch (address)
   {
   case 0x4100:
      reg110_cmd = value & 0x07;
      break;
   case 0x4101:
      switch (reg110_cmd)
      {
      case 5:
         mmc_bankrom(8, 0x8000, (value << 2) + 0);
         mmc_bankrom(8, 0xA000, (value << 2) + 1);
         mmc_bankrom(8, 0xC000, (value << 2) + 2);
         mmc_bankrom(8, 0xE000, (value << 2) + 3);
         break;
      case 0:
         reg110_chr = value & 0x01;
         map110_set_ppu();
         break;
      case 2:
         reg110_chr = value;
         map110_set_ppu();
         break;
      case 4:
         reg110_chr = reg110_chr | (value << 1);
         map110_set_ppu();
         break;
      case 6:
         reg110_chr = reg110_chr | (value << 2);
         map110_set_ppu();
         break;
      default:
         break;
      }
      break;
   }
}

static void map110_init(void)
{
   reg110_cmd = 0;
   reg110_chr = 0;
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

static const map_memwrite map110_memwrite[] =
{
   { 0x4100, 0x4FFF, map110_write },
   {     -1,     -1, NULL }
};

const mapintf_t map110_intf =
{
   110,          /* mapper number */
   "110",        /* mapper name */
   map110_init,  /* init */
   NULL,         /* vblank */
   NULL,         /* hblank */
   NULL, NULL, NULL,
   map110_memwrite,
   NULL
};
