/*
** map232.c
** Mapper 232 — Quattro Games
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint8 reg232[2];

static void map232_update(void)
{
   uint8 bank = reg232[0] | reg232[1];
   mmc_bankrom(8, 0x8000, (bank << 1) + 0);
   mmc_bankrom(8, 0xA000, (bank << 1) + 1);
   mmc_bankrom(8, 0xC000, ((reg232[0] | 0x03) << 1) + 0);
   mmc_bankrom(8, 0xE000, ((reg232[0] | 0x03) << 1) + 1);
}

static void map232_write(uint32 address, uint8 value)
{
   if (address == 0x9000)
   {
      reg232[0] = (value & 0x18) >> 1;
   }
   else if (address >= 0xA000)
   {
      reg232[1] = value & 0x03;
   }
   map232_update();
}

static void map232_init(void)
{
   int last = mmc_getinfo()->rom_banks * 2 - 1;
   reg232[0] = 0x0C;
   reg232[1] = 0x00;
   mmc_bankrom(8, 0x8000, 0);
   mmc_bankrom(8, 0xA000, 1);
   mmc_bankrom(8, 0xC000, last - 1);
   mmc_bankrom(8, 0xE000, last);
}

static const map_memwrite map232_memwrite[] =
{
   { 0x8000, 0xFFFF, map232_write },
   {     -1,     -1, NULL }
};

const mapintf_t map232_intf =
{
   232,             /* mapper number */
   "Quattro Games", /* mapper name */
   map232_init,     /* init */
   NULL,            /* vblank */
   NULL,            /* hblank */
   NULL, NULL, NULL,
   map232_memwrite,
   NULL
};
