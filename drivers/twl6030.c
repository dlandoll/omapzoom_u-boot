/*
 * (C) Copyright 2009
 * Texas Instruments, <www.ti.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#include <config.h>
#ifdef CONFIG_TWL6030

#include <twl6030.h>

/* Functions to read and write from TWL6030 */
static inline int twl6030_i2c_write_u8(u8 chip_no, u8 val, u8 reg)
{
	return i2c_write(chip_no, reg, 1, &val, 1);
}

static inline int twl6030_i2c_read_u8(u8 chip_no, u8 *val, u8 reg)
{
	return i2c_read(chip_no, reg, 1, val, 1);
}

static int twl6030_gpadc_read_channel(t_twl6030_gpadc_data * gpadc, u8 channel_no)
{
	u8 lsb = 0;
	u8 msb = 0;
	int ret = 0;
	u8 channel = channel_no;

	if (gpadc->twl_chip_type == chip_TWL6032)
		channel = 0;

	ret = twl6030_i2c_read_u8(TWL6030_CHIP_ADC, &lsb,
			gpadc->rbase + channel * 2);
	if (ret)
		return ret;

	ret = twl6030_i2c_read_u8(TWL6030_CHIP_ADC, &msb,
			gpadc->rbase + 1 + channel * 2);
	if (ret)
		return ret;

	return (msb << 8) | lsb;
}

static int twl6030_gpadc_sw2_trigger(t_twl6030_gpadc_data * gpadc)
{
	u8 val;
	int ret = 0;

	ret = twl6030_i2c_write_u8(TWL6030_CHIP_ADC, gpadc->enable, gpadc->ctrl);
	if (ret)
		return ret;

	/* Waiting until the SW1 conversion ends*/
	val =  TWL6030_GPADC_BUSY;

	while (!((val & TWL6030_GPADC_EOC_SW) && (!(val & TWL6030_GPADC_BUSY)))) {
		ret = twl6030_i2c_read_u8(TWL6030_CHIP_ADC, &val, gpadc->ctrl);
		if (ret)
			return ret;
		udelay(1000);
	}

	return 0;
}


void twl6030_start_usb_charging(void)
{
	twl6030_i2c_write_u8(TWL6030_CHIP_CHARGER, CHARGERUSB_VICHRG_1500,
							CHARGERUSB_VICHRG);
	twl6030_i2c_write_u8(TWL6030_CHIP_CHARGER, CHARGERUSB_CIN_LIMIT_NONE,
							CHARGERUSB_CINLIMIT);
	twl6030_i2c_write_u8(TWL6030_CHIP_CHARGER, MBAT_TEMP,
							CONTROLLER_INT_MASK);
	twl6030_i2c_write_u8(TWL6030_CHIP_CHARGER, MASK_MCHARGERUSB_THMREG,
							CHARGERUSB_INT_MASK);
	twl6030_i2c_write_u8(TWL6030_CHIP_CHARGER, CHARGERUSB_VOREG_4P0,
							CHARGERUSB_VOREG);
	twl6030_i2c_write_u8(TWL6030_CHIP_CHARGER, CHARGERUSB_CTRL2_VITERM_100,
							CHARGERUSB_CTRL2);
	/* Enable USB charging */
	twl6030_i2c_write_u8(TWL6030_CHIP_CHARGER, CONTROLLER_CTRL1_EN_CHARGER,
							CONTROLLER_CTRL1);

	return;
}

int twl6030_get_battery_voltage(t_twl6030_gpadc_data * gpadc)
{
	int battery_volt = 0;
	int ret = 0;
	u8 vbatch = TWL6030_GPADC_VBAT_CHNL;

	if (gpadc->twl_chip_type == chip_TWL6032) {
		vbatch = TWL6032_GPADC_VBAT_CHNL;
		twl6030_i2c_write_u8(TWL6030_CHIP_ADC, vbatch, TWL6032_GPSELECT_ISB);
	}

	/* Start GPADC SW conversion */
	ret = twl6030_gpadc_sw2_trigger(gpadc);
	if (ret) {
		printf("Failed to convert battery voltage\n");
		return ret;
	}

	/* measure Vbat voltage */
	battery_volt = twl6030_gpadc_read_channel(gpadc, vbatch);
	if (battery_volt < 0) {
		printf("Failed to read battery voltage\n");
		return ret;
	}

	if (gpadc->twl_chip_type == chip_TWL6030)
		battery_volt = (battery_volt * 25 * 1000) >> (10 + 2);
	else
		battery_volt = (battery_volt * 25 * 1000) >> (12 + 2);

	return battery_volt;
}

void twl6030_init_battery_charging(void)
{
	u8 stat1 = 0;
	int battery_volt = 0;
	int ret = 0;
	t_twl6030_gpadc_data gpadc;
	u8 val;
	int abort = 0;
	int chargedelay = 5;

	gpadc.twl_chip_type = chip_TWL6030;
	gpadc.rbase = GPCH0_LSB;
	gpadc.ctrl = CTRL_P2;
	gpadc.enable = CTRL_P2_SP2;

	ret = twl6030_i2c_read_u8(TWL6030_CHIP_USB, &val, USB_PRODUCT_ID_LSB);

	if (ret == 0) {
		if(val == 0x32)
		{
			gpadc.twl_chip_type = chip_TWL6032;
			gpadc.rbase = TWL6032_GPCH0_LSB;
			gpadc.ctrl = TWL6032_CTRL_P1;
			gpadc.enable = CTRL_P1_SP1;
		}
	} else {
		printf("twl6030_init_battery_charging(): "
				"could not determine chip! "
				"TWL6030 will be used\n");
	}

	twl6030_start_usb_charging();

	/* Enable VBAT measurement */
	if (gpadc.twl_chip_type == chip_TWL6030)
		twl6030_i2c_write_u8(TWL6030_CHIP_PM, VBAT_MEAS, MISC1);
	else
		twl6030_i2c_write_u8(TWL6030_CHIP_ADC, GPADC_CTRL2_CH18_SCALER_EN, TWL6032_GPADC_CTRL2);

	/* Enable GPADC module */
	ret = twl6030_i2c_write_u8(TWL6030_CHIP_CHARGER, FGS | GPADCS, TOGGLE1);
	if (ret) {
		printf("Failed to enable GPADC\n");
		return;
	}

	/*
	 * Make dummy conversion for the TWL6032
	 * (first conversion may be failed)
	 */
	if (gpadc.twl_chip_type == chip_TWL6032)
		twl6030_gpadc_sw2_trigger(&gpadc);

	battery_volt = twl6030_get_battery_voltage(&gpadc);
	if (battery_volt < 0)
		return;

	if (battery_volt < 3400) {

	#ifdef CONFIG_SILENT_CONSOLE
		if (gd->flags & GD_FLG_SILENT) {
			/* Restore serial console */
			console_assign (stdout, "serial");
			console_assign (stderr, "serial");
		}
	#endif

		printf("Main battery voltage too low!\n");
		printf("Hit any key to stop charging: %2d ", chargedelay);

		if (tstc()) {	/* we got a key press	*/
			(void) getc();  /* consume input	*/
		}

		while ((chargedelay > 0) && (!abort)) {
			int i;

			--chargedelay;
			/* delay 100 * 10ms */
			for (i=0; !abort && i<100; ++i) {
				if (tstc()) {	/* we got a key press	*/
					abort  = 1;	/* don't auto boot	*/
					chargedelay = 0;	/* no more delay	*/
					(void) getc();  /* consume input	*/
					break;
				}
				udelay (10000);
			}
			printf ("\b\b\b%2d ", chargedelay);
		}
		putc ('\n');

	#ifdef CONFIG_SILENT_CONSOLE
		if (gd->flags & GD_FLG_SILENT) {
			/* Restore silent console */
			console_assign (stdout, "nulldev");
			console_assign (stderr, "nulldev");
		}
	#endif

		if (!abort)
		{
			printf("Charging...\n");

			/* wait for battery to charge to the level when kernel can boot */
			while (battery_volt < 3400) {
				battery_volt = twl6030_get_battery_voltage(&gpadc);
				printf("\rBattery Voltage: %d mV", battery_volt);
			}
			printf("\n");
		}
	}
}

void twl6030_usb_device_settings()
{
	u8 data = 0;

	/* Select APP Group and set state to ON */
	twl6030_i2c_write_u8(TWL6030_CHIP_PM, 0x21, VUSB_CFG_STATE);

	twl6030_i2c_read_u8(TWL6030_CHIP_PM, &data, MISC2);
	data |= 0x10;

	/* Select the input supply for VBUS regulator */
	twl6030_i2c_write_u8(TWL6030_CHIP_PM, data, MISC2);
}
#endif
