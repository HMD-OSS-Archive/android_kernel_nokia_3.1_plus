/*
 * fih_karkit.c
 *
 * Copyright (C) 2015 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/ctype.h>
#include <linux/device.h>

#include <linux/err.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/types.h>

#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include "fih_rfcable_class.h"

struct karkit_dr_st {
	struct device		*dev;
	char			*name;
	int			*debug_mask;
	int			kk_enable_pin;
	int			irq;
	int			fake_status;
	struct fih_rfcable_phy_instance	*rfc_instance;
	struct delayed_work 	check_gpio_work;
};

struct karkit_dev_st {
	struct karkit_dr_st	kk;
	struct dentry		*dfs_root;
};

//__debug_mask: 0 = disable,, 1 = enable,, 2 = create work queue
// echo 2 > sys/module/fih_karkit/parameters/debug_mask
// cat /sys/class/rfcable_det/rfcable/uevent
static int __debug_mask =0x0;
module_param_named(
	debug_mask, __debug_mask, int, 0600
);


/****************  Property  **********************/
static enum fih_rfcable_property kk_dr_properties[] = {
	FIH_RFCABLE_PROP_STATUS,
	FIH_RFCABLE_PROP_FAKE_STATUS,
};

static int kk_dr_prop_writeable(struct fih_rfcable_phy_instance *dual_role,
					enum fih_rfcable_property prop)
{
	int rc;

	switch (prop) {
	case FIH_RFCABLE_PROP_FAKE_STATUS:
		rc = 1;
		break;
	default:
		rc = 0;
	}
	return rc;
}

static int kk_dr_set_property(struct fih_rfcable_phy_instance *rfc_instance,
					enum fih_rfcable_property prop,
					const unsigned int *val)
{
	int rc = 0;	
	struct karkit_dr_st *kk = fih_rfcable_get_drvdata(rfc_instance);

	if (!kk)
		return -EINVAL;

	switch (prop) {
	case FIH_RFCABLE_PROP_FAKE_STATUS:
		kk->fake_status = *val;
		if (kk->rfc_instance)
			fih_rfcable_instance_changed(kk->rfc_instance);
		break;
	default:
		pr_debug("Invalid kk request %d\n", prop);
		rc = -EINVAL;
	}

	return rc;
}

static int kk_dr_get_property(struct fih_rfcable_phy_instance *rfc_instance,
					enum fih_rfcable_property prop,
					unsigned int *val)
{
	struct karkit_dr_st *kk = fih_rfcable_get_drvdata(rfc_instance);
	int kk_gpio =0;

	if (!kk)
		return -EINVAL;
		
	kk_gpio = gpio_get_value((kk->kk_enable_pin));

	switch (prop) {
	case FIH_RFCABLE_PROP_STATUS:
		if(kk->fake_status != 0x3)
			*val = kk->fake_status;
		else
			*val = kk_gpio;
		break;
	case FIH_RFCABLE_PROP_FAKE_STATUS:
		if(*kk->debug_mask == 2)
		{
			cancel_delayed_work_sync(&kk->check_gpio_work);
			schedule_delayed_work(&kk->check_gpio_work, msecs_to_jiffies(10000));
			*kk->debug_mask = 1;
		}
		*val = kk->fake_status;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static const struct fih_rfcable_phy_desc dr_desc = {
	.name = "rfcable",
	.properties = kk_dr_properties,
	.num_properties = ARRAY_SIZE(kk_dr_properties),
	.get_property = kk_dr_get_property,
	.set_property = kk_dr_set_property,
	.property_is_writeable = kk_dr_prop_writeable,
};

/****************  driver  **********************/
static void fih_check_gpio_work(struct work_struct *work)
{
	struct karkit_dr_st *kk = container_of(work, struct karkit_dr_st,
							check_gpio_work.work);
	int ikk_det =0;
	ikk_det = gpio_get_value((kk->kk_enable_pin));

	if(*kk->debug_mask == 1)
		pr_err("[%s]  kk gpio status =%d\n", __func__, ikk_det);
	schedule_delayed_work(&kk->check_gpio_work, msecs_to_jiffies(10000));
}


static irqreturn_t kk_detection_handler(int irq, void *_chip)
{
	struct karkit_dr_st *kk = _chip;
	int ikk_det =0;
	
	if(*kk->debug_mask == 1)
	{
		ikk_det = gpio_get_value((kk->kk_enable_pin));
		pr_err("[%s]  kk gpio status =%d\n", __func__, ikk_det);
	}
	
	if (kk->rfc_instance)
		fih_rfcable_instance_changed(kk->rfc_instance);

	return IRQ_HANDLED;
}

static int kk_parse_dt(struct karkit_dev_st *kk_dev)
{
	struct karkit_dr_st *kk = &kk_dev->kk;
	struct device_node *node = kk->dev->of_node;
	int rc=0;
	int ikk_det =0;

	if (!node) {
		pr_err("device tree node missing\n");
		return -EINVAL;
	}
	kk->kk_enable_pin = of_get_named_gpio(node,
		"qcom,karkit-enable-pin", 0);
	if (!gpio_is_valid(kk->kk_enable_pin))
	{
		pr_info("%s: kk_enable_pin gpio not specified\n", __func__);
		return -EINVAL;
	}
	else {
		rc = gpio_request(kk->kk_enable_pin, "karkit_status");
		if (rc)
			pr_err("request kk_enable_pin, rc=%d\n", rc);
		else
		{
			gpio_direction_input(kk->kk_enable_pin);
			ikk_det = gpio_get_value((kk->kk_enable_pin));
			pr_err("[%s] kk_enable_pin status =%d\n", __func__, ikk_det);
		}
	}
	
	return rc;
}


static int karkit_dr_probe(struct platform_device *pdev)
{
	struct karkit_dev_st *kk_dev;
	struct karkit_dr_st *kk;
	int rc = 0;
	unsigned long flags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING
							| IRQF_ONESHOT;
	
	kk_dev = devm_kzalloc(&pdev->dev, sizeof(*kk_dev), GFP_KERNEL);
	if (!kk_dev)
		return -ENOMEM;

	kk = &kk_dev->kk;
	kk->dev = &pdev->dev;
	kk->debug_mask = &__debug_mask;
	kk->fake_status = 0x3;

	rc = kk_parse_dt(kk_dev);
	if (rc < 0) {
		pr_err("Couldn't parse device tree rc=%d\n", rc);
		return rc;
	}

	/* set driver data before resources request it */
	platform_set_drvdata(pdev, kk_dev);

	if (gpio_is_valid(kk->kk_enable_pin))
	{
		kk->irq = gpio_to_irq(kk->kk_enable_pin);
		rc = devm_request_threaded_irq(kk->dev,
			kk->irq, NULL, kk_detection_handler, flags, "kk_det",
			kk);
		if(rc < 0)
		{
			rc = request_threaded_irq(kk->irq, NULL, kk_detection_handler,
				   flags, "kk_det", kk);
				pr_err("[%s] request_threaded_irq, rc=%d\n", __func__, rc);

		}
	}

	INIT_DELAYED_WORK(&kk->check_gpio_work, fih_check_gpio_work);

	kk->rfc_instance = devm_fih_rfcable_instance_register(kk->dev,	&dr_desc);
	if (IS_ERR(kk->rfc_instance)) {
		pr_err("Couldn't register rfc_instance\n");
		rc = PTR_ERR(kk->rfc_instance);
	} else {
		kk->rfc_instance->drv_data = kk;
	}

	pr_err("[%s] end, rc =%d \n",__func__, rc);
	return rc;
};

static int karkit_dr_remove(struct platform_device *pdev)
{
	struct karkit_dev_st *kk_dev= platform_get_drvdata(pdev);
	struct karkit_dr_st *kk = &kk_dev->kk;
	cancel_delayed_work_sync(&kk->check_gpio_work);
	
	pr_info("[%s] end\n",__func__);
	return 0;
}

static void karkit_dr_shutdown(struct platform_device *pdev)
{
	pr_info("[%s] end\n",__func__);
}

static const struct of_device_id match_table[] = {
	{ .compatible = "fih,karkit", },
	{ },
};

static struct platform_driver karkit_driver = {
	.driver		= {
		.name		= "fih,karkit-dr",
		.owner		= THIS_MODULE,
		.of_match_table	= match_table,
	},
	.probe		= karkit_dr_probe,
	.remove		= karkit_dr_remove,
	.shutdown	= karkit_dr_shutdown,
};

module_platform_driver(karkit_driver);

MODULE_DESCRIPTION("FIH karkit_driver Driver");
MODULE_LICENSE("GPL v2");
