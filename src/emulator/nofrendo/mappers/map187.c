/*
** map187.c
** Mapper 187 — Street Fighter Zero 2 97 (MMC3 pirate variant with extended PRG/CHR)
*/

#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static int   chr[8];    /* 8KB CHR bank indices (chr[4..7] may carry +0x100 offset) */
static uint8 bank[8];   /* raw values written at $8001 per register */
static uint8 prg[4];    /* 8KB PRG page indices for slots $8000/$A000/$C000/$E000 */
static uint8 ext_mode, chr_mode, ext_enable;
static uint8 irq_enable, irq_counter, irq_latch, irq_occur;
static uint8 last_write;

static void map187_set_cpu(void)
{
   mmc_bankrom(8, 0x8000, prg[0]);
   mmc_bankrom(8, 0xA000, prg[1]);
   mmc_bankrom(8, 0xC000, prg[2]);
   mmc_bankrom(8, 0xE000, prg[3]);
}

/* CHR formula: chr[n] stores an 8KB-unit index; <<3 converts to 1KB pages; +n
** is the sub-page offset.  chr[4..7] carry a +0x100 bias so they reference the
** extended CHR region of this pirate cartridge. */
static void map187_set_ppu(void)
{
   mmc_bankvrom(1, 0x0000, (chr[0] << 3) + 0);
   mmc_bankvrom(1, 0x0400, (chr[1] << 3) + 1);
   mmc_bankvrom(1, 0x0800, (chr[2] << 3) + 2);
   mmc_bankvrom(1, 0x0C00, (chr[3] << 3) + 3);
   mmc_bankvrom(1, 0x1000, (chr[4] << 3) + 4);
   mmc_bankvrom(1, 0x1400, (chr[5] << 3) + 5);
   mmc_bankvrom(1, 0x1800, (chr[6] << 3) + 6);
   mmc_bankvrom(1, 0x1C00, (chr[7] << 3) + 7);
}

static void map187_write(uint32 address, uint8 value)
{
   last_write = value;
   switch (address)
   {
   case 0x8003:
      ext_enable = 0xFF;
      chr_mode   = value;
      if ((value & 0xF0) == 0) {
         prg[2] = mmc_getinfo()->rom_banks * 2 - 2;
         map187_set_cpu();
      }
      break;

   case 0x8000:
      ext_enable = 0;
      chr_mode   = value;
      break;

   case 0x8001:
      if (!ext_enable) {
         switch (chr_mode & 7) {
         case 0:
            value &= 0xFE;
            chr[4] = (int)value + 0x100;
            chr[5] = (int)value + 0x100 + 1;
            map187_set_ppu();
            break;
         case 1:
            value &= 0xFE;
            chr[6] = (int)value + 0x100;
            chr[7] = (int)value + 0x100 + 1;
            map187_set_ppu();
            break;
         case 2:
            chr[0] = value;
            map187_set_ppu();
            break;
         case 3:
            chr[1] = value;
            map187_set_ppu();
            break;
         case 4:
            chr[2] = value;
            map187_set_ppu();
            break;
         case 5:
            chr[3] = value;
            map187_set_ppu();
            break;
         case 6:
            if ((ext_mode & 0xA0) != 0xA0) {
               prg[0] = value;
               map187_set_cpu();
            }
            break;
         case 7:
            if ((ext_mode & 0xA0) != 0xA0) {
               prg[1] = value;
               map187_set_cpu();
            }
            break;
         }
      } else {
         switch (chr_mode) {
         case 0x2A:
            prg[1] = 0x0F;
            break;
         case 0x28:
            prg[2] = 0x17;
            break;
         default:
            break;
         }
         map187_set_cpu();
      }
      bank[chr_mode & 7] = value;
      break;

   case 0xA000:
      if (value & 0x01)
         ppu_mirror(0, 0, 1, 1); /* horizontal */
      else
         ppu_mirror(0, 1, 0, 1); /* vertical */
      break;
   case 0xA001:
      break;

   case 0xC000:
      irq_counter = value;
      irq_occur   = 0;
      break;
   case 0xC001:
      irq_latch = value;
      irq_occur = 0;
      break;
   case 0xE000:
   case 0xE002:
      irq_enable = 0;
      irq_occur  = 0;
      break;
   case 0xE001:
   case 0xE003:
      irq_enable = 1;
      irq_occur  = 0;
      break;
   }
}

/* Extended PRG control written at $5000 */
static void map187_apu_write(uint32 address, uint8 value)
{
   last_write = value;
   if (address != 0x5000)
      return;
   ext_mode = value;
   if (value & 0x80) {
      if (value & 0x20) {
         prg[0] = ((value & 0x1E) << 1) + 0;
         prg[1] = ((value & 0x1E) << 1) + 1;
         prg[2] = ((value & 0x1E) << 1) + 2;
         prg[3] = ((value & 0x1E) << 1) + 3;
      } else {
         prg[2] = ((value & 0x1F) << 1) + 0;
         prg[3] = ((value & 0x1F) << 1) + 1;
      }
   } else {
      prg[0] = bank[6];
      prg[1] = bank[7];
      prg[2] = mmc_getinfo()->rom_banks * 2 - 2;
      prg[3] = mmc_getinfo()->rom_banks * 2 - 1;
   }
   map187_set_cpu();
}

/* Security read: returns a value based on the low 2 bits of the last APU write */
static uint8 map187_read(uint32 address)
{
   (void)address;
   switch (last_write & 0x03) {
   case 0: return 0x83;
   case 1: return 0x83;
   case 2: return 0x42;
   case 3: return 0x00;
   }
   return 0;
}

static void map187_hblank(int vblank)
{
   if (!vblank && irq_enable) {
      if (!irq_counter) {
         irq_counter--;
         irq_enable = 0;
         irq_occur  = 0xFF;
      } else {
         irq_counter--;
      }
   }
   if (irq_occur)
      nes_irq();
}

static void map187_init(void)
{
   int i, nb;
   for (i = 0; i < 8; i++) {
      chr[i]  = 0;
      bank[i] = 0;
   }
   nb     = mmc_getinfo()->rom_banks * 2;
   prg[0] = nb - 4;
   prg[1] = nb - 3;
   prg[2] = nb - 2;
   prg[3] = nb - 1;
   map187_set_cpu();
   map187_set_ppu();

   ext_mode = ext_enable = chr_mode = 0;
   irq_enable = irq_counter = irq_latch = irq_occur = last_write = 0;
}

static const map_memread map187_memread[] =
{
   { 0x5000, 0x5FFF, map187_read },
   {     -1,     -1, NULL }
};

static const map_memwrite map187_memwrite[] =
{
   { 0x5000, 0x5FFF, map187_apu_write },
   { 0x8000, 0xFFFF, map187_write },
   {     -1,     -1, NULL }
};

const mapintf_t map187_intf =
{
   187,              /* mapper number */
   "SF Zero 2 97",  /* mapper name */
   map187_init,      /* init */
   NULL,             /* vblank */
   map187_hblank,    /* hblank */
   NULL,             /* getstate */
   NULL,             /* setstate */
   map187_memread,
   map187_memwrite,
   NULL
};
