/*
** map135.c
** Mapper 135 — SACHEN CHEN
*/
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint8 reg135_cmd;
static uint8 reg135_chr0l, reg135_chr1l, reg135_chr0h, reg135_chr1h, reg135_chrch;

static void map135_set_ppu(void)
{
   uint32 a = (reg135_chr0l << 2) | (reg135_chrch << 5);
   uint32 b = 2 | (reg135_chr0h << 2) | (reg135_chrch << 5);
   uint32 c = (reg135_chr1l << 2) | (reg135_chrch << 5);
   uint32 d = 2 | (reg135_chr1h << 2) | (reg135_chrch << 5);

   mmc_bankvrom(1, 0x0000, a + 0);
   mmc_bankvrom(1, 0x0400, a + 1);
   mmc_bankvrom(1, 0x0800, b + 0);
   mmc_bankvrom(1, 0x0C00, b + 1);
   mmc_bankvrom(1, 0x1000, c + 0);
   mmc_bankvrom(1, 0x1400, c + 1);
   mmc_bankvrom(1, 0x1800, d + 0);
   mmc_bankvrom(1, 0x1C00, d + 1);
}

static void map135_write(uint32 address, uint8 value)
{
   switch (address & 0x4101)
   {
   case 0x4100:
      reg135_cmd = value & 0x07;
      break;
   case 0x4101:
      switch (reg135_cmd)
      {
      case 0: reg135_chr0l = value & 0x07; map135_set_ppu(); break;
      case 1: reg135_chr0h = value & 0x07; map135_set_ppu(); break;
      case 2: reg135_chr1l = value & 0x07; map135_set_ppu(); break;
      case 3: reg135_chr1h = value & 0x07; map135_set_ppu(); break;
      case 4: reg135_chrch = value & 0x07; map135_set_ppu(); break;
      case 5:
         {
            uint8 prg = (value % 7) << 2;
            mmc_bankrom(8, 0x8000, prg + 0);
            mmc_bankrom(8, 0xA000, prg + 1);
            mmc_bankrom(8, 0xC000, prg + 2);
            mmc_bankrom(8, 0xE000, prg + 3);
         }
         break;
      case 6:
         break;
      case 7:
         switch ((value >> 1) & 0x03)
         {
         case 0: ppu_mirror(0, 0, 0, 0); break; /* NT0 */
         case 1: ppu_mirror(0, 0, 1, 1); break; /* horizontal */
         case 2: ppu_mirror(0, 1, 0, 1); break; /* vertical */
         case 3: ppu_mirror(0, 0, 0, 0); break; /* NT0 */
         }
         break;
      }
      break;
   }
}

static void map135_init(void)
{
   reg135_cmd = 0;
   reg135_chr0l = reg135_chr1l = reg135_chr0h = reg135_chr1h = reg135_chrch = 0;
   mmc_bankrom(8, 0x8000, 0);
   mmc_bankrom(8, 0xA000, 1);
   mmc_bankrom(8, 0xC000, 2);
   mmc_bankrom(8, 0xE000, 3);
   map135_set_ppu();
}

static const map_memwrite map135_memwrite[] =
{
   { 0x4100, 0x5FFF, map135_write },
   {     -1,     -1, NULL }
};

const mapintf_t map135_intf =
{
   135,              /* mapper number */
   "SACHEN CHEN 2",  /* mapper name */
   map135_init,      /* init */
   NULL,             /* vblank */
   NULL,             /* hblank */
   NULL, NULL, NULL,
   map135_memwrite,
   NULL
};
