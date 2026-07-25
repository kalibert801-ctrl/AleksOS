/*
** map083.c
** Mapper 83 — Pirates / Cony
**
** PRG banking at $8000/$B000/$B0FF/$B1FF (32KB block + 16KB sub-bank).
** CHR banking at $8310-$8318: 1KB or 2KB pages depending on mode bits.
** IRQ: 16-bit counter decremented by 114 each HBlank; fires when counter
**   falls to 114 or below, then disables itself.
**
** Expansion register at $5100-$51FF:
**   Read: returns reg83[2].
**   Write ($5101-$5103): sets reg83[2].
*/

#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint8 reg83[3];       /* [0]=PRG/CHR block, [1]=mode, [2]=expansion */
static uint32 chr83_bank;    /* CHR bank offset (bits 8-9 from reg[0]) */
static int    irq83_cnt;     /* 16-bit countdown timer (signed for underflow) */
static uint8  irq83_enabled;

/* ------------------------------------------------------------------ */
/* Expansion read ($5100-$51FF)                                         */
/* ------------------------------------------------------------------ */
static uint8 map83_read(uint32 address)
{
   (void)address;
   return reg83[2];
}

/* ------------------------------------------------------------------ */
/* Expansion write ($5100-$51FF)                                        */
/* ------------------------------------------------------------------ */
static void map83_apu_write(uint32 address, uint8 value)
{
   if (address >= 0x5101 && address <= 0x5103)
      reg83[2] = value;
}

/* ------------------------------------------------------------------ */
/* HBlank callback — cycle-based IRQ timer                             */
/* ------------------------------------------------------------------ */
static void map83_hblank(int vblank)
{
   (void)vblank; /* timer fires regardless of vblank */
   if (irq83_enabled)
   {
      if (irq83_cnt <= 114)
      {
         nes_irq();
         irq83_enabled = 0;
      }
      else
      {
         irq83_cnt -= 114;
      }
   }
}

/* ------------------------------------------------------------------ */
/* Main write ($8000-$FFFF)                                             */
/* ------------------------------------------------------------------ */
static void map83_write(uint32 address, uint8 value)
{
   switch (address)
   {
   case 0x8000:
   case 0xB000:
   case 0xB0FF:
   case 0xB1FF:
      reg83[0]   = value;
      chr83_bank = (value & 0x30) << 4;   /* block offset: 0, 0x100, 0x200, 0x300 */
      mmc_bankrom(8, 0x8000, value * 2 + 0);
      mmc_bankrom(8, 0xA000, value * 2 + 1);
      mmc_bankrom(8, 0xC000, ((value & 0x30) | 0x0F) * 2 + 0);
      mmc_bankrom(8, 0xE000, ((value & 0x30) | 0x0F) * 2 + 1);
      break;

   case 0x8100:
      if (mmc_getinfo()->vrom_banks <= 32)
         reg83[1] = value;
      switch (value & 0x03)
      {
      case 0x00: ppu_mirror(0, 1, 0, 1); break; /* vertical */
      case 0x01: ppu_mirror(0, 0, 1, 1); break; /* horizontal */
      case 0x02: ppu_mirror(1, 1, 1, 1); break; /* one-screen NT1 */
      case 0x03: ppu_mirror(0, 0, 0, 0); break; /* one-screen NT0 */
      }
      break;

   case 0x8200:
      irq83_cnt = (irq83_cnt & 0xFF00) | (int)value;
      break;

   case 0x8201:
      irq83_cnt     = (irq83_cnt & 0x00FF) | ((int)value << 8);
      irq83_enabled = value;
      break;

   case 0x8300:
      mmc_bankrom(8, 0x8000, value);
      break;

   case 0x8301:
      mmc_bankrom(8, 0xA000, value);
      break;

   case 0x8302:
      mmc_bankrom(8, 0xC000, value);
      break;

   case 0x8310:
      if ((reg83[1] & 0x30) == 0x30)
      {
         mmc_bankvrom(1, 0x0000, chr83_bank ^ value);
      }
      else if ((reg83[1] & 0x30) == 0x10 || (reg83[1] & 0x30) == 0x20)
      {
         mmc_bankvrom(1, 0x0000, (chr83_bank ^ value) * 2 + 0);
         mmc_bankvrom(1, 0x0400, (chr83_bank ^ value) * 2 + 1);
      }
      break;

   case 0x8311:
      if ((reg83[1] & 0x30) == 0x30)
      {
         mmc_bankvrom(1, 0x0400, chr83_bank ^ value);
      }
      else if ((reg83[1] & 0x30) == 0x10 || (reg83[1] & 0x30) == 0x20)
      {
         mmc_bankvrom(1, 0x0800, (chr83_bank ^ value) * 2 + 0);
         mmc_bankvrom(1, 0x0C00, (chr83_bank ^ value) * 2 + 1);
      }
      break;

   case 0x8312:
      if ((reg83[1] & 0x30) == 0x30)
      {
         mmc_bankvrom(1, 0x0800, chr83_bank ^ value);
      }
      else if ((reg83[1] & 0x30) == 0x10 || (reg83[1] & 0x30) == 0x20)
      {
         mmc_bankvrom(1, 0x1000, (chr83_bank ^ value) * 2 + 0);
         mmc_bankvrom(1, 0x1400, (chr83_bank ^ value) * 2 + 1);
      }
      break;

   case 0x8313:
      if ((reg83[1] & 0x30) == 0x30)
      {
         mmc_bankvrom(1, 0x0C00, chr83_bank ^ value);
      }
      else if ((reg83[1] & 0x30) == 0x10 || (reg83[1] & 0x30) == 0x20)
      {
         mmc_bankvrom(1, 0x1800, (chr83_bank ^ value) * 2 + 0);
         mmc_bankvrom(1, 0x1C00, (chr83_bank ^ value) * 2 + 1);
      }
      break;

   case 0x8314:
      if ((reg83[1] & 0x30) == 0x30)
      {
         mmc_bankvrom(1, 0x1000, chr83_bank ^ value);
      }
      else if ((reg83[1] & 0x30) == 0x10 || (reg83[1] & 0x30) == 0x20)
      {
         mmc_bankvrom(1, 0x1000, (chr83_bank ^ value) * 2 + 0);
         mmc_bankvrom(1, 0x1400, (chr83_bank ^ value) * 2 + 1);
      }
      break;

   case 0x8315:
      if ((reg83[1] & 0x30) == 0x30)
      {
         mmc_bankvrom(1, 0x1400, chr83_bank ^ value);
      }
      else if ((reg83[1] & 0x30) == 0x10 || (reg83[1] & 0x30) == 0x20)
      {
         mmc_bankvrom(1, 0x1800, (chr83_bank ^ value) * 2 + 0);
         mmc_bankvrom(1, 0x1C00, (chr83_bank ^ value) * 2 + 1);
      }
      break;

   case 0x8316:
      if ((reg83[1] & 0x30) == 0x30)
      {
         mmc_bankvrom(1, 0x1800, chr83_bank ^ value);
      }
      else if ((reg83[1] & 0x30) == 0x10 || (reg83[1] & 0x30) == 0x20)
      {
         mmc_bankvrom(1, 0x1000, (chr83_bank ^ value) * 2 + 0);
         mmc_bankvrom(1, 0x1400, (chr83_bank ^ value) * 2 + 1);
      }
      break;

   case 0x8317:
      if ((reg83[1] & 0x30) == 0x30)
      {
         mmc_bankvrom(1, 0x1C00, chr83_bank ^ value);
      }
      else if ((reg83[1] & 0x30) == 0x10 || (reg83[1] & 0x30) == 0x20)
      {
         mmc_bankvrom(1, 0x1800, (chr83_bank ^ value) * 2 + 0);
         mmc_bankvrom(1, 0x1C00, (chr83_bank ^ value) * 2 + 1);
      }
      break;

   case 0x8318:
      mmc_bankrom(8, 0x8000, ((reg83[0] & 0x30) | value) * 2 + 0);
      mmc_bankrom(8, 0xA000, ((reg83[0] & 0x30) | value) * 2 + 1);
      break;
   }
}

/* ------------------------------------------------------------------ */
/* Init                                                                  */
/* ------------------------------------------------------------------ */
static void map83_init(void)
{
   int i;
   reg83[0] = reg83[1] = reg83[2] = 0;
   chr83_bank    = 0;
   irq83_enabled = 0;
   irq83_cnt     = 0;

   if (mmc_getinfo()->rom_banks * 2 >= 32)
   {
      /* 256KB+ PRG ROM: map first 2 banks and fixed last block */
      mmc_bankrom(8, 0x8000, 0);
      mmc_bankrom(8, 0xA000, 1);
      mmc_bankrom(8, 0xC000, 30);
      mmc_bankrom(8, 0xE000, 31);
      reg83[1] = 0x30;
   }
   else
   {
      int last = mmc_getinfo()->rom_banks * 2 - 1;
      mmc_bankrom(8, 0x8000, 0);
      mmc_bankrom(8, 0xA000, 1);
      mmc_bankrom(8, 0xC000, last - 1);
      mmc_bankrom(8, 0xE000, last);
   }

   if (mmc_getinfo()->vrom_banks > 0)
   {
      for (i = 0; i < 8; i++)
         mmc_bankvrom(1, i * 0x400, i);
   }
}

static const map_memread map83_memread[] =
{
   { 0x5100, 0x51FF, map83_read },
   {     -1,     -1, NULL }
};

static const map_memwrite map83_memwrite[] =
{
   { 0x5100, 0x51FF, map83_apu_write },
   { 0x8000, 0xFFFF, map83_write },
   {     -1,     -1, NULL }
};

const mapintf_t map83_intf =
{
   83,            /* mapper number */
   "Cony Pirates",/* mapper name */
   map83_init,    /* init */
   NULL,          /* vblank */
   map83_hblank,  /* hblank */
   NULL, NULL,
   map83_memread,
   map83_memwrite,
   NULL
};
