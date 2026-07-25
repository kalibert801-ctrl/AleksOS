/*
** map240.c
** Mapper 240 — Gen Ke Le Zhuan
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static void map240_write(uint32 address, uint8 value)
{
   int prg = (value & 0xF0) >> 2;
   int chr = (value & 0x0F) << 3;
   mmc_bankrom(8, 0x8000, prg+0);
   mmc_bankrom(8, 0xA000, prg+1);
   mmc_bankrom(8, 0xC000, prg+2);
   mmc_bankrom(8, 0xE000, prg+3);
   mmc_bankvrom(1, 0x0000, chr+0);
   mmc_bankvrom(1, 0x0400, chr+1);
   mmc_bankvrom(1, 0x0800, chr+2);
   mmc_bankvrom(1, 0x0C00, chr+3);
   mmc_bankvrom(1, 0x1000, chr+4);
   mmc_bankvrom(1, 0x1400, chr+5);
   mmc_bankvrom(1, 0x1800, chr+6);
   mmc_bankvrom(1, 0x1C00, chr+7);
}

static void map240_init(void)
{
   int last = mmc_getinfo()->rom_banks * 2 - 1;
   mmc_bankrom(8, 0x8000, 0);
   mmc_bankrom(8, 0xA000, 1);
   mmc_bankrom(8, 0xC000, last-1);
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

static const map_memwrite map240_memwrite[] =
{
   { 0x4020, 0x5FFF, map240_write },
   {     -1,     -1, NULL }
};

const mapintf_t map240_intf =
{
   240,                /* mapper number */
   "Gen Ke Le Zhuan",  /* mapper name */
   map240_init,        /* init */
   NULL, NULL, NULL, NULL, NULL,
   map240_memwrite,
   NULL
};
