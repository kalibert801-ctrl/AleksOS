/*
** map251.c
** Mapper 251 — MMC3 variant with outer-bank masking
**
** reg[0..7]: MMC3 bank registers
** reg[8]:    MMC3 command register
** reg[9]:    SRAM-write-enable flag (set by $A001 bit 7)
** reg[10]:   SRAM write index (0..3, auto-increments)
** breg[0..3]: outer bank data (written 4 bytes via $6000)
**
** Effective CHR[i] = (reg[i] | (breg[1]<<4)) & ((breg[2]<<4)|0x0F)
** Effective PRG    = (reg[6/7] & mask) | breg[1]
**   where mask = (breg[3]&0x3F)^0x3F
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint8 reg251[11];
static uint8 breg251[4];

static void map251_set_banks(void)
{
   int nChr[6];
   int nPrg0, nPrg1, nPrg23;
   int i;
   uint8 chr_mask = (uint8)(((breg251[2] << 4) & 0xF0) | 0x0F);

   for (i = 0; i < 6; i++) {
      nChr[i] = (reg251[i] | (breg251[1] << 4)) & chr_mask;
   }

   if (reg251[8] & 0x80) {
      mmc_bankvrom(1, 0x0000, nChr[2]);
      mmc_bankvrom(1, 0x0400, nChr[3]);
      mmc_bankvrom(1, 0x0800, nChr[4]);
      mmc_bankvrom(1, 0x0C00, nChr[5]);
      mmc_bankvrom(1, 0x1000, nChr[0]);
      mmc_bankvrom(1, 0x1400, nChr[0] + 1);
      mmc_bankvrom(1, 0x1800, nChr[1]);
      mmc_bankvrom(1, 0x1C00, nChr[1] + 1);
   } else {
      mmc_bankvrom(1, 0x0000, nChr[0]);
      mmc_bankvrom(1, 0x0400, nChr[0] + 1);
      mmc_bankvrom(1, 0x0800, nChr[1]);
      mmc_bankvrom(1, 0x0C00, nChr[1] + 1);
      mmc_bankvrom(1, 0x1000, nChr[2]);
      mmc_bankvrom(1, 0x1400, nChr[3]);
      mmc_bankvrom(1, 0x1800, nChr[4]);
      mmc_bankvrom(1, 0x1C00, nChr[5]);
   }

   {
      uint8 prg_mask = (breg251[3] & 0x3F) ^ 0x3F;
      nPrg0  = (reg251[6] & prg_mask) | breg251[1];
      nPrg1  = (reg251[7] & prg_mask) | breg251[1];
      nPrg23 = prg_mask | breg251[1];   /* "fixed" upper bank */
   }

   if (reg251[8] & 0x40) {
      mmc_bankrom(8, 0x8000, nPrg23);
      mmc_bankrom(8, 0xA000, nPrg1);
      mmc_bankrom(8, 0xC000, nPrg0);
      mmc_bankrom(8, 0xE000, nPrg23);
   } else {
      mmc_bankrom(8, 0x8000, nPrg0);
      mmc_bankrom(8, 0xA000, nPrg1);
      mmc_bankrom(8, 0xC000, nPrg23);
      mmc_bankrom(8, 0xE000, nPrg23);
   }
}

static void map251_sram(uint32 address, uint8 value)
{
   /* Only respond to even addresses in $6000-$7FFF when write-enable is set */
   if ((address & 0xE001) == 0x6000) {
      if (reg251[9]) {
         breg251[reg251[10]++] = value;
         if (reg251[10] == 4) {
            reg251[10] = 0;
            map251_set_banks();
         }
      }
   }
}

static void map251_write(uint32 address, uint8 value)
{
   switch (address & 0xE001) {
   case 0x8000:
      reg251[8] = value;
      map251_set_banks();
      break;

   case 0x8001:
      reg251[reg251[8] & 0x07] = value;
      map251_set_banks();
      break;

   case 0xA001:
      if (value & 0x80) {
         reg251[9]  = 1;
         reg251[10] = 0;
      } else {
         reg251[9]  = 0;
      }
      break;
   }
}

static void map251_init(void)
{
   int last = mmc_getinfo()->rom_banks * 2 - 1;
   int i;

   for (i = 0; i < 11; i++) reg251[i]  = 0;
   for (i = 0; i < 4;  i++) breg251[i] = 0;

   mmc_bankrom(8, 0x8000, 0);
   mmc_bankrom(8, 0xA000, 1);
   mmc_bankrom(8, 0xC000, last - 1);
   mmc_bankrom(8, 0xE000, last);

   ppu_mirror(0, 1, 0, 1); /* vertical (default) */
}

static const map_memwrite map251_memwrite[] =
{
   { 0x6000, 0x7FFF, map251_sram  },
   { 0x8000, 0xFFFF, map251_write },
   {     -1,     -1, NULL }
};

const mapintf_t map251_intf =
{
   251,           /* mapper number */
   "MMC3 Outer",  /* mapper name */
   map251_init,   /* init */
   NULL,          /* vblank */
   NULL,          /* hblank */
   NULL, NULL, NULL,
   map251_memwrite,
   NULL
};
