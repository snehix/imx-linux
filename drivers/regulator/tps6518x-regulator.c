/*
 * Copyright (C) 2010 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/mfd/tps6518x.h>
#include <linux/gpio.h>
#include <linux/pmic_status.h>
#include <linux/of_gpio.h>

struct tps6518x_data {
	int num_regulators;
	struct tps6518x *tps6518x;
	struct regulator_dev **rdev;
};


static int tps6518x_pass_num = { 2 };
//static int tps6518x_vcom = { -2680000 };
//static int tps6518x_vcom = { -1400000 };
//static int tps6518x_vcom = { -1830000 };
//static int tps6518x_vcom = { -1730000 };
static int tps6518x_vcom = { -1390000 };
static int tps65180_current_Enable_Register = 0;

static int tps6518x_is_power_good(struct tps6518x *tps6518x);
/*
 * to_reg_val(): Creates a register value with new data
 *
 * Creates a new register value for a particular field.  The data
 * outside of the new field is not modified.
 *
 * @cur_reg: current value in register
 * @reg_mask: mask of field bits to be modified
 * @fld_val: new value for register field.
 */
static unsigned int to_reg_val(unsigned int cur_reg, unsigned int fld_mask,
							   unsigned int fld_val)
{
	return (cur_reg & (~fld_mask)) | fld_val;
}

/*
 * Regulator operations
 */
/* Convert uV to the VCOM register bitfield setting */

static int vcom_rs_to_uV(unsigned int reg_setting)
{
	if (reg_setting <= TPS65180_VCOM_MIN_SET)
		return TPS65180_VCOM_MIN_uV;
	if (reg_setting >= TPS65180_VCOM_MAX_SET)
		return TPS65180_VCOM_MAX_uV;
	return -(reg_setting * TPS65180_VCOM_STEP_uV);
}
static int vcom2_rs_to_uV(unsigned int reg_setting)
{
	if (reg_setting <= TPS65185_VCOM_MIN_SET)
		return TPS65185_VCOM_MIN_uV;
	if (reg_setting >= TPS65185_VCOM_MAX_SET)
		return TPS65185_VCOM_MAX_uV;
	return -(reg_setting * TPS65185_VCOM_STEP_uV);
}


static int vcom_uV_to_rs(int uV)
{
	if (uV <= TPS65180_VCOM_MIN_uV)
		return TPS65180_VCOM_MIN_SET;
	if (uV >= TPS65180_VCOM_MAX_uV)
		return TPS65180_VCOM_MAX_SET;
	return (-uV) / TPS65180_VCOM_STEP_uV;
}

static int vcom2_uV_to_rs(int uV)
{
	if (uV <= TPS65185_VCOM_MIN_uV)
		return TPS65185_VCOM_MIN_SET;
	if (uV >= TPS65185_VCOM_MAX_uV)
		return TPS65185_VCOM_MAX_SET;
	return (-uV) / TPS65185_VCOM_STEP_uV;
}


static int epdc_pwr0_enable(struct regulator_dev *reg)
{
	struct tps6518x *tps6518x = rdev_get_drvdata(reg);
	
	//printk("epdc_pwr0_enable\n");

	gpio_set_value(tps6518x->gpio_pmic_powerup, 1);

	return 0;

}

static int epdc_pwr0_disable(struct regulator_dev *reg)
{
	struct tps6518x *tps6518x = rdev_get_drvdata(reg);
	
	//printk("epdc_pwr0_disable\n");

	gpio_set_value(tps6518x->gpio_pmic_powerup, 0);

	return 0;

}

static int epdc_pwr0_is_enabled(struct regulator_dev *reg)
{
	struct tps6518x *tps6518x = rdev_get_drvdata(reg);
	
	//printk("epdc_pwr0_is_enabled\n");


	int gpio = gpio_get_value(tps6518x->gpio_pmic_powerup);

	//printk("epdc_pwr0_is_enabled end, return=%d\n",gpio);
	
	return gpio;
	
}

static int tps6518x_v3p3_enable(struct regulator_dev *reg)
{
	struct tps6518x *tps6518x = rdev_get_drvdata(reg);
	
	//printk("epdc_v3p3_enable\n");


	return 0;


	gpio_set_value(tps6518x->gpio_pmic_v3p3_ctrl, 1);
	return 0;
}

static int tps6518x_v3p3_disable(struct regulator_dev *reg)
{
	struct tps6518x *tps6518x = rdev_get_drvdata(reg);
	
	//printk("epdc_v3p3_disable\n");
	
	//tps6518x_reg_write(REG_TPS65180_ENABLE, 0x40);
	//msleep(24);
	//epdc_pwr0_disable(reg);

	return 0;

	gpio_set_value(tps6518x->gpio_pmic_v3p3_ctrl, 0);
	return 0;

}
static int tps6518x_v3p3_is_enabled(struct regulator_dev *reg)
{
	struct tps6518x *tps6518x = rdev_get_drvdata(reg);
	
	//printk("tps6518x_v3p3_is_enabled\n");

	return 1;

	int gpio = gpio_get_value(tps6518x->gpio_pmic_v3p3_ctrl);

	//printk("tps6518x_v3p3_is_enabled end, return=%d\n",gpio);


	return gpio;
}

static int tps6518x_vcom_set_voltage(struct regulator_dev *reg,
					int minuV, int uV, unsigned *selector)
{
	struct tps6518x *tps6518x = rdev_get_drvdata(reg);
	unsigned int cur_reg_val; /* current register value to modify */
	unsigned int new_reg_val; /* new register value to write */
	int retval;
	int gpio;

	int tmpdata,tmpdata1;
	int flag_prgo;
	
	//printk("tps6518x_vcom_set_voltage\n");

	/*
	 * this will not work on tps65182
	 */
	if (tps6518x->revID == 65182)
		return 0;

	uV=tps6518x_vcom;	
#if 0
	if (uV < 200000)
		return 0;
#endif
	switch (tps6518x->revID & 15)
	{
		case 0 : /* TPS65180 */
		case 1 : /* TPS65181 */
		case 4 : /* TPS65180-rev1 */
			tps6518x_reg_read(REG_TPS65180_VCOM_ADJUST,&cur_reg_val);
			new_reg_val = to_reg_val(cur_reg_val,
					BITFMASK(VCOM_SET),
					BITFVAL(VCOM_SET, vcom_uV_to_rs(uV)));

			retval = tps6518x_reg_write(REG_TPS65180_VCOM_ADJUST,
					new_reg_val);
			break;
		case 5 : /* TPS65185 */
		case 6 : /* TPS65186 */
#if 1
			//printk("REG_TPS65185_VCOM1 -> writing -> 0x%x, voltage=%d\n",vcom2_uV_to_rs(uV) & 255,uV);
#if 1
			retval = tps6518x_reg_write(REG_TPS65185_VCOM1,
					vcom2_uV_to_rs(uV) & 255);
#else
			retval = tps6518x_reg_write(REG_TPS65185_VCOM1,0x8c);
#endif
			tps6518x_reg_read( REG_TPS65185_VCOM2,&cur_reg_val);
			new_reg_val = to_reg_val(cur_reg_val,
					BITFMASK(VCOM2_SET),
					BITFVAL(VCOM2_SET, vcom2_uV_to_rs(uV)/256));
			//printk("REG_TPS65185_VCOM2 -> writing -> 0x%x\n",new_reg_val);
#if 1
			retval = tps6518x_reg_write(REG_TPS65185_VCOM2,
					new_reg_val);
#else
			retval = tps6518x_reg_write(REG_TPS65185_VCOM2,0x0);
#endif
#else
			flag_prgo=0;
			gpio_set_value(tps6518x->gpio_pmic_wakeup,1);
	//		gpio_set_value(tps6518x->gpio_pmic_powerup,0);
			tmpdata=vcom2_uV_to_rs(uV) & 255;
			if(tmpdata>0xb4)   //0xb4 ==-1800 mv
				tmpdata=tmpdata-10;
			tps6518x_reg_read( REG_TPS65185_VCOM1,&tmpdata1);
		//	printk("wym tps6518x_vcom_set_voltage :%x,w:%x,R:%x\n",uV ,tmpdata,tmpdata1);
			if(tmpdata!=tmpdata1)
			{
			//	printk("wym tps6518x_vcom_set_voltage 1:%x\n",uV );
				retval = tps6518x_reg_write(REG_TPS65185_VCOM1,
					vcom2_uV_to_rs(uV) & 255);
				flag_prgo=1;
			}
			tps6518x_reg_read( REG_TPS65185_VCOM2,&cur_reg_val);
			tmpdata=vcom2_uV_to_rs(uV)/256;
			if(cur_reg_val&0x01!=tmpdata)
			{
		//		printk("wym tps6518x_vcom_set_voltage 2:%x,w:%x,R:%x\n",uV,tmpdata,cur_reg_val );
				new_reg_val = to_reg_val(cur_reg_val,
							BITFMASK(VCOM2_SET),
							BITFVAL(VCOM2_SET, vcom2_uV_to_rs(uV)/256));

				retval = tps6518x_reg_write(REG_TPS65185_VCOM2,new_reg_val);
				flag_prgo=1;
				}
			if(flag_prgo==1)
			{
				tps6518x_reg_read( REG_TPS65185_VCOM2,&tmpdata1);
				retval = tps6518x_reg_write(REG_TPS65185_VCOM2,tmpdata1|0x40);
			//	printk("wym tps6518x_vcom_set_voltage 3:%x\n",tmpdata1 );
				tps6518x_reg_read( REG_TPS65185_ENABLE,&tmpdata1);
				gpio_set_value(tps6518x->gpio_pmic_powerup,0);
			//	printk("wym tps6518x_vcom_set_voltage   REG_TPS65185_ENABLE:%x\n",tmpdata1 );
				tps6518x_reg_write(REG_TPS65185_ENABLE,0);

		//		gpio_set_value(tps6518x->gpio_pmic_wakeup,0);
				tps6518x_reg_read( REG_TPS65185_INT_EN1,&tmpdata1);
			//	printk("wym tps6518x_vcom_set_voltage REG_TPS65185_INT_EN1 :%x\n",tmpdata1 );
				msleep(10);
				flag_prgo=20;
				while(flag_prgo--)
				{
					tps6518x_reg_read( REG_TPS65185_INT1,&tmpdata1);
					if(tmpdata1&0x01==0x01)
					{
					//	printk("wym tps6518x_vcom_set_voltage save vcom ok \n" );
						break;
						}
				//	printk("wym tps6518x_vcom_set_voltage wait ....%x\n",flag_prgo );
					msleep(1);
				}
				gpio_set_value(tps6518x->gpio_pmic_powerup,1);
			}
#endif

			break;
		default :
		retval = -1;
	}
	
	return retval;
}

static int tps6518x_vcom_get_voltage(struct regulator_dev *reg)
{
	struct tps6518x *tps6518x = rdev_get_drvdata(reg);
	unsigned int cur_reg_val; /* current register value */
	unsigned int cur_reg2_val; /* current register value */
	unsigned int cur_fld_val; /* current bitfield value*/
	int vcomValue;
	int gpio;
	
	//printk("tps6518x_vcom_get_voltage\n");

	/*
	 * this will not work on tps65182
	 */
	if (tps6518x->revID == 65182)
		return 0;
	
	switch (tps6518x->revID & 15)
	{
		case 0 : /* TPS65180 */
		case 1 : /* TPS65181 */
		case 4 : /* TPS65180-rev1 */
			tps6518x_reg_read(REG_TPS65180_VCOM_ADJUST, &cur_reg_val);
			cur_fld_val = BITFEXT(cur_reg_val, VCOM_SET);
			vcomValue = vcom_rs_to_uV(cur_fld_val);
			break;
		case 5 : /* TPS65185 */
		case 6 : /* TPS65186 */
			tps6518x_reg_read(REG_TPS65185_VCOM1,&cur_reg_val);
			tps6518x_reg_read(REG_TPS65185_VCOM2,&cur_reg2_val);
			cur_reg_val |= 256 * (1 & cur_reg2_val);
			vcomValue = vcom2_rs_to_uV(cur_reg_val);
			break;
		default:
			vcomValue = 0;
	}
	
	//printk("tps6518x_vcom_get_voltage end, vcomValue=%d\n",vcomValue);
	
	return vcomValue;

}

static int tps6518x_vcom_enable(struct regulator_dev *reg)
{
	struct tps6518x *tps6518x = rdev_get_drvdata(reg);

	unsigned int cur_reg_val; /* current register value */
	int vcomEnable = 0;
	int gpio;
	
	//printk("tps6518x_vcom_enable\n");
	/*
	 * check for the TPS65182 device
	 */
	if (tps6518x->revID == 65182)
	{
		gpio_set_value(tps6518x->gpio_pmic_vcom_ctrl,vcomEnable);
		return 0;
	}

#if 1
	/*
	 * Check to see if we need to set the VCOM voltage.
	 * Should only be done one time. And, we can
	 * only change vcom voltage if we have been enabled.
	 */
	if (!tps6518x->vcom_setup && tps6518x_is_power_good(tps6518x)) {
		tps6518x_vcom_set_voltage(reg,
			tps6518x->vcom_uV,
			tps6518x->vcom_uV,
			NULL);
		tps6518x->vcom_setup = true;
	}

	switch (tps6518x->revID & 15)
	{
		case 0 : /* TPS65180 */
		case 1 : /* TPS65181 */
		case 4 : /* TPS65180-rev1 */
			vcomEnable = 1;
			break;
		case 5 : /* TPS65185 */
		case 6 : /* TPS65186 */
			tps6518x_reg_read(REG_TPS65185_VCOM2,&cur_reg_val);
			// do not enable vcom if HiZ bit is set
			if (cur_reg_val & (1<<VCOM_HiZ_LSH))
				vcomEnable = 0;
			else
				vcomEnable = 1;
			break;
		default:
			vcomEnable = 0;
	}
	//printk("vcom set voltage value=%d\n",vcomEnable);
	gpio_set_value(tps6518x->gpio_pmic_vcom_ctrl,vcomEnable);
#else
	tps6518x_reg_write(REG_TPS65180_ENABLE, 0xbf);
	gpio_set_value(tps6518x->gpio_pmic_vcom_ctrl,1);
#endif	

	return 0;
}

static int tps6518x_vcom_disable(struct regulator_dev *reg)
{
	struct tps6518x *tps6518x = rdev_get_drvdata(reg);
	

	gpio_set_value(tps6518x->gpio_pmic_vcom_ctrl,0);
	msleep(1);
	
	return 0;
}

static int tps6518x_vcom_is_enabled(struct regulator_dev *reg)
{
	struct tps6518x *tps6518x = rdev_get_drvdata(reg);
	

	int gpio = gpio_get_value(tps6518x->gpio_pmic_vcom_ctrl);

	return gpio;	
}


static int tps6518x_is_power_good(struct tps6518x *tps6518x)
{
	/*
	 * XOR of polarity (starting value) and current
	 * value yields whether power is good.
	 */
	int val;
	int read_val;
	//printk("tps6518x_is_power_good\n");

	tps6518x->pwrgood_polarity=1;

	val=gpio_get_value(tps6518x->gpio_pmic_pwrgood);

	tps6518x_reg_read(0x0f, &read_val);
	//printk("read back value = 0x%x\n",read_val);
	//printk("val=%d,pwrgood_polarity=%d\n",val,tps6518x->pwrgood_polarity);

	return ((read_val==0xfa)&&val ^ tps6518x->pwrgood_polarity);

#if 0
	return gpio_get_value(tps6518x->gpio_pmic_pwrgood) ^
		tps6518x->pwrgood_polarity;
#else
	return val ^ tps6518x->pwrgood_polarity;
#endif
}

static int tps6518x_wait_power_good(struct tps6518x *tps6518x)
{
	int i;
	//printk("tps6518x_wait_power_good\n");
	//for (i = 0; i < tps6518x->max_wait * 3; i++) {
	for (i = 0; i < tps6518x->max_wait; i++) {
		if (tps6518x_is_power_good(tps6518x)){
			//printk("tps6518x_wait_power_goodi success\n");
			return 0;
		}

		msleep(1);
	}
	return -ETIMEDOUT;
}

static int tps6518x_wait_power_down(struct tps6518x *tps6518x)
{
	int i;
	//printk("tps6518x_wait_power_dwown\n");
	//for (i = 0; i < tps6518x->max_wait * 3; i++) {
	for (i = 0; i < tps6518x->max_wait; i++) {
		if (!tps6518x_is_power_good(tps6518x)){
			//printk("tps6518x_wait_power_down success\n");
			return 0;
		}

		msleep(10);
	}
	return -ETIMEDOUT;
}

static int tps6518x_display_enable(struct regulator_dev *reg)
{
	struct tps6518x *tps6518x = rdev_get_drvdata(reg);
	unsigned int cur_reg_val; /* current register value to modify */
	unsigned int fld_mask;	  /* register mask for bitfield to modify */
	unsigned int fld_val;	  /* new bitfield value to write */
	unsigned int new_reg_val; /* new register value to write */
	int gpio;
	
	//printk("tps6518x_display_enable\n");
	
	if (tps6518x->revID == 65182)
	{
		epdc_pwr0_enable(reg);
	}
	else
	{

#if 0
		printk("enable display regulators\n");
		/* enable display regulators */
		cur_reg_val = tps65180_current_Enable_Register & 0x3f;
		fld_mask = BITFMASK(VDDH_EN) | BITFMASK(VPOS_EN) |
			BITFMASK(VEE_EN) | BITFMASK(VNEG_EN);
		fld_val = BITFVAL(VDDH_EN, true) | BITFVAL(VPOS_EN, true) |
			BITFVAL(VEE_EN, true) | BITFVAL(VNEG_EN, true) | BITFVAL(VCOM_EN, true);
		printk("tps65180_current_Enable_Register=%d,fld_mask=%d,fld_val=%d\n",tps65180_current_Enable_Register,fld_mask,fld_val);
		new_reg_val = tps65180_current_Enable_Register = to_reg_val(cur_reg_val, fld_mask, fld_val);
		printk("new_reg_val=%d\n",new_reg_val);
		tps6518x_reg_write(REG_TPS65180_ENABLE, new_reg_val);
		
		/* turn on display regulators */
		printk("turn on display regulators\n");
		cur_reg_val = tps65180_current_Enable_Register & 0x3f;
		fld_mask = BITFMASK(ACTIVE);
		fld_val = BITFVAL(ACTIVE, true);
		printk("tps65180_current_Enable_Register=%d,fld_mask=%d,fld_val=%d\n",tps65180_current_Enable_Register,fld_mask,fld_val);
		new_reg_val = tps65180_current_Enable_Register = to_reg_val(cur_reg_val, fld_mask, fld_val);

		//new_reg_val=0xBF;	
		printk("new_reg_val=%d\n",new_reg_val);
		tps6518x_reg_write(REG_TPS65180_ENABLE, new_reg_val);

		//msleep(72);
		//msleep(24);
		msleep(10);
#else
		tps6518x_reg_write(REG_TPS65180_ENABLE, 0x2b);
		msleep(1);
		tps6518x_reg_write(REG_TPS65180_ENABLE, 0x2f);
		//msleep(6);
		tps6518x_reg_write(REG_TPS65180_ENABLE, 0xaf);
		//epdc_pwr0_enable(reg);
		//msleep(24);
#endif
		
		
	}

	return tps6518x_wait_power_good(tps6518x);
}

static int tps6518x_display_disable(struct regulator_dev *reg)
{
	struct tps6518x *tps6518x = rdev_get_drvdata(reg);
	unsigned int cur_reg_val; /* current register value to modify */
	unsigned int fld_mask;	  /* register mask for bitfield to modify */
	unsigned int fld_val;	  /* new bitfield value to write */
	unsigned int new_reg_val; /* new register value to write */
	
	//printk("tps6518x_display_disable\n");

	if (tps6518x->revID == 65182)
	{
		epdc_pwr0_disable(reg);
	}
	else
	{
#if 0
		/* turn off display regulators */
		printk("turn off display regulators\n");
		cur_reg_val = tps65180_current_Enable_Register & 0x3f;
		fld_mask = BITFMASK(VCOM_EN) | BITFMASK(STANDBY);
		fld_val = BITFVAL(VCOM_EN, true) | BITFVAL(STANDBY, true);
		new_reg_val = tps65180_current_Enable_Register = to_reg_val(cur_reg_val, fld_mask, fld_val);
		tps6518x_reg_write(REG_TPS65180_ENABLE, new_reg_val);
#else
		tps6518x_reg_write(REG_TPS65180_ENABLE, 0x40);
#endif		
		
	}

	//msleep(tps6518x->max_wait*3);
	//msleep(tps6518x->max_wait);
	return tps6518x_wait_power_down(tps6518x);

	return 0;
}

static int tps6518x_display_is_enabled(struct regulator_dev *reg)
{
	struct tps6518x *tps6518x = rdev_get_drvdata(reg);
	//printk("tps6518x_display_is_enabled\n");
	int val;

	if (tps6518x->revID == 65182)
		return gpio_get_value(tps6518x->gpio_pmic_wakeup) ? 1:0;
	else{
		//return tps65180_current_Enable_Register & BITFMASK(ACTIVE);
		tps6518x_reg_read(REG_TPS65180_ENABLE, &val);
		return (val & BITFMASK(ACTIVE));
	}
}

/*
 * Regulator operations
 */

static struct regulator_ops tps6518x_display_ops = {
	.enable = tps6518x_display_enable,
	.disable = tps6518x_display_disable,
	.is_enabled = tps6518x_display_is_enabled,
};

static struct regulator_ops tps6518x_vcom_ops = {
	.enable = tps6518x_vcom_enable,
	.disable = tps6518x_vcom_disable,
	.get_voltage = tps6518x_vcom_get_voltage,
	.set_voltage = tps6518x_vcom_set_voltage,
	.is_enabled = tps6518x_vcom_is_enabled,
};

#if 1
static struct regulator_ops tps6518x_v3p3_ops = {
	.enable = tps6518x_v3p3_enable,
	.disable = tps6518x_v3p3_disable,
	.is_enabled = tps6518x_v3p3_is_enabled,
};
#else
static struct regulator_ops tps6518x_v3p3_ops = {
	.enable = epdc_pwr0_enable,
	.disable = epdc_pwr0_disable,
	.is_enabled = epdc_pwr0_is_enabled,
};
#endif

/*
 * Regulator descriptors
 */
static struct regulator_desc tps6518x_reg[TPS6518x_NUM_REGULATORS] = {
{
	.name = "DISPLAY",
	.id = TPS6518x_DISPLAY,
	.ops = &tps6518x_display_ops,
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
},
{
	.name = "VCOM",
	.id = TPS6518x_VCOM,
	.ops = &tps6518x_vcom_ops,
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
},
{
	.name = "V3P3",
	.id = TPS6518x_V3P3,
	.ops = &tps6518x_v3p3_ops,
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
},
};

static void tps6518x_setup_timings(struct tps6518x *tps6518x)
{

	int temp0, temp1, temp2, temp3;
		
	tps6518x_reg_read(REG_TPS65180_REVID,&tps6518x->revID);
	//printk("Revision id=0x%x\n",tps6518x->revID);

	/* read the current setting in the PMIC */
	if ((tps6518x->revID == TPS65180_PASS1) || (tps6518x->revID == TPS65181_PASS1) ||
	   (tps6518x->revID == TPS65180_PASS2) || (tps6518x->revID == TPS65181_PASS2)) {
	   tps6518x_reg_read(REG_TPS65180_PWRSEQ0, &temp0);
	   tps6518x_reg_read(REG_TPS65180_PWRSEQ1, &temp1);
	   tps6518x_reg_read(REG_TPS65180_PWRSEQ2, &temp2);

	   if ((temp0 != tps6518x->pwr_seq0) ||
		(temp1 != tps6518x->pwr_seq1) ||
		(temp2 != tps6518x->pwr_seq2)) {
		tps6518x_reg_write(REG_TPS65180_PWRSEQ0, tps6518x->pwr_seq0);
		tps6518x_reg_write(REG_TPS65180_PWRSEQ1, tps6518x->pwr_seq1);
		tps6518x_reg_write(REG_TPS65180_PWRSEQ2, tps6518x->pwr_seq2);
	    }
	}
#if 1
#if 1
	tps6518x->upseq0=0xe1;	
	//tps6518x->upseq1=0xff;	
	tps6518x->upseq1=0x55;	
	tps6518x->dwnseq0=0x1e;	
	//tps6518x->dwnseq1=0x00;	
	tps6518x->dwnseq1=0xe0;	
	//tps6518x->dwnseq1=0x55;	
#else
	tps6518x->upseq0=0xe4;	
	tps6518x->upseq1=0x55;	
	tps6518x->dwnseq0=0x1e;	
	tps6518x->dwnseq1=0xe0;	
#endif
#endif
	//printk("Revision id=0x%x\n",tps6518x->revID);

	if ((tps6518x->revID == TPS65185_PASS0) ||
		 (tps6518x->revID == TPS65186_PASS0) ||
		 (tps6518x->revID == TPS65185_PASS1) ||
		 (tps6518x->revID == TPS65186_PASS1) ||
		 (tps6518x->revID == TPS65185_PASS2) ||
		 (tps6518x->revID == TPS65186_PASS2)) {
	   tps6518x_reg_read(REG_TPS65185_UPSEQ0, &temp0);
	   tps6518x_reg_read(REG_TPS65185_UPSEQ1, &temp1);
	   tps6518x_reg_read(REG_TPS65185_DWNSEQ0, &temp2);
	   tps6518x_reg_read(REG_TPS65185_DWNSEQ1, &temp3);
//printk("temp0=0x%x,temp1=0x%x,temp2=0x%x,temp3=0x%x\n",temp0,temp1,temp2,temp3);
	   if ((temp0 != tps6518x->upseq0) ||
		(temp1 != tps6518x->upseq1) ||
		(temp2 != tps6518x->dwnseq0) ||
		(temp3 != tps6518x->dwnseq1)) {
//printk("setting the sequence vaues\n");
		tps6518x_reg_write(REG_TPS65185_UPSEQ0, tps6518x->upseq0);
		tps6518x_reg_write(REG_TPS65185_UPSEQ1, tps6518x->upseq1);
		tps6518x_reg_write(REG_TPS65185_DWNSEQ0, tps6518x->dwnseq0);
		tps6518x_reg_write(REG_TPS65185_DWNSEQ1, tps6518x->dwnseq1);
printk("set: upseq0=0x%x,upseq1=0x%x,downseq0=0x%x,downseq1=0x%x\n",tps6518x->upseq0,tps6518x->upseq0,tps6518x->dwnseq0,tps6518x->dwnseq1);
	    }
	}
		tps6518x_reg_write(REG_TPS65185_UPSEQ0, tps6518x->upseq0);
		tps6518x_reg_write(REG_TPS65185_UPSEQ1, tps6518x->upseq1);
		tps6518x_reg_write(REG_TPS65185_DWNSEQ0, tps6518x->dwnseq0);
		tps6518x_reg_write(REG_TPS65185_DWNSEQ1, tps6518x->dwnseq1);
printk("set: upseq0=0x%x,upseq1=0x%x,downseq0=0x%x,downseq1=0x%x\n",tps6518x->upseq0,tps6518x->upseq0,tps6518x->dwnseq0,tps6518x->dwnseq1);
}

#define CHECK_PROPERTY_ERROR_KFREE(prop) \
do { \
	int ret = of_property_read_u32(tps6518x->dev->of_node, \
					#prop, &tps6518x->prop); \
	if (ret < 0) { \
		return ret;	\
	}	\
} while (0);

#ifdef CONFIG_OF
static int tps6518x_pmic_dt_parse_pdata(struct platform_device *pdev,
					struct tps6518x_platform_data *pdata)
{
	struct tps6518x *tps6518x = dev_get_drvdata(pdev->dev.parent);
	struct device_node *pmic_np, *regulators_np, *reg_np;
	struct tps6518x_regulator_data *rdata;
	int i, ret;

	pmic_np = of_node_get(tps6518x->dev->of_node);
	if (!pmic_np) {
		dev_err(&pdev->dev, "could not find pmic sub-node\n");
		return -ENODEV;
	}

	regulators_np = of_find_node_by_name(pmic_np, "regulators");
	if (!regulators_np) {
		dev_err(&pdev->dev, "could not find regulators sub-node\n");
		return -EINVAL;
	}

	pdata->num_regulators = of_get_child_count(regulators_np);
	dev_dbg(&pdev->dev, "num_regulators %d\n", pdata->num_regulators);

	rdata = devm_kzalloc(&pdev->dev, sizeof(*rdata) *
				pdata->num_regulators, GFP_KERNEL);
	if (!rdata) {
		of_node_put(regulators_np);
		dev_err(&pdev->dev, "could not allocate memory for"
			"regulator data\n");
		return -ENOMEM;
	}

	pdata->regulators = rdata;
	for_each_child_of_node(regulators_np, reg_np) {
		for (i = 0; i < ARRAY_SIZE(tps6518x_reg); i++)
			if (!of_node_cmp(reg_np->name, tps6518x_reg[i].name))
				break;

		if (i == ARRAY_SIZE(tps6518x_reg)) {
			dev_warn(&pdev->dev, "don't know how to configure"
				"regulator %s\n", reg_np->name);
			continue;
		}

		rdata->id = i;
		rdata->initdata = of_get_regulator_init_data(&pdev->dev,
							     reg_np,&tps6518x_reg[i]);
		rdata->reg_node = reg_np;
		rdata++;
	}
	of_node_put(regulators_np);

	tps6518x->max_wait = (6 + 6 + 6 + 6);
	//tps6518x->max_wait = (3 + 3 + 3 + 3);
	//tps6518x->max_wait = 140;

	tps6518x->gpio_pmic_wakeup = of_get_named_gpio(pmic_np,
					"gpio_pmic_wakeup", 0);
	if (!gpio_is_valid(tps6518x->gpio_pmic_wakeup)) {
		dev_err(&pdev->dev, "no epdc pmic wakeup pin available\n");
		goto err;
	}
	ret = devm_gpio_request_one(&pdev->dev, tps6518x->gpio_pmic_wakeup,
				GPIOF_OUT_INIT_HIGH, "epdc-pmic-wake");
	if (ret < 0)
		goto err;

	tps6518x->gpio_pmic_vcom_ctrl = of_get_named_gpio(pmic_np,
					"gpio_pmic_vcom_ctrl", 0);
	if (!gpio_is_valid(tps6518x->gpio_pmic_vcom_ctrl)) {
		dev_err(&pdev->dev, "no epdc pmic vcom_ctrl pin available\n");
		goto err;
	}
	ret = devm_gpio_request_one(&pdev->dev, tps6518x->gpio_pmic_vcom_ctrl,
				GPIOF_OUT_INIT_LOW, "epdc-vcom");
	if (ret < 0)
		goto err;
	
#if 0
	tps6518x->gpio_pmic_v3p3_ctrl = of_get_named_gpio(pmic_np,
					"gpio_pmic_v3p3_ctrl", 0);
	if (!gpio_is_valid(tps6518x->gpio_pmic_v3p3_ctrl)) {
		dev_err(&pdev->dev, "no epdc pmic v3p3_ctrl pin available\n");
		goto err;
	}
	ret = devm_gpio_request_one(&pdev->dev, tps6518x->gpio_pmic_v3p3_ctrl,
				GPIOF_OUT_INIT_LOW, "epdc-v3p3");
	if (ret < 0)
		goto err;
#endif

	tps6518x->gpio_pmic_powerup = of_get_named_gpio(pmic_np,
					"gpio_pmic_powerup", 0);
	if (!gpio_is_valid(tps6518x->gpio_pmic_powerup)) {
		dev_err(&pdev->dev, "no epdc pmic powerup pin available\n");
		goto err;
	}
	ret = devm_gpio_request_one(&pdev->dev, tps6518x->gpio_pmic_powerup,
				GPIOF_OUT_INIT_LOW, "epdc-powerup");
	if (ret < 0)
		goto err;

	tps6518x->gpio_pmic_intr = of_get_named_gpio(pmic_np,
					"gpio_pmic_intr", 0);
	if (!gpio_is_valid(tps6518x->gpio_pmic_intr)) {
		dev_err(&pdev->dev, "no epdc pmic intr pin available\n");
		goto err;
	}
	ret = devm_gpio_request_one(&pdev->dev, tps6518x->gpio_pmic_intr,
				GPIOF_IN, "epdc-pmic-int");
	if (ret < 0)
		goto err;

	tps6518x->gpio_pmic_pwrgood = of_get_named_gpio(pmic_np,
					"gpio_pmic_pwrgood", 0);
	if (!gpio_is_valid(tps6518x->gpio_pmic_pwrgood)) {
		dev_err(&pdev->dev, "no epdc pmic pwrgood pin available\n");
		goto err;
	}
	ret = devm_gpio_request_one(&pdev->dev, tps6518x->gpio_pmic_pwrgood,
				GPIOF_IN, "epdc-pwrstat");
	if (ret < 0)
		goto err;

	tps6518x->gpio_epdc_bdr0 = of_get_named_gpio(pmic_np,
					"gpio_epdc_brd0", 0);
	if (!gpio_is_valid(tps6518x->gpio_epdc_bdr0)) {
		dev_err(&pdev->dev, "no epdc bdr0 pin available\n");
		goto err;
	}
	ret = devm_gpio_request_one(&pdev->dev, tps6518x->gpio_epdc_bdr0,
				GPIOF_OUT_INIT_LOW, "epdc-bdr0");
	if (ret < 0)
		goto err;

	tps6518x->gpio_epdc_bdr1 = of_get_named_gpio(pmic_np,
					"gpio_epdc_brd1", 0);
	if (!gpio_is_valid(tps6518x->gpio_epdc_bdr1)) {
		dev_err(&pdev->dev, "no epdc bdr1 pin available\n");
		goto err;
	}
	ret = devm_gpio_request_one(&pdev->dev, tps6518x->gpio_epdc_bdr1,
				GPIOF_OUT_INIT_LOW, "epdc-bdr1");
	if (ret < 0)
		goto err;
err:
	return 0;

}
#else
static int tps6518x_pmic_dt_parse_pdata(struct platform_device *pdev,
					struct tps6518x *tps6518x)
{
	return 0;
}
#endif	/* !CONFIG_OF */

/*
 * Regulator init/probing/exit functions
 */
static int tps6518x_regulator_probe(struct platform_device *pdev)
{
	struct tps6518x *tps6518x = dev_get_drvdata(pdev->dev.parent);
	struct tps6518x_platform_data *pdata = tps6518x->pdata;
	struct tps6518x_data *priv;
	struct regulator_dev **rdev;
	struct regulator_config config = { };
	int size, i, ret = 0;


        printk("tps6518x_regulator_probe starting\n");

	
	if (tps6518x->dev->of_node) {
		ret = tps6518x_pmic_dt_parse_pdata(pdev, pdata);
		printk("--->tps6518x_pmic_dt_parse_pdata , ret=%d\n",ret);
		if (ret)
			return ret;
	}
	priv = devm_kzalloc(&pdev->dev, sizeof(struct tps6518x_data),
			       GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	size = sizeof(struct regulator_dev *) * pdata->num_regulators;
	priv->rdev = devm_kzalloc(&pdev->dev, size, GFP_KERNEL);
	if (!priv->rdev)
		return -ENOMEM;

	rdev = priv->rdev;
	priv->num_regulators = pdata->num_regulators;
	platform_set_drvdata(pdev, priv);

	tps6518x->vcom_setup = false;
	tps6518x->pass_num = tps6518x_pass_num;
	tps6518x->vcom_uV = tps6518x_vcom;

	for (i = 0; i < pdata->num_regulators; i++) {
		int id = pdata->regulators[i].id;

		printk("regulator number=%d, nb regulator=%d\n",i,pdata->num_regulators);

		config.dev = tps6518x->dev;
		config.init_data = pdata->regulators[i].initdata;
		config.driver_data = tps6518x;
		config.of_node = pdata->regulators[i].reg_node;

		rdev[i] = regulator_register(&tps6518x_reg[id], &config);
		if (IS_ERR(rdev[i])) {
			ret = PTR_ERR(rdev[i]);
			dev_err(&pdev->dev, "regulator init failed for %d\n",
					id);
			rdev[i] = NULL;
			goto err;
		}
	}

	/*
	 * Set up PMIC timing values.
	 * Should only be done one time!  Timing values may only be
	 * changed a limited number of times according to spec.
	 */

	tps6518x_setup_timings(tps6518x);

	tps6518x_detect1();
	

    	printk("tps6518x_regulator_probe success\n");
	return 0;
err:
	while (--i >= 0)
		regulator_unregister(rdev[i]);
	return ret;
}

static int tps6518x_regulator_remove(struct platform_device *pdev)
{
	struct tps6518x_data *priv = platform_get_drvdata(pdev);
	struct regulator_dev **rdev = priv->rdev;
	int i;

	printk("tps6518x_regulator_remove\n");

	for (i = 0; i < priv->num_regulators; i++)
		regulator_unregister(rdev[i]);
	return 0;
}

static const struct platform_device_id tps6518x_pmic_id[] = {
	{ "tps6518x-pmic", 0},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(platform, tps6518x_pmic_id);

static struct platform_driver tps6518x_regulator_driver = {
	.probe = tps6518x_regulator_probe,
	.remove = tps6518x_regulator_remove,
	.id_table = tps6518x_pmic_id,
	.driver = {
		.name = "tps6518x-pmic",
	},
};

static int __init tps6518x_regulator_init(void)
{
	return platform_driver_register(&tps6518x_regulator_driver);
}
subsys_initcall_sync(tps6518x_regulator_init);

static void __exit tps6518x_regulator_exit(void)
{
	platform_driver_unregister(&tps6518x_regulator_driver);
}
module_exit(tps6518x_regulator_exit);


/*
* Parse user specified options (`tps6518x:')
* example:
*   tps6518x:pass=2,vcom=-1250000
*/
static int __init tps6518x_setup(char *options)
{
	int ret;
	char *opt;
	unsigned long ulResult;
	while ((opt = strsep(&options, ",")) != NULL) {
		if (!*opt)
			continue;
		if (!strncmp(opt, "pass=", 5)) {
			ret = kstrtoul((const char *)(opt + 5), 0, &ulResult);
			tps6518x_pass_num = ulResult;
			if (ret < 0)
				return ret;
		}
		if (!strncmp(opt, "vcom=", 5)) {
			int offs = 5;
			if (opt[5] == '-')
				offs = 6;
			ret = kstrtoul((const char *)(opt + offs), 0, &ulResult);
			tps6518x_vcom = (int) ulResult;
			if (ret < 0)
				return ret;
			tps6518x_vcom = -tps6518x_vcom;
		}
	}

	return 1;
}

__setup("tps6518x:", tps6518x_setup);

static int __init tps65182_setup(char *options)
{
	int ret;
	char *opt;
	unsigned long ulResult;
	while ((opt = strsep(&options, ",")) != NULL) {
		if (!*opt)
			continue;
		if (!strncmp(opt, "pass=", 5)) {
			ret = kstrtoul((const char *)(opt + 5), 0, &ulResult);
			tps6518x_pass_num = ulResult;
			if (ret < 0)
				return ret;
		}
		if (!strncmp(opt, "vcom=", 5)) {
			int offs = 5;
			if (opt[5] == '-')
				offs = 6;
			ret = kstrtoul((const char *)(opt + offs), 0, &ulResult);
			tps6518x_vcom = (int) ulResult;
			if (ret < 0)
				return ret;
			tps6518x_vcom = -tps6518x_vcom;
		}
	}

	return 1;
}

__setup("tps65182:", tps65182_setup);

/* Module information */
MODULE_DESCRIPTION("TPS6518x regulator driver");
MODULE_LICENSE("GPL");
