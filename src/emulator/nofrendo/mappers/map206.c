/*
** map206.c
** Mapper 206 (DxROM / Namcot 118)
**
** Simplified MMC3 without IRQ:
**   $8000 (even): bank select command (bits 2-0 = register 0-7,
**                 bit 7 = CHR A12 inversion, bit 6 = PRG A13 inversion)
**   $8001 (odd):  bank data for selected register
**
**   Cmd 0: 2KB CHR page (2 × 1KB) at base+$0000 / base+$0400
**   Cmd 1: 2KB CHR page (2 × 1KB) at base+$0800 / base+$0C00
**   Cmd 2: 1KB CHR at alt+$0000
**   Cmd 3: 1KB CHR at alt+$0400
**   Cmd 4: 1KB CHR at alt+$0800
**   Cmd 5: 1KB CHR at alt+$0C00
**   Cmd 6: 8KB PRG at $8000 (or $C000 if PRG inversion)
**   Cmd 7: 8KB PRG at $A000
**   $C000-$DFFF: second-to-last 8KB bank (fixed)
**   $E000-$FFFF: last 8KB bank (fixed)
*/

#include <string.h>
#include <noftypes.h>
#include <nes_mmc.h>

static uint8 bank_sel;
static uint8 banks[8];

static void map206_apply(void)
{
   int vbase = (bank_sel & 0x80) ? 0x1000 : 0x0000;
   int valt  = vbase ^ 0x1000;

   /* CHR banks */
   mmc_bankvrom(1, vbase + 0x0000, banks[0] & ~1);
   mmc_bankvrom(1, vbase + 0x0400, banks[0] |  1);
   mmc_bankvrom(1, vbase + 0x0800, banks[1] & ~1);
   mmc_bankvrom(1, vbase + 0x0C00, banks[1] |  1);
   mmc_bankvrom(1, valt  + 0x0000, banks[2]);
   mmc_bankvrom(1, valt  + 0x0400, banks[3]);
   mmc_bankvrom(1, valt  + 0x0800, banks[4]);
   mmc_bankvrom(1, valt  + 0x0C00, banks[5]);

   /* PRG banks */
   int paddr0 = (bank_sel & 0x40) ? 0xC000 : 0x8000;
   int paddr2 = paddr0 ^ 0x4000;
   mmc_bankrom(8, paddr0, banks[6] & 0x3F);
   mmc_bankrom(8, 0xA000, banks[7] & 0x3F);
   mmc_bankrom(8, paddr2, mmc_getinfo()->rom_banks * 2 - 2);
   mmc_bankrom(8, 0xE000, mmc_getinfo()->rom_banks * 2 - 1);
}

static void map206_write(uint32 address, uint8 value)
{
   switch (address & 0xE001)
   {
   case 0x8000:
      bank_sel = value;
      map206_apply();
      break;
   case 0x8001:
      banks[bank_sel & 7] = value;
      map206_apply();
      break;
   }
}

static void map206_init(void)
{
   bank_sel = 0;
   memset(banks, 0, sizeof(banks));
   map206_apply();
}

static const map_memwrite map206_memwrite[] =
{
   { 0x8000, 0xFFFF, map206_write },
   {     -1,     -1, NULL }
};

const mapintf_t map206_intf =
{
   206,          /* mapper number */
   "DxROM",      /* mapper name */
   map206_init,
   NULL, NULL, NULL, NULL, NULL,
   map206_memwrite,
   NULL
};
