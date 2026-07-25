/*
** map090.c
** Mapper 90 — JY Company
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint8  prg_reg[4];
static uint8  chr_low[8];
static uint8  chr_high[8];
static uint8  nam_low[4];
static uint8  nam_high[4];

static uint8  prg_size;    /* bits 1-0 of $D000 */
static uint8  prg_6000;    /* bit 7 of $D000: map ROM at $6000 */
static uint8  prg_e000;    /* bit 2 of $D000: 4x8KB fully switchable */
static uint8  chr_size;    /* bits 4-3 of $D000: 0=8KB,1=4KB,2=2KB,3=1KB */
static uint8  mirror_mode; /* bit 5 of $D000: 1=CHR-NT (unsupported, fallback) */
static uint8  mirror_type; /* bits 1-0 of $D001 */

static uint32 mul_val1;
static uint32 mul_val2;

static uint8  irq_enable;
static uint8  irq_cnt;
static uint8  irq_latch;

static void sync_prg(void)
{
   int last = mmc_getinfo()->rom_banks * 2 - 1;
   switch (prg_size) {
   case 0: /* fixed last 32KB */
      mmc_bankrom(8, 0x8000, last-3);
      mmc_bankrom(8, 0xA000, last-2);
      mmc_bankrom(8, 0xC000, last-1);
      mmc_bankrom(8, 0xE000, last);
      break;
   case 1: /* 16KB switchable, $C000/$E000 fixed */
      mmc_bankrom(8, 0x8000, (prg_reg[1]<<1)+0);
      mmc_bankrom(8, 0xA000, (prg_reg[1]<<1)+1);
      mmc_bankrom(8, 0xC000, last-1);
      mmc_bankrom(8, 0xE000, last);
      break;
   case 2: /* 4x8KB */
      if (prg_e000) {
         mmc_bankrom(8, 0x8000, prg_reg[0]);
         mmc_bankrom(8, 0xA000, prg_reg[1]);
         mmc_bankrom(8, 0xC000, prg_reg[2]);
         mmc_bankrom(8, 0xE000, prg_reg[3]);
      } else {
         if (prg_6000) mmc_bankrom(8, 0x6000, prg_reg[3]);
         mmc_bankrom(8, 0x8000, prg_reg[0]);
         mmc_bankrom(8, 0xA000, prg_reg[1]);
         mmc_bankrom(8, 0xC000, prg_reg[2]);
         mmc_bankrom(8, 0xE000, last);
      }
      break;
   default: /* 4x8KB reversed */
      mmc_bankrom(8, 0x8000, prg_reg[3]);
      mmc_bankrom(8, 0xA000, prg_reg[2]);
      mmc_bankrom(8, 0xC000, prg_reg[1]);
      mmc_bankrom(8, 0xE000, prg_reg[0]);
      break;
   }
}

static void sync_chr(void)
{
   uint32 bank[8];
   int i;
   for (i = 0; i < 8; i++)
      bank[i] = ((uint32)chr_high[i] << 8) | chr_low[i];

   switch (chr_size) {
   case 0: /* 8KB: one register covers all */
      for (i = 0; i < 8; i++)
         mmc_bankvrom(1, i*0x400, (bank[0]<<3)+i);
      break;
   case 1: /* 4KB: bank[0] and bank[4] */
      for (i = 0; i < 4; i++) mmc_bankvrom(1,  i     *0x400, (bank[0]<<2)+i);
      for (i = 0; i < 4; i++) mmc_bankvrom(1, (i+4)  *0x400, (bank[4]<<2)+i);
      break;
   case 2: /* 2KB: bank[0,2,4,6] */
      for (i = 0; i < 2; i++) mmc_bankvrom(1,  i     *0x400, (bank[0]<<1)+i);
      for (i = 0; i < 2; i++) mmc_bankvrom(1, (i+2)  *0x400, (bank[2]<<1)+i);
      for (i = 0; i < 2; i++) mmc_bankvrom(1, (i+4)  *0x400, (bank[4]<<1)+i);
      for (i = 0; i < 2; i++) mmc_bankvrom(1, (i+6)  *0x400, (bank[6]<<1)+i);
      break;
   default: /* 1KB: each slot independent */
      for (i = 0; i < 8; i++)
         mmc_bankvrom(1, i*0x400, bank[i]);
      break;
   }
}

static void sync_mirror(void)
{
   /* CHR-based NT mirroring (mirror_mode=1) not supported in Nofrendo */
   switch (mirror_type) {
   case 0x00: ppu_mirror(0,1,0,1); break; /* vertical */
   case 0x01: ppu_mirror(0,0,1,1); break; /* horizontal */
   default:   ppu_mirror(1,1,1,1); break; /* one-screen NT1 */
   }
}

static void map90_apu_write(uint32 address, uint8 value)
{
   if (address == 0x5000)      mul_val1 = value;
   else if (address == 0x5001) mul_val2 = value;
}

static uint8 map90_apu_read(uint32 address)
{
   if (address == 0x5000) return (uint8)((mul_val1 * mul_val2) & 0xFF);
   return (uint8)(address >> 8);
}

static void map90_write(uint32 address, uint8 value)
{
   switch (address) {
   case 0x8000: case 0x8001: case 0x8002: case 0x8003:
      prg_reg[address & 0x03] = value;
      sync_prg();
      break;

   case 0x9000: case 0x9001: case 0x9002: case 0x9003:
   case 0x9004: case 0x9005: case 0x9006: case 0x9007:
      chr_low[address & 0x07] = value;
      sync_chr();
      break;

   case 0xA000: case 0xA001: case 0xA002: case 0xA003:
   case 0xA004: case 0xA005: case 0xA006: case 0xA007:
      chr_high[address & 0x07] = value;
      sync_chr();
      break;

   case 0xB000: case 0xB001: case 0xB002: case 0xB003:
      nam_low[address & 0x03] = value;
      sync_mirror();
      break;

   case 0xB004: case 0xB005: case 0xB006: case 0xB007:
      nam_high[address & 0x03] = value;
      sync_mirror();
      break;

   case 0xC002:
      irq_enable = 0;
      break;

   case 0xC003:
   case 0xC004:
      if (!irq_enable) {
         irq_enable = 1;
         irq_cnt = irq_latch;
      }
      break;

   case 0xC005:
      irq_cnt   = value;
      irq_latch = value;
      break;

   case 0xD000:
      prg_6000    = value & 0x80;
      prg_e000    = value & 0x04;
      prg_size    = value & 0x03;
      chr_size    = (value & 0x18) >> 3;
      mirror_mode = value & 0x20;
      sync_prg();
      sync_chr();
      sync_mirror();
      break;

   case 0xD001:
      mirror_type = value & 0x03;
      sync_mirror();
      break;
   }
}

static void map90_hblank(int vblank)
{
   if (!vblank && irq_enable) {
      if (irq_cnt == 0) {
         nes_irq();
         irq_latch  = 0;
         irq_enable = 0;
      } else {
         irq_cnt--;
      }
   }
}

static void map90_init(void)
{
   int last = mmc_getinfo()->rom_banks * 2 - 1;
   int i;

   mmc_bankrom(8, 0x8000, last-3);
   mmc_bankrom(8, 0xA000, last-2);
   mmc_bankrom(8, 0xC000, last-1);
   mmc_bankrom(8, 0xE000, last);

   mmc_bankvrom(1, 0x0000, 0); mmc_bankvrom(1, 0x0400, 1);
   mmc_bankvrom(1, 0x0800, 2); mmc_bankvrom(1, 0x0C00, 3);
   mmc_bankvrom(1, 0x1000, 4); mmc_bankvrom(1, 0x1400, 5);
   mmc_bankvrom(1, 0x1800, 6); mmc_bankvrom(1, 0x1C00, 7);

   irq_enable  = 0;
   irq_cnt     = 0;
   irq_latch   = 0;
   mul_val1    = 0;
   mul_val2    = 0;
   prg_size    = 0;
   prg_6000    = 0;
   prg_e000    = 0;
   chr_size    = 3;
   mirror_mode = 0;
   mirror_type = 0;

   for (i = 0; i < 4; i++) {
      prg_reg[i]    = last - 3 + i;
      nam_low[i]    = 0;
      nam_high[i]   = 0;
      chr_low[i]    = i;
      chr_high[i]   = 0;
      chr_low[i+4]  = i + 4;
      chr_high[i+4] = 0;
   }
}

static const map_memwrite map90_memwrite[] =
{
   { 0x5000, 0x5FFF, map90_apu_write },
   { 0x8000, 0xFFFF, map90_write },
   {     -1,     -1, NULL }
};

static const map_memread map90_memread[] =
{
   { 0x5000, 0x5FFF, map90_apu_read },
   {     -1,     -1, NULL }
};

const mapintf_t map90_intf =
{
   90,           /* mapper number */
   "JY Company", /* mapper name */
   map90_init,   /* init */
   NULL,         /* vblank */
   map90_hblank, /* hblank */
   NULL, NULL,
   map90_memread,
   map90_memwrite,
   NULL
};
