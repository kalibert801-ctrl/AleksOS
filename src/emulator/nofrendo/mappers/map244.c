/*
** map244.c
** Mapper 244
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static void map244_write(uint32 address, uint8 value)
{
   int bank;
   if (address >= 0x8065 && address <= 0x80A4) {
      bank = ((address - 0x8065) & 0x3) << 2;
      mmc_bankrom(8, 0x8000, bank+0);
      mmc_bankrom(8, 0xA000, bank+1);
      mmc_bankrom(8, 0xC000, bank+2);
      mmc_bankrom(8, 0xE000, bank+3);
   } else if (address >= 0x80A5 && address <= 0x80E4) {
      bank = ((address - 0x80A5) & 0x7) << 2;
      mmc_bankrom(8, 0x8000, bank+0);
      mmc_bankrom(8, 0xA000, bank+1);
      mmc_bankrom(8, 0xC000, bank+2);
      mmc_bankrom(8, 0xE000, bank+3);
   }
}

static void map244_init(void)
{
   mmc_bankrom(8, 0x8000, 0);
   mmc_bankrom(8, 0xA000, 1);
   mmc_bankrom(8, 0xC000, 2);
   mmc_bankrom(8, 0xE000, 3);
}

static const map_memwrite map244_memwrite[] =
{
   { 0x8000, 0xFFFF, map244_write },
   {     -1,     -1, NULL }
};

const mapintf_t map244_intf =
{
   244,          /* mapper number */
   "Mapper 244", /* mapper name */
   map244_init,  /* init */
   NULL, NULL, NULL, NULL, NULL,
   map244_memwrite,
   NULL
};
