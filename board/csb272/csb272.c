/*
 * (C) Copyright 2004
 * Tolunay Orkun, Nextio Inc., torkun@nextio.com
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <asm/u-boot.h>
#include <asm/processor.h>
#include <common.h>
#include <i2c.h>
#include <miiphy.h>
#include <405gp_enet.h>

/*
 * Configuration data for AMIS FS6377-01 Programmable 3-PLL Clock Generator
 *
 * CLKA output => Epson LCD Controller
 * CLKB output => Not Connected
 * CLKC output => Ethernet
 * CLKD output => UART external clock
 *
 * Note: these values are obtained from device after init by micromonitor
*/
uchar pll_fs6377_regs[16] = {
	0x28, 0xef, 0x53, 0x03, 0x4b, 0x80, 0x32, 0x80,
	0x94, 0x32, 0x80, 0xd4, 0x56, 0xf6, 0xf6, 0xe0 };

/*
 * pll_init: Initialize AMIS IC FS6377-01 PLL
 *
 * PLL supplies Epson LCD Clock, Ethernet Clock and UART external clock
 *
 */
int pll_init(void)
{
	i2c_init(CFG_I2C_SPEED, CFG_I2C_SLAVE);

	return  i2c_write(CFG_I2C_PLL_ADDR, 0, 1,
		(uchar *) pll_fs6377_regs, sizeof(pll_fs6377_regs));
}

/*
 * board_pre_init: do any preliminary board initialization
 *
 */
int board_pre_init(void)
{
	/* initialize PLL so UART, LCD, Ethernet clocked at correctly */
	(void) get_clocks();
	pll_init();

   /*-------------------------------------------------------------------------+
   | Interrupt controller setup for the Walnut board.
   | Note: IRQ 0-15  405GP internally generated; active high; level sensitive
   |       IRQ 16    405GP internally generated; active low; level sensitive
   |       IRQ 17-24 RESERVED
   |       IRQ 25 (EXT IRQ 0) FPGA; active high; level sensitive
   |       IRQ 26 (EXT IRQ 1) SMI; active high; level sensitive
   |       IRQ 27 (EXT IRQ 2) Not Used
   |       IRQ 28 (EXT IRQ 3) PCI SLOT 3; active low; level sensitive
   |       IRQ 29 (EXT IRQ 4) PCI SLOT 2; active low; level sensitive
   |       IRQ 30 (EXT IRQ 5) PCI SLOT 1; active low; level sensitive
   |       IRQ 31 (EXT IRQ 6) PCI SLOT 0; active low; level sensitive
   | Note for Walnut board:
   |       An interrupt taken for the FPGA (IRQ 25) indicates that either
   |       the Mouse, Keyboard, IRDA, or External Expansion caused the
   |       interrupt. The FPGA must be read to determine which device
   |       caused the interrupt. The default setting of the FPGA clears
   |
   +-------------------------------------------------------------------------*/

	mtdcr (uicsr, 0xFFFFFFFF);   /* clear all ints */
	mtdcr (uicer, 0x00000000);   /* disable all ints */
	mtdcr (uiccr, 0x00000000);   /* set all to be non-critical */
	mtdcr (uicpr, 0xFFFFFF83);   /* set int polarities */
	mtdcr (uictr, 0x10000000);   /* set int trigger levels */
	mtdcr (uicvcr, 0x00000001);  /* set vect base=0,INT0 highest priority */
	mtdcr (uicsr, 0xFFFFFFFF);   /* clear all ints */

	mtebc (epcr, 0xa8400000);   /* EBC always driven */

	return 0; /* success */
}

/*
 * checkboard: identify/verify the board we are running
 *
 * Remark: we just assume it is correct board here!
 *
 */
int checkboard(void)
{
	printf("BOARD: Cogent CSB272\n");

	return 0; /* success */
}

/*
 * initram: Determine the size of mounted DRAM
 *
 * Size is determined by reading SDRAM configuration registers as
 * configured by initialization code
 *
 */
long initdram (int board_type)
{
	ulong tot_size;
	ulong bank_size;
	ulong tmp;

	tot_size = 0;

	mtdcr (memcfga, mem_mb0cf);
	tmp = mfdcr (memcfgd);
	if (tmp & 0x00000001) {
		bank_size = 0x00400000 << ((tmp >> 17) & 0x7);
		tot_size += bank_size;
	}

	mtdcr (memcfga, mem_mb1cf);
	tmp = mfdcr (memcfgd);
	if (tmp & 0x00000001) {
		bank_size = 0x00400000 << ((tmp >> 17) & 0x7);
		tot_size += bank_size;
	}

	mtdcr (memcfga, mem_mb2cf);
	tmp = mfdcr (memcfgd);
	if (tmp & 0x00000001) {
		bank_size = 0x00400000 << ((tmp >> 17) & 0x7);
		tot_size += bank_size;
	}

	mtdcr (memcfga, mem_mb3cf);
	tmp = mfdcr (memcfgd);
	if (tmp & 0x00000001) {
		bank_size = 0x00400000 << ((tmp >> 17) & 0x7);
		tot_size += bank_size;
	}

	return tot_size;
}

/*
 * last_stage_init: final configurations (such as PHY etc)
 *
 */
int last_stage_init(void)
{
	/* initialize the PHY */
	miiphy_reset(CONFIG_PHY_ADDR);
	miiphy_write(CONFIG_PHY_ADDR, PHY_BMCR,
			PHY_BMCR_AUTON | PHY_BMCR_RST_NEG);	/* AUTO neg */
	miiphy_write(CONFIG_PHY_ADDR, PHY_FCSCR, 0x0d08);	/* LEDs     */

	return 0; /* success */
}