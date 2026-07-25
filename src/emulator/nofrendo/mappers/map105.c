/*
** Nofrendo (c) 1998-2000 Matthew Conte (matt@conte.com)
**
** map105.c
** Mapper 105 (NES World Championships 1990)
**
** Uses the MMC1 serial shift-register interface with a 29-bit IRQ timer.
**
** Registers (5-bit serial, D7 = reset):
**   $8000-$9FFF (reg 0, control): bits 1-0 = mirroring, bits 3-2 = PRG mode
**   $A000-$BFFF (reg 1, CHR0):   bits 3-1 = outer 128KB PRG block,
**                                  bit  4   = IRQ disable (1) / enable (0)
**   $C000-$DFFF (reg 2, CHR1):   unused
**   $E000-$FFFF (reg 3, PRG):    bits 3-0 = inner 16KB bank within outer block
**
** IRQ: 29-bit counter incremented ~114 cycles per scanline (hblank callback).
**      Fires nes_irq() when bit 29 (0x20000000) is reached (~5 minutes).
*/

#include <string.h>
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static int    sr;       /* shift register accumulator */
static int    sr_count; /* bits shifted in so far */
static uint8  regs[4];  /* decoded registers */
static uint32 irq_cnt;  /* 29-bit IRQ counter (CPU cycles via hblank) */
static bool   irq_en;   /* IRQ counting active */

static void map105_sync(void)
{
   int outer = (regs[1] >> 1) & 0x07; /* CHR0 bits 3-1: 128KB outer block */
   int inner = regs[3] & 0x0F;         /* PRG bits 3-0: 16KB inner bank */
   int base  = outer << 3;             /* first 16KB bank of outer block */

   if (!(regs[0] & 0x08))
   {
      /* 32KB PRG mode */
      mmc_bankrom(32, 0x8000, (outer << 2) | (inner >> 1));
   }
   else if (regs[0] & 0x04)
   {
      /* 16KB: $8000 switchable, $C000 fixed to last of outer block */
      mmc_bankrom(16, 0x8000, base + (inner & 0x07));
      mmc_bankrom(16, 0xC000, base + 7);
   }
   else
   {
      /* 16KB: $8000 fixed to first of outer block, $C000 switchable */
      mmc_bankrom(16, 0x8000, base);
      mmc_bankrom(16, 0xC000, base + (inner & 0x07));
   }

   /* IRQ timer: CHR0 bit 4 = 1 disables and resets the counter */
   irq_en = !(regs[1] & 0x10);
   if (!irq_en)
      irq_cnt = 0;

   /* Mirroring from control bits 1-0 */
   switch (regs[0] & 0x03)
   {
   case 0: ppu_mirror(0, 0, 0, 0); break; /* one-screen NT0 */
   case 1: ppu_mirror(1, 1, 1, 1); break; /* one-screen NT1 */
   case 2: ppu_mirror(0, 1, 0, 1); break; /* vertical */
   case 3: ppu_mirror(0, 0, 1, 1); break; /* horizontal */
   }
}

static void map105_write(uint32 address, uint8 value)
{
   int regnum = (address >> 13) & 3; /* A14-A13 -> register 0-3 */

   if (value & 0x80)
   {
      /* D7 set: reset shift register, force 16KB PRG mode */
      sr       = 0;
      sr_count = 0;
      regs[0] |= 0x0C;
      map105_sync();
      return;
   }

   sr |= (value & 1) << sr_count++;

   if (sr_count == 5)
   {
      regs[regnum] = (uint8)sr;
      sr       = 0;
      sr_count = 0;
      map105_sync();
   }
}

static void map105_hblank(int vblank)
{
   UNUSED(vblank);

   if (!irq_en)
      return;

   irq_cnt += 114;

   if (irq_cnt & 0x20000000)
   {
      irq_en  = false;
      irq_cnt = 0;
      nes_irq();
   }
}

static void map105_init(void)
{
   sr       = 0;
   sr_count = 0;
   irq_cnt  = 0;
   irq_en   = false;
   memset(regs, 0, sizeof(regs));

   /* Start in 16KB mode, $8000 switchable, $C000 fixed */
   regs[0] = 0x0C;
   map105_sync();
}

static const map_memwrite map105_memwrite[] =
{
   { 0x8000, 0xFFFF, map105_write },
   {     -1,     -1, NULL }
};

const mapintf_t map105_intf =
{
   105,                 /* mapper number */
   "NWC1990",           /* mapper name */
   map105_init,         /* init routine */
   NULL,                /* vblank callback */
   map105_hblank,       /* hblank callback */
   NULL,                /* get state (snss) */
   NULL,                /* set state (snss) */
   NULL,                /* memory read structure */
   map105_memwrite,     /* memory write structure */
   NULL                 /* external sound device */
};
