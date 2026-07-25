/*
** map057.c
** Mapper 57 — Multi-game pirate board
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint8 reg57;

static void map57_write(uint32 address, uint8 value)
{
   uint8 chr;

   switch (address)
   {
   case 0x8000:
   case 0x8001:
   case 0x8002:
   case 0x8003:
      if (value & 0x40)
      {
         chr = (value & 0x03) + ((reg57 & 0x10) >> 1) + (reg57 & 0x07);
         mmc_bankvrom(1, 0x0000, (chr << 3) + 0);
         mmc_bankvrom(1, 0x0400, (chr << 3) + 1);
         mmc_bankvrom(1, 0x0800, (chr << 3) + 2);
         mmc_bankvrom(1, 0x0C00, (chr << 3) + 3);
         mmc_bankvrom(1, 0x1000, (chr << 3) + 4);
         mmc_bankvrom(1, 0x1400, (chr << 3) + 5);
         mmc_bankvrom(1, 0x1800, (chr << 3) + 6);
         mmc_bankvrom(1, 0x1C00, (chr << 3) + 7);
      }
      break;

   case 0x8800:
      reg57 = value;

      if (value & 0x80)
      {
         mmc_bankrom(8, 0x8000, ((value & 0x40) >> 6) * 4 + 8 + 0);
         mmc_bankrom(8, 0xA000, ((value & 0x40) >> 6) * 4 + 8 + 1);
         mmc_bankrom(8, 0xC000, ((value & 0x40) >> 6) * 4 + 8 + 2);
         mmc_bankrom(8, 0xE000, ((value & 0x40) >> 6) * 4 + 8 + 3);
      }
      else
      {
         mmc_bankrom(8, 0x8000, ((value & 0x60) >> 5) * 2 + 0);
         mmc_bankrom(8, 0xA000, ((value & 0x60) >> 5) * 2 + 1);
         mmc_bankrom(8, 0xC000, ((value & 0x60) >> 5) * 2 + 0);
         mmc_bankrom(8, 0xE000, ((value & 0x60) >> 5) * 2 + 1);
      }

      chr = (value & 0x07) + ((value & 0x10) >> 1);
      mmc_bankvrom(1, 0x0000, (chr << 3) + 0);
      mmc_bankvrom(1, 0x0400, (chr << 3) + 1);
      mmc_bankvrom(1, 0x0800, (chr << 3) + 2);
      mmc_bankvrom(1, 0x0C00, (chr << 3) + 3);
      mmc_bankvrom(1, 0x1000, (chr << 3) + 4);
      mmc_bankvrom(1, 0x1400, (chr << 3) + 5);
      mmc_bankvrom(1, 0x1800, (chr << 3) + 6);
      mmc_bankvrom(1, 0x1C00, (chr << 3) + 7);

      if (value & 0x08) ppu_mirror(0, 0, 1, 1); /* horizontal */
      else              ppu_mirror(0, 1, 0, 1); /* vertical */
      break;
   }
}

static void map57_init(void)
{
   reg57 = 0;
   mmc_bankrom(8, 0x8000, 0);
   mmc_bankrom(8, 0xA000, 1);
   mmc_bankrom(8, 0xC000, 0);
   mmc_bankrom(8, 0xE000, 1);
}

static const map_memwrite map57_memwrite[] =
{
   { 0x8000, 0xFFFF, map57_write },
   {     -1,     -1, NULL }
};

const mapintf_t map57_intf =
{
   57,           /* mapper number */
   "57",         /* mapper name */
   map57_init,   /* init */
   NULL,         /* vblank */
   NULL,         /* hblank */
   NULL, NULL, NULL,
   map57_memwrite,
   NULL
};
