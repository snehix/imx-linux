/*
 * Functions to access SY3686A power management chip temperature
 *
 * Copyright (C) 2019 reMarkable AS - http://www.remarkable.com/
 *
 * Author: Lars Ivar Miljeteig <lars.ivar.miljeteig@remarkable.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>
#include <linux/regulator/consumer.h>

#include <linux/mfd/sy7636a.h>

struct sy7636a_data {
	struct sy7636a *sy7636a;
	struct thermal_zone_device *thermal_zone_dev;
	struct regulator *regulator;
};

static int sy7636a_get_temp(void *arg, int *res)
{
	unsigned int reg_val;
	int ret;
	struct sy7636a_data *data = arg;

	ret = regulator_enable(data->regulator);
	if (ret)
		return ret;

	ret = regmap_read(data->sy7636a->regmap,
			SY7636A_REG_TERMISTOR_READOUT, &reg_val);

	if (!ret) {
		*res = *((signed char*)&reg_val);
		*res *= 1000;
	}

	regulator_disable(data->regulator);

	return ret;
}

static const struct thermal_zone_of_device_ops ops = {
	.get_temp	= sy7636a_get_temp,
};

static int sy7636a_thermal_probe(struct platform_device *pdev)
{
	struct sy7636a *sy7636a = dev_get_drvdata(pdev->dev.parent);
	struct sy7636a_data *data;

	if (!sy7636a)
		return -EPROBE_DEFER;

	data = devm_kzalloc(&pdev->dev, sizeof(struct sy7636a_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	platform_set_drvdata(pdev, data);

	data->regulator = devm_regulator_get(&pdev->dev, "VCOM");
	if (IS_ERR(data->regulator)) {
		dev_err(&pdev->dev,
				"Unable to get vcom regulator, returned %ld\n",
				PTR_ERR(data->regulator));
		return PTR_ERR(data->regulator);
	}

	data->sy7636a = sy7636a;
	data->thermal_zone_dev = devm_thermal_zone_of_sensor_register(
			pdev->dev.parent,
			0,
			data,
			&ops);

	return PTR_ERR_OR_ZERO(data->thermal_zone_dev);
}

static const struct platform_device_id sy7636a_thermal_id_table[] = {
	{ "sy7636a-thermal", },
};
MODULE_DEVICE_TABLE(platform, sy7636a_thermal_id_table);

static struct platform_driver sy7636a_thermal_driver = {
	.driver = {
		.name = "sy7636a-thermal",
	},
	.probe = sy7636a_thermal_probe,
	.id_table = sy7636a_thermal_id_table,
};
module_platform_driver(sy7636a_thermal_driver);

MODULE_AUTHOR("Lars Ivar Miljeteig <lars.ivar.miljeteig@remarkable.com>");
MODULE_DESCRIPTION("SY7636A thermal driver");
MODULE_LICENSE("GPL v2");
