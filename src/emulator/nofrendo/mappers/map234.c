/*
** map234.c
** Mapper 234 — Maxi-15
**
** Bank switching is triggered by CPU writes (intercepted by the mapper)
** to addresses $FF80-$FF9F (set Reg[0] once, locked) and
** $FFE8-$FFF7 (set Reg[1] freely).
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint8 reg234[2];

static void map234_set_banks(void)
{
   uint8 prg, chr;

   if (reg234[0] & 0x80) ppu_mirror(0, 0, 1, 1); /* horizontal */
   else                   ppu_mirror(0, 1, 0, 1); /* vertical   */

   if (reg234[0] & 0x40) {
      prg = (reg234[0] & 0x0E) | (reg234[1] & 0x01);
      chr = (uint8)(((reg234[0] & 0x0E) << 2) | ((reg234[1] >> 4) & 0x07));
   } else {
      prg = reg234[0] & 0x0F;
      chr = (uint8)(((reg234[0] & 0x0F) << 2) | ((reg234[1] >> 4) & 0x03));
   }

   mmc_bankrom(8, 0x8000, (prg << 2) + 0);
   mmc_bankrom(8, 0xA000, (prg << 2) + 1);
   mmc_bankrom(8, 0xC000, (prg << 2) + 2);
   mmc_bankrom(8, 0xE000, (prg << 2) + 3);

   mmc_bankvrom(1, 0x0000, (chr << 3) + 0);
   mmc_bankvrom(1, 0x0400, (chr << 3) + 1);
   mmc_bankvrom(1, 0x0800, (chr << 3) + 2);
   mmc_bankvrom(1, 0x0C00, (chr << 3) + 3);
   mmc_bankvrom(1, 0x1000, (chr << 3) + 4);
   mmc_bankvrom(1, 0x1400, (chr << 3) + 5);
   mmc_bankvrom(1, 0x1800, (chr << 3) + 6);
   mmc_bankvrom(1, 0x1C00, (chr << 3) + 7);
}

static void map234_write(uint32 address, uint8 value)
{
   if (address >= 0xFF80 && address <= 0xFF9F) {
      if (!reg234[0]) {
         reg234[0] = value;
         map234_set_banks();
      }
   }
   if (address >= 0xFFE8 && address <= 0xFFF7) {
      reg234[1] = value;
      map234_set_banks();
   }
}

static void map234_init(void)
{
   reg234[0] = 0;
   reg234[1] = 0;
   mmc_bankrom(8, 0x8000, 0);
   mmc_bankrom(8, 0xA000, 1);
   mmc_bankrom(8, 0xC000, 2);
   mmc_bankrom(8, 0xE000, 3);
}

static const map_memwrite map234_memwrite[] =
{
   { 0x8000, 0xFFFF, map234_write },
   {     -1,     -1, NULL }
};

const mapintf_t map234_intf =
{
   234,          /* mapper number */
   "Maxi-15",    /* mapper name */
   map234_init,  /* init */
   NULL,         /* vblank */
   NULL,         /* hblank */
   NULL, NULL, NULL,
   map234_memwrite,
   NULL
};
