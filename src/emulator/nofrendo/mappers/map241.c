/*
** map241.c
** Mapper 241 — Fon Serm Bon
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static void map241_write(uint32 address, uint8 value)
{
   if (address == 0x8000) {
      mmc_bankrom(8, 0x8000, (value<<2)+0);
      mmc_bankrom(8, 0xA000, (value<<2)+1);
      mmc_bankrom(8, 0xC000, (value<<2)+2);
      mmc_bankrom(8, 0xE000, (value<<2)+3);
   }
}

static void map241_init(void)
{
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

static const map_memwrite map241_memwrite[] =
{
   { 0x8000, 0xFFFF, map241_write },
   {     -1,     -1, NULL }
};

const mapintf_t map241_intf =
{
   241,             /* mapper number */
   "Fon Serm Bon",  /* mapper name */
   map241_init,     /* init */
   NULL, NULL, NULL, NULL, NULL,
   map241_memwrite,
   NULL
};
