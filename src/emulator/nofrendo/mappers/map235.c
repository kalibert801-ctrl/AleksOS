/*
** map235.c
** Mapper 235 — 150-in-1 multicart
**
** All bank info is encoded in the write address; the data byte is ignored.
** PRG size variants: 64-bank (100-in-1), 128-bank (150-in-1),
**                    192-bank, 256-bank.
** byBus=1 means the selected PRG block is outside the ROM and should
** read open-bus (0xFF). We approximate by mapping to the last ROM page.
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static void map235_write(uint32 address, uint8 value)
{
   int  rom_pages = mmc_getinfo()->rom_banks * 2; /* total 8KB pages */
   int  prg  = (int)(((address & 0x0300) >> 3) | (address & 0x001F));
   int  byBus = 0;
   (void)value;

   if (rom_pages == 64 * 2) {
      /* 100-in-1 */
      switch (address & 0x0300) {
      case 0x0100: case 0x0200: case 0x0300: byBus = 1; break;
      default: break;
      }
   } else if (rom_pages == 128 * 2) {
      /* 150-in-1 */
      switch (address & 0x0300) {
      case 0x0100:                           byBus = 1; break;
      case 0x0200: prg = (prg & 0x1F) | 0x20; break;
      case 0x0300:                           byBus = 1; break;
      default: break;
      }
   } else if (rom_pages == 192 * 2) {
      switch (address & 0x0300) {
      case 0x0100:                           byBus = 1; break;
      case 0x0200: prg = (prg & 0x1F) | 0x20; break;
      case 0x0300: prg = (prg & 0x1F) | 0x40; break;
      default: break;
      }
   }
   /* 256-bank: no adjustment needed */

   if (!byBus) {
      if (address & 0x0800) {
         if (address & 0x1000) {
            mmc_bankrom(8, 0x8000, (prg << 2) + 2);
            mmc_bankrom(8, 0xA000, (prg << 2) + 3);
            mmc_bankrom(8, 0xC000, (prg << 2) + 2);
            mmc_bankrom(8, 0xE000, (prg << 2) + 3);
         } else {
            mmc_bankrom(8, 0x8000, (prg << 2) + 0);
            mmc_bankrom(8, 0xA000, (prg << 2) + 1);
            mmc_bankrom(8, 0xC000, (prg << 2) + 0);
            mmc_bankrom(8, 0xE000, (prg << 2) + 1);
         }
      } else {
         mmc_bankrom(8, 0x8000, (prg << 2) + 0);
         mmc_bankrom(8, 0xA000, (prg << 2) + 1);
         mmc_bankrom(8, 0xC000, (prg << 2) + 2);
         mmc_bankrom(8, 0xE000, (prg << 2) + 3);
      }
   } else {
      /* Open-bus region: approximate with last ROM page */
      int last = rom_pages - 1;
      mmc_bankrom(8, 0x8000, last);
      mmc_bankrom(8, 0xA000, last);
      mmc_bankrom(8, 0xC000, last);
      mmc_bankrom(8, 0xE000, last);
   }

   if (address & 0x0400)      ppu_mirror(1, 1, 1, 1); /* one-screen NT1 */
   else if (address & 0x2000) ppu_mirror(0, 0, 1, 1); /* horizontal     */
   else                       ppu_mirror(0, 1, 0, 1); /* vertical       */
}

static void map235_init(void)
{
   mmc_bankrom(8, 0x8000, 0);
   mmc_bankrom(8, 0xA000, 1);
   mmc_bankrom(8, 0xC000, 2);
   mmc_bankrom(8, 0xE000, 3);
}

static const map_memwrite map235_memwrite[] =
{
   { 0x8000, 0xFFFF, map235_write },
   {     -1,     -1, NULL }
};

const mapintf_t map235_intf =
{
   235,           /* mapper number */
   "150-in-1",    /* mapper name */
   map235_init,   /* init */
   NULL,          /* vblank */
   NULL,          /* hblank */
   NULL, NULL, NULL,
   map235_memwrite,
   NULL
};
