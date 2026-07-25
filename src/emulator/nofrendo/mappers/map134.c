/*
** map134.c
** Mapper 134
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint8 reg134_cmd, reg134_prg, reg134_chr;

static void map134_set_banks(void)
{
   mmc_bankrom(8, 0x8000, (reg134_prg << 2) + 0);
   mmc_bankrom(8, 0xA000, (reg134_prg << 2) + 1);
   mmc_bankrom(8, 0xC000, (reg134_prg << 2) + 2);
   mmc_bankrom(8, 0xE000, (reg134_prg << 2) + 3);

   mmc_bankvrom(1, 0x0000, (reg134_chr << 3) + 0);
   mmc_bankvrom(1, 0x0400, (reg134_chr << 3) + 1);
   mmc_bankvrom(1, 0x0800, (reg134_chr << 3) + 2);
   mmc_bankvrom(1, 0x0C00, (reg134_chr << 3) + 3);
   mmc_bankvrom(1, 0x1000, (reg134_chr << 3) + 4);
   mmc_bankvrom(1, 0x1400, (reg134_chr << 3) + 5);
   mmc_bankvrom(1, 0x1800, (reg134_chr << 3) + 6);
   mmc_bankvrom(1, 0x1C00, (reg134_chr << 3) + 7);
}

static void map134_write(uint32 address, uint8 value)
{
   switch (address & 0x4101)
   {
   case 0x4100:
      reg134_cmd = value & 0x07;
      break;
   case 0x4101:
      switch (reg134_cmd)
      {
      case 0:
         reg134_prg = 0;
         reg134_chr = 3;
         break;
      case 4:
         reg134_chr = (reg134_chr & 0x03) | ((value & 0x07) << 2);
         break;
      case 5:
         reg134_prg = value & 0x07;
         break;
      case 6:
         reg134_chr = (reg134_chr & 0x1C) | (value & 0x03);
         break;
      case 7:
         if (value & 0x01) ppu_mirror(0, 0, 1, 1); /* horizontal */
         else              ppu_mirror(0, 1, 0, 1); /* vertical */
         break;
      default:
         break;
      }
      map134_set_banks();
      break;
   }
}

static void map134_init(void)
{
   reg134_cmd = 0;
   reg134_prg = 0;
   reg134_chr = 0;
   map134_set_banks();
}

static const map_memwrite map134_memwrite[] =
{
   { 0x4100, 0x5FFF, map134_write },
   {     -1,     -1, NULL }
};

const mapintf_t map134_intf =
{
   134,          /* mapper number */
   "134",        /* mapper name */
   map134_init,  /* init */
   NULL,         /* vblank */
   NULL,         /* hblank */
   NULL, NULL, NULL,
   map134_memwrite,
   NULL
};
