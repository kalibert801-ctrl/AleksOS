/*
** Nofrendo (c) 1998-2000 Matthew Conte (matt@conte.com)
**
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of version 2 of the GNU Library General
** Public License as published by the Free Software Foundation.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
** Library General Public License for more details.  To obtain a
** copy of the GNU Library General Public License, write to the Free
** Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** Any permitted reproduction of these routines, in whole or in part,
** must bear this legend.
**
**
** mmclist.c
**
** list of all mapper interfaces
** $Id: mmclist.c,v 1.2 2001/04/27 14:37:11 neil Exp $
*/

#include <noftypes.h>
#include <nes_mmc.h>

/* mapper interfaces */
extern const mapintf_t map0_intf;
extern const mapintf_t map1_intf;
extern const mapintf_t map2_intf;
extern const mapintf_t map3_intf;
extern const mapintf_t map4_intf;
extern const mapintf_t map5_intf;
extern const mapintf_t map6_intf;
extern const mapintf_t map7_intf;
extern const mapintf_t map8_intf;
extern const mapintf_t map9_intf;
extern const mapintf_t map10_intf;
extern const mapintf_t map11_intf;
extern const mapintf_t map13_intf;
extern const mapintf_t map15_intf;
extern const mapintf_t map16_intf;
extern const mapintf_t map17_intf;
extern const mapintf_t map18_intf;
extern const mapintf_t map19_intf;
extern const mapintf_t map21_intf;
extern const mapintf_t map22_intf;
extern const mapintf_t map23_intf;
extern const mapintf_t map24_intf;
extern const mapintf_t map25_intf;
extern const mapintf_t map26_intf;
extern const mapintf_t map32_intf;
extern const mapintf_t map33_intf;
extern const mapintf_t map34_intf;
extern const mapintf_t map40_intf;
extern const mapintf_t map41_intf;
extern const mapintf_t map42_intf;
extern const mapintf_t map43_intf;
extern const mapintf_t map44_intf;
extern const mapintf_t map45_intf;
extern const mapintf_t map46_intf;
extern const mapintf_t map47_intf;
extern const mapintf_t map48_intf;
extern const mapintf_t map49_intf;
extern const mapintf_t map50_intf;
extern const mapintf_t map51_intf;
extern const mapintf_t map57_intf;
extern const mapintf_t map58_intf;
extern const mapintf_t map60_intf;
extern const mapintf_t map61_intf;
extern const mapintf_t map62_intf;
extern const mapintf_t map64_intf;
extern const mapintf_t map65_intf;
extern const mapintf_t map66_intf;
extern const mapintf_t map67_intf;
extern const mapintf_t map68_intf;
extern const mapintf_t map69_intf;
extern const mapintf_t map70_intf;
extern const mapintf_t map71_intf;
extern const mapintf_t map72_intf;
extern const mapintf_t map73_intf;
extern const mapintf_t map74_intf;
extern const mapintf_t map75_intf;
extern const mapintf_t map76_intf;
extern const mapintf_t map77_intf;
extern const mapintf_t map78_intf;
extern const mapintf_t map79_intf;
extern const mapintf_t map80_intf;
extern const mapintf_t map82_intf;
extern const mapintf_t map83_intf;
extern const mapintf_t map85_intf;
extern const mapintf_t map86_intf;
extern const mapintf_t map87_intf;
extern const mapintf_t map88_intf;
extern const mapintf_t map89_intf;
extern const mapintf_t map90_intf;
extern const mapintf_t map91_intf;
extern const mapintf_t map92_intf;
extern const mapintf_t map93_intf;
extern const mapintf_t map94_intf;
extern const mapintf_t map95_intf;
extern const mapintf_t map96_intf;
extern const mapintf_t map97_intf;
extern const mapintf_t map99_intf;
extern const mapintf_t map100_intf;
extern const mapintf_t map101_intf;
extern const mapintf_t map105_intf;
extern const mapintf_t map107_intf;
extern const mapintf_t map108_intf;
extern const mapintf_t map109_intf;
extern const mapintf_t map110_intf;
extern const mapintf_t map112_intf;
extern const mapintf_t map113_intf;
extern const mapintf_t map114_intf;
extern const mapintf_t map115_intf;
extern const mapintf_t map116_intf;
extern const mapintf_t map117_intf;
extern const mapintf_t map118_intf;
extern const mapintf_t map119_intf;
extern const mapintf_t map122_intf;
extern const mapintf_t map133_intf;
extern const mapintf_t map134_intf;
extern const mapintf_t map135_intf;
extern const mapintf_t map140_intf;
extern const mapintf_t map151_intf;
extern const mapintf_t map152_intf;
extern const mapintf_t map160_intf;
extern const mapintf_t map180_intf;
extern const mapintf_t map181_intf;
extern const mapintf_t map182_intf;
extern const mapintf_t map183_intf;
extern const mapintf_t map184_intf;
extern const mapintf_t map185_intf;
extern const mapintf_t map187_intf;
extern const mapintf_t map188_intf;
extern const mapintf_t map189_intf;
extern const mapintf_t map191_intf;
extern const mapintf_t map193_intf;
extern const mapintf_t map194_intf;
extern const mapintf_t map200_intf;
extern const mapintf_t map201_intf;
extern const mapintf_t map202_intf;
extern const mapintf_t map206_intf;
extern const mapintf_t map222_intf;
extern const mapintf_t map225_intf;
extern const mapintf_t map226_intf;
extern const mapintf_t map227_intf;
extern const mapintf_t map228_intf;
extern const mapintf_t map229_intf;
extern const mapintf_t map230_intf;
extern const mapintf_t map231_intf;
extern const mapintf_t map232_intf;
extern const mapintf_t map233_intf;
extern const mapintf_t map234_intf;
extern const mapintf_t map235_intf;
extern const mapintf_t map236_intf;
extern const mapintf_t map240_intf;
extern const mapintf_t map241_intf;
extern const mapintf_t map242_intf;
extern const mapintf_t map243_intf;
extern const mapintf_t map244_intf;
extern const mapintf_t map245_intf;
extern const mapintf_t map246_intf;
extern const mapintf_t map248_intf;
extern const mapintf_t map249_intf;
extern const mapintf_t map251_intf;
extern const mapintf_t map252_intf;
extern const mapintf_t map255_intf;

/* implemented mapper interfaces */
const mapintf_t *mappers[] =
{
   &map0_intf,
   &map1_intf,
   &map2_intf,
   &map3_intf,
   &map4_intf,
   &map5_intf,
   &map6_intf,
   &map7_intf,
   &map8_intf,
   &map9_intf,
   &map10_intf,
   &map11_intf,
   &map13_intf,
   &map15_intf,
   &map16_intf,
   &map17_intf,
   &map18_intf,
   &map19_intf,
   &map21_intf,
   &map22_intf,
   &map23_intf,
   &map24_intf,
   &map25_intf,
   &map26_intf,
   &map32_intf,
   &map33_intf,
   &map34_intf,
   &map40_intf,
   &map41_intf,
   &map42_intf,
   &map43_intf,
   &map44_intf,
   &map45_intf,
   &map46_intf,
   &map47_intf,
   &map48_intf,
   &map49_intf,
   &map50_intf,
   &map51_intf,
   &map57_intf,
   &map58_intf,
   &map60_intf,
   &map61_intf,
   &map62_intf,
   &map64_intf,
   &map65_intf,
   &map66_intf,
   &map67_intf,
   &map68_intf,
   &map69_intf,
   &map70_intf,
   &map71_intf,
   &map72_intf,
   &map73_intf,
   &map74_intf,
   &map75_intf,
   &map76_intf,
   &map77_intf,
   &map78_intf,
   &map79_intf,
   &map80_intf,
   &map82_intf,
   &map83_intf,
   &map85_intf,
   &map86_intf,
   &map87_intf,
   &map88_intf,
   &map89_intf,
   &map90_intf,
   &map91_intf,
   &map92_intf,
   &map93_intf,
   &map94_intf,
   &map95_intf,
   &map96_intf,
   &map97_intf,
   &map99_intf,
   &map100_intf,
   &map101_intf,
   &map105_intf,
   &map107_intf,
   &map108_intf,
   &map109_intf,
   &map110_intf,
   &map112_intf,
   &map113_intf,
   &map114_intf,
   &map115_intf,
   &map116_intf,
   &map117_intf,
   &map118_intf,
   &map119_intf,
   &map122_intf,
   &map133_intf,
   &map134_intf,
   &map135_intf,
   &map140_intf,
   &map151_intf,
   &map152_intf,
   &map160_intf,
   &map180_intf,
   &map181_intf,
   &map182_intf,
   &map183_intf,
   &map184_intf,
   &map185_intf,
   &map187_intf,
   &map188_intf,
   &map189_intf,
   &map191_intf,
   &map193_intf,
   &map194_intf,
   &map200_intf,
   &map201_intf,
   &map202_intf,
   &map206_intf,
   &map222_intf,
   &map225_intf,
   &map226_intf,
   &map227_intf,
   &map228_intf,
   &map229_intf,
   &map230_intf,
   &map231_intf,
   &map232_intf,
   &map233_intf,
   &map234_intf,
   &map235_intf,
   &map236_intf,
   &map240_intf,
   &map241_intf,
   &map242_intf,
   &map243_intf,
   &map244_intf,
   &map245_intf,
   &map246_intf,
   &map248_intf,
   &map249_intf,
   &map251_intf,
   &map252_intf,
   &map255_intf,
   NULL
};

/*
** $Log: mmclist.c,v $
** Revision 1.2  2001/04/27 14:37:11  neil
** wheeee
**
** Revision 1.1.1.1  2001/04/27 07:03:54  neil
** initial
**
** Revision 1.1  2000/10/24 12:20:28  matt
** changed directory structure
**
** Revision 1.2  2000/10/10 13:05:30  matt
** Mr. Clean makes a guest appearance
**
** Revision 1.1  2000/07/31 04:27:39  matt
** initial revision
**
*/
