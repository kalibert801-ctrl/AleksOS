/*
** map188.c
** Mapper 188 — Bandai Karaoke Studio
**
** $8000-$FFFF write selects switchable 16KB PRG bank ($8000-$BFFF).
** $C000-$FFFF is fixed at the last 16KB of ROM.
** Bit 4 of the write value inverts the ROM region used.
*/

#include <noftypes.h>
#include <nes_mmc.h>

static void map188_write(uint32 address, uint8 value)
{
   (void)address;
   if (value)
   {
      if (value & 0x10)
      {
         /* bit 4 set: select from lower ROM region, bits 2-0 as 16KB bank */
         value = (value & 0x07) << 1;
         mmc_bankrom(8, 0x8000, value + 0);
         mmc_bankrom(8, 0xA000, value + 1);
      }
      else
      {
         /* bit 4 clear: select from upper ROM region (offset by 16 pages) */
         value <<= 1;
         mmc_bankrom(8, 0x8000, value + 16);
         mmc_bankrom(8, 0xA000, value + 17);
      }
   }
   else
   {
      /* value 0: reset to default upper-region bank */
      int nb = mmc_getinfo()->rom_banks * 2;
      if (nb == 0x10)
      {
         mmc_bankrom(8, 0x8000, 14);
         mmc_bankrom(8, 0xA000, 15);
      }
      else
      {
         mmc_bankrom(8, 0x8000, 16);
         mmc_bankrom(8, 0xA000, 17);
      }
   }
}

static void map188_init(void)
{
   int nb = mmc_getinfo()->rom_banks * 2;
   mmc_bankrom(8, 0x8000, 0);
   mmc_bankrom(8, 0xA000, 1);
   if (nb > 16)
   {
      mmc_bankrom(8, 0xC000, 14);
      mmc_bankrom(8, 0xE000, 15);
   }
   else
   {
      /* ROMLASTPAGE(1) / ROMLASTPAGE(0) */
      mmc_bankrom(8, 0xC000, nb - 2);
      mmc_bankrom(8, 0xE000, nb - 1);
   }
}

static const map_memwrite map188_memwrite[] =
{
   { 0x8000, 0xFFFF, map188_write },
   {     -1,     -1, NULL }
};

const mapintf_t map188_intf =
{
   188,
   "Bandai Karaoke Studio",
   map188_init,
   NULL, NULL, NULL, NULL, NULL,
   map188_memwrite,
   NULL
};
