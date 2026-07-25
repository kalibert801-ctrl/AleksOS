/*
** map242.c
** Mapper 242 — Wai Xing Zhan Shi
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static void map242_write(uint32 address, uint8 value)
{
   if (address & 0x01) {
      int bank = (address & 0xF8) >> 1;
      mmc_bankrom(8, 0x8000, bank+0);
      mmc_bankrom(8, 0xA000, bank+1);
      mmc_bankrom(8, 0xC000, bank+2);
      mmc_bankrom(8, 0xE000, bank+3);
   }
}

static void map242_init(void)
{
   mmc_bankrom(8, 0x8000, 0);
   mmc_bankrom(8, 0xA000, 1);
   mmc_bankrom(8, 0xC000, 2);
   mmc_bankrom(8, 0xE000, 3);
}

static const map_memwrite map242_memwrite[] =
{
   { 0x8000, 0xFFFF, map242_write },
   {     -1,     -1, NULL }
};

const mapintf_t map242_intf =
{
   242,                  /* mapper number */
   "Wai Xing Zhan Shi",  /* mapper name */
   map242_init,          /* init */
   NULL, NULL, NULL, NULL, NULL,
   map242_memwrite,
   NULL
};
