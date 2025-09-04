#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/wakelock.h>
#include <linux/regulator/consumer.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include "fpc_irq.h"

#ifdef CONFIG_MTK_CLKMGR
#include "mach/mt_clkmgr.h"
#else
#include <linux/clk.h>
#endif

#define FPC_RESET_LOW_US 5000
#define FPC_RESET_HIGH1_US 100
#define FPC_RESET_HIGH2_US 5000

#define FPC_TTW_HOLD_TIME 1000
#define SUPPLY_1V8	1800000UL
#define SUPPLY_3V3	3300000UL
#define SUPPLY_TX_MIN	SUPPLY_3V3
#define SUPPLY_TX_MAX	SUPPLY_3V3

#define GPIO_PIN_IRQ   3
static DEFINE_MUTEX(spidev_set_gpio_mutex);
struct fpc_data *g_fpsensor = NULL;

#define     FPSENSOR_RST_PIN      1  // not gpio, only macro,not need modified!!
#define     FPSENSOR_SPI_CS_PIN   2  // not gpio, only macro,not need modified!!     
#define     FPSENSOR_SPI_MO_PIN   3  // not gpio, only macro,not need modified!!   
#define     FPSENSOR_SPI_MI_PIN   4  // not gpio, only macro,not need modified!!   
#define     FPSENSOR_SPI_CK_PIN   5  // not gpio, only macro,not need modified!!  

//add by yanqiyang start
void fpsensor_gpio_output_dts(int gpio, int level)
{
    mutex_lock(&spidev_set_gpio_mutex);
    printk("[fpsensor]fpsensor_gpio_output_dts: gpio= %d, level = %d\n", gpio, level);
    if (gpio == FPSENSOR_RST_PIN) {
        if (level) {
            pinctrl_select_state(g_fpsensor->pinctrl1, g_fpsensor->fp_rst_high);
        } else {
            pinctrl_select_state(g_fpsensor->pinctrl1, g_fpsensor->fp_rst_low);
        }
    }
    mutex_unlock(&spidev_set_gpio_mutex);
}

int fpsensor_gpio_wirte(int gpio, int value)
{
    fpsensor_gpio_output_dts(gpio, value);
    return 0;
}

int fpsensor_gpio_read(int gpio)
{
    return gpio_get_value(gpio);
}

int fpsensor_spidev_dts_init(struct fpc_data *fpsensor)
{
    struct device_node *node = NULL;
	struct platform_device *pdev = NULL;
    int ret = 0;
	
    printk( "%s\n", __func__);

	node = of_find_compatible_node(NULL, NULL, "mediatek,fih-fp"/*"mediatek,fingerprint"*/);

	if (node) {
			pdev = of_find_device_by_node(node);
			if (pdev) {
				fpsensor->pinctrl1 = devm_pinctrl_get(&pdev->dev);
				if (IS_ERR(fpsensor->pinctrl1)) {
					ret = PTR_ERR(fpsensor->pinctrl1);
					printk("%s can't find fingerprint pinctrl\n", __func__);
					return ret;
				}
			} else {
				printk("%s platform device is null\n", __func__);
			}
		} else {
			printk("%s device node is null\n", __func__);
	}

	
        fpsensor->fp_rst_low = pinctrl_lookup_state(fpsensor->pinctrl1, "reset_low");
        if (IS_ERR(fpsensor->fp_rst_low)) {
            ret = PTR_ERR(fpsensor->fp_rst_low);
            printk("fpensor Cannot find fp pinctrl fpsensor_finger_rst_low!\n");
            return ret;
        }
        fpsensor->fp_rst_high = pinctrl_lookup_state(fpsensor->pinctrl1, "reset_high");
        if (IS_ERR(fpsensor->fp_rst_high)) {
            ret = PTR_ERR(fpsensor->fp_rst_high);
            printk( "fpsensor Cannot find fp pinctrl fpsensor_finger_rst_high!\n");
            return ret;
        }
/*
        fpsensor->eint_as_int = pinctrl_lookup_state(fpsensor->pinctrl1,"fingerprint_irq"); 
        if (IS_ERR(fpsensor->eint_as_int)) {
            ret = PTR_ERR(fpsensor->eint_as_int);
            printk( "fpsensor Cannot find fp pinctrl fpsensor_eint!\n");
            return ret;
        }
		
        fpsensor->eint_in_low = pinctrl_lookup_state(fpsensor->pinctrl1, "fingerprint_irq");
        if (IS_ERR(fpsensor->eint_as_int)) {
            ret = PTR_ERR(fpsensor->eint_as_int);
            printk("fpsensor Cannot find fp pinctrl fpsensor_eint_in_low!\n");
            return ret;
        }
		
        fpsensor->eint_in_float = pinctrl_lookup_state(fpsensor->pinctrl1, "fingerprint_irq");
        if (IS_ERR(fpsensor->eint_in_float)) {
            ret = PTR_ERR(fpsensor->eint_in_float);
            printk(" Cannot find fp pinctrl eint_in_float!\n");
            return ret;
		}
*/
    return 0;
}

static void fpsensor_irq_gpio_cfg(void)
{
    struct fpc_data *fpsensor;
	struct device_node *node, *node_eint;
    printk("%s\n", __func__);

    fpsensor = g_fpsensor;
	pinctrl_select_state(fpsensor->pinctrl1, fpsensor->eint_as_int);
#if 0
        fpsensor->irq_gpio = GPIO_PIN_IRQ;

        fpsensor->irq = gpio_to_irq(fpsensor->irq_gpio);  // get irq number
        if (!fpsensor->irq) {
            printk("fpsensor irq_of_parse_and_map fail!!\n");
            return ;
        }
#endif

	/*
	
	&fpc_interrupt {
			interrupt-parent = <&pio>;
			interrupts = <3 IRQ_TYPE_EDGE_RISING 3 0>;
			status = "okay";
	};
	*/

	/*
		fpsensor_fp_eint: fpsensor_fp_eint {
	compatible = "mediatek,fpsensor_fp_eint";
	int-gpios = <&pio 3 0x0>;
    };
	*/
    node = of_find_compatible_node(NULL, NULL, "mediatek,fih-fp"/*"mediatek,fingerprint"*/);
    if ( node) {
#if 0
        of_property_read_u32_array( node, "debounce", ints, ARRAY_SIZE(ints));
        // gpio_request(ints[0], "fpsensor-irq");
        // gpio_set_debounce(ints[0], ints[1]);
        fpsensor_printk("[fpsensor]ints[0] = %d,is irq_gpio , ints[1] = %d!!\n", ints[0], ints[1]);
        fpsensor->irq_gpio = ints[0];
#else
        //fpsensor->irq_gpio = GPIO_PIN_IRQ;
        node_eint = of_find_compatible_node(NULL, NULL, "mediatek,fpsensor_fp_eint");
		if (node_eint == NULL) {
			printk("%s: ====cannot find node_eint====\n",__func__);
			fpsensor->irq_gpio = GPIO_PIN_IRQ;
		}
		fpsensor->irq_gpio = of_get_named_gpio(node_eint, "int-gpios",0);
		printk("%s: ====irq_gpio: %d====\n",__func__,fpsensor->irq_gpio);
#endif
        fpsensor->irq = irq_of_parse_and_map(node, 0);  // get irq number
        if (!fpsensor->irq) {
            printk("fpsensor irq_of_parse_and_map fail!!\n");
            return ;
        }
        printk(" [fpsensor]fpsensor->irq= %d,fpsensor>irq_gpio = %d\n", fpsensor->irq,
                        fpsensor->irq_gpio);
    } else {
        printk("fpsensor null irq node!!\n");
        return ;
    }
       printk(" [fpsensor]fpsensor->irq= %d,fpsensor>irq_gpio = %d\n", fpsensor->irq,
                        fpsensor->irq_gpio);
}

 static void fpsensor_hw_power_enable(u8 onoff)
 {
     static int enable = 1;
     if (onoff && enable)
     {
         //pinctrl_select_state(g_fpsensor->pinctrl1, g_fpsensor->pins_power_on);
	printk("yqy--------power enable on!\n");
         enable = 0;
     }
     else if (!onoff && !enable)
     {
         //pinctrl_select_state(g_fpsensor->pinctrl1, g_fpsensor->pins_power_off);
	printk("yqy--------power enable off!\n");
         enable = 1;
     }
 }

static void fpsensor_spi_clk_enable(u8 bonoff)
{
	static int count;
	printk("yqy------test-------%d, %s\n", __LINE__, __func__);
#ifdef CONFIG_MTK_CLKMGR
	if (bonoff)
		enable_clock(MT_CG_PERI_SPI0, "spi");
	else
		disable_clock(MT_CG_PERI_SPI0, "spi");

#else
	if (bonoff && (count == 0)) {
		 mt_spi_enable_master_clk(g_fpsensor->pldev);
		count = 1;
	} else if ((count > 0) && (bonoff == 0)) {
		mt_spi_disable_master_clk(g_fpsensor->pldev);
		count = 0;
	}
#endif
}
//add by yanqiyang end
static int hw_reset(struct  fpc_data *fpc)
{
	int irq_gpio;
	struct device *dev = fpc->dev;

	//add by yanqiyang start
	//fpc->hwabs->set_val(fpc->rst_gpio, 1);
	fpsensor_gpio_wirte(FPSENSOR_RST_PIN,	1);
	usleep_range(FPC_RESET_HIGH1_US, FPC_RESET_HIGH1_US + 100);
	//printk("%s: RST_PIN status: %d\n",__func__,fpsensor_gpio_read(FPSENSOR_RST_PIN));

	//fpc->hwabs->set_val(fpc->rst_gpio, 0);
	fpsensor_gpio_wirte(FPSENSOR_RST_PIN,  0);
	usleep_range(FPC_RESET_LOW_US, FPC_RESET_LOW_US + 100);
	//printk("%s: RST_PIN status: %d\n",__func__,fpsensor_gpio_read(FPSENSOR_RST_PIN));

	//fpc->hwabs->set_val(fpc->rst_gpio, 1);
	fpsensor_gpio_wirte(FPSENSOR_RST_PIN,  1);
	usleep_range(FPC_RESET_HIGH2_US, FPC_RESET_HIGH2_US + 100);
	//printk("%s: RST_PIN status: %d\n",__func__,fpsensor_gpio_read(FPSENSOR_RST_PIN));

	irq_gpio = fpsensor_gpio_read(fpc->irq_gpio);
	//add by yanqiyang end
	dev_info(dev, "IRQ after reset %d\n", irq_gpio);

	dev_info( dev, "Using GPIO#%d as IRQ.\n", fpc->irq_gpio );
	dev_info( dev, "Using GPIO#%d as RST.\n", fpc->rst_gpio );

	return 0;
}

static ssize_t hw_reset_set(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int rc;
	struct  fpc_data *fpc = dev_get_drvdata(dev);

	if (!strncmp(buf, "reset", strlen("reset"))) {
		rc = hw_reset(fpc);
		return rc ? rc : count;
	}
	else
		return -EINVAL;


}
static DEVICE_ATTR(hw_reset, S_IWUSR, NULL, hw_reset_set);

/**
 * sysfs node for controlling whether the driver is allowed
 * to wake up the platform on interrupt.
 */
static ssize_t wakeup_enable_set(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct  fpc_data *fpc = dev_get_drvdata(dev);

	if (!strncmp(buf, "enable", strlen("enable")))
	{
		fpc->wakeup_enabled = true;
		smp_wmb();
	}
	else if (!strncmp(buf, "disable", strlen("disable")))
	{
		fpc->wakeup_enabled = false;
		smp_wmb();
	}
	else
		return -EINVAL;

	return count;
}
static DEVICE_ATTR(wakeup_enable, S_IWUSR, NULL, wakeup_enable_set);

/**
 * sysf node to check the interrupt status of the sensor, the interrupt
 * handler should perform sysf_notify to allow userland to poll the node.
 */
static ssize_t irq_get(struct device *device,
			struct device_attribute *attribute,
			char* buffer)
{
	struct fpc_data *fpc = dev_get_drvdata(device);
	int irq = 0;
	
	irq = gpio_get_value(fpc->irq_gpio);
	//irq = irq_to_gpio(fpc->irq);
	printk("=====%s======: irq: %d, irq_gpio: %d\n",__func__,irq,fpc->irq_gpio);
	return scnprintf(buffer, PAGE_SIZE, "%i\n", irq);
}

/**
 * writing to the irq node will just drop a printk message
 * and return success, used for latency measurement.
 */
static ssize_t irq_ack(struct device *device,
			struct device_attribute *attribute,
			const char *buffer, size_t count)
{
	struct fpc_data *fpc = dev_get_drvdata(device);
	dev_dbg(fpc->dev, "%s\n", __func__);

	return count;
}

static DEVICE_ATTR(irq, S_IRUSR | S_IWUSR, irq_get, irq_ack);

static ssize_t clk_enable_set(struct device *device,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct fpc_data *fpc = dev_get_drvdata(device);

	if (!fpc->hwabs->clk_enable_set)
		return count;

	return fpc->hwabs->clk_enable_set(fpc, buf, count);
}

static DEVICE_ATTR(clk_enable, S_IWUSR, NULL, clk_enable_set);

static struct attribute *fpc_attributes[] = {
	&dev_attr_hw_reset.attr,
	&dev_attr_wakeup_enable.attr,
	&dev_attr_clk_enable.attr,
	&dev_attr_irq.attr,
	NULL
};

static const struct attribute_group const fpc_attribute_group = {
	.attrs = fpc_attributes,
};

static irqreturn_t fpc_irq_handler(int irq, void *handle)
{
	struct fpc_data *fpc = handle;
	if (fpc->hwabs->irq_handler)
		fpc->hwabs->irq_handler(irq, fpc);

	/* Make sure 'wakeup_enabled' is updated before using it
	** since this is interrupt context (other thread...) */
	smp_rmb();
	printk("fpc->wakeup_enabled = %d\n", fpc->wakeup_enabled);

	if (fpc->wakeup_enabled) {
		wake_lock_timeout(&fpc->ttw_wl,
					msecs_to_jiffies(FPC_TTW_HOLD_TIME));
	}

	sysfs_notify(&fpc->dev->kobj, NULL, dev_attr_irq.attr.name);

	return IRQ_HANDLED;
}

/*
static int fpc_request_named_gpio(struct fpc_data *fpc, const char *label, int *gpio)
{
	struct device *dev = fpc->dev;
	struct device_node *node = dev->of_node;
	int rc = of_get_named_gpio(node, label, 0);
	if (rc < 0) {
		dev_err(dev, "failed to get '%s'\n", label);
		return rc;
	}
	*gpio = rc;
	rc = devm_gpio_request(dev, *gpio, label);
	if (rc) {
		dev_err(dev, "failed to request gpio %d\n", *gpio);
		return rc;
	}
	dev_dbg(dev, "%s %d\n", label, *gpio);
	return 0;
}
*/

int fpc_probe(struct spi_device *pldev,
		struct fpc_gpio_info *fpc_gpio_ops)
{
	struct device *dev = &pldev->dev;
	//struct device_node *node = dev->of_node;
	struct fpc_data *fpc;
	int irqf = 0;
	//int irq_num;
	int rc;

	dev_dbg(dev, "%s\n", __func__);
	printk("yqy------test-------%d, %s\n", __LINE__, __func__);

	fpc = devm_kzalloc(dev, sizeof(*fpc), GFP_KERNEL);
	if (!fpc) {
		dev_err(dev,
			"failed to allocate memory for struct fpc_data\n");
		rc = -ENOMEM;
		goto exit;
	}

	fpc->dev = dev;
	g_fpsensor = fpc;
	dev_set_drvdata(dev, fpc);
	fpc->pldev = pldev;
	fpc->pldev->irq = 0;
	fpc->hwabs = fpc_gpio_ops;
#if 0
	if (!node) {
		dev_err(dev, "no of node found\n");
		rc = -EINVAL;
		goto exit;
	}
#endif
	rc = fpc->hwabs->init(fpc);

	if (rc) {
		printk(KERN_INFO "error\n");
		goto exit;
	}

	/* Get the gpio pin used for irq from device tree */
	/*
	rc = fpc_request_named_gpio(fpc, "fpc,gpio_irq",
			&fpc->irq_gpio);
	if (rc) {
		dev_err(dev, "Requesting GPIO for IRQ failed with %d.\n", rc);
		goto exit;
	}
	*/
	//add by yanqiyang start
	printk("yqy------test-------%d, %s\n", __LINE__, __func__);
	/*
	pldev->dev.of_node = of_find_compatible_node(NULL, NULL, "mediatek,fingerprint");
    fpc->pinctrl1 = devm_pinctrl_get(&pldev->dev);
    if (IS_ERR(fpc->pinctrl1)) {
        rc = PTR_ERR(fpc->pinctrl1);
        printk("fpc Cannot find fp pinctrl1--error = %d.\n", rc);
        //goto err1;
    }
*/
    fpsensor_spidev_dts_init(fpc);
	fpsensor_irq_gpio_cfg();
	fpsensor_hw_power_enable(1);
	fpsensor_spi_clk_enable(0);
	//add by yanqiyang end
/*
	rc = fpc_request_named_gpio(fpc, "fpc,gpio_rst",
			&fpc->rst_gpio);
	if (rc) {
		dev_err(dev, "Requesting GPIO for RST failed with %d.\n", rc);
		goto exit;
	}

	rc = fpc->hwabs->configure(fpc, &irq_num, &irqf);

	if (rc < 0)
		goto exit;

	dev_dbg(dev, "Using GPIO#%d as IRQ.\n", fpc->irq_gpio);
	dev_dbg(dev, "Using GPIO#%d as RST.\n", fpc->rst_gpio);
*/
	fpc->wakeup_enabled = false;

	irqf |= IRQF_ONESHOT;
	//if (of_property_read_bool(dev->of_node, "fpc,enable-wakeup")) {
	irqf |= IRQF_NO_SUSPEND;
	irqf |= IRQF_TRIGGER_RISING;
	device_init_wakeup(dev, 1);
	//}
	printk("wangbin test ,irq no is:%d\r\n",fpc->irq);

	/*
		retval = request_threaded_irq(gf_dev->irq, NULL, gf_irq,
				IRQF_TRIGGER_RISING | IRQF_ONESHOT, "goodix_fp_irq", gf_dev);
	*/
	#if 0
	rc = devm_request_threaded_irq(dev, fpc->irq,
			NULL, fpc_irq_handler, irqf,
			dev_name(dev), fpc);
	#else
	rc  = request_threaded_irq(fpc->irq, NULL, fpc_irq_handler,
					IRQF_TRIGGER_RISING | IRQF_ONESHOT, "fpc_fp_irq", fpc);

		
	#endif
	if (rc) {
		dev_err(dev, "could not request irq \n");
		goto exit;
	}
	printk("yqy------test-------%d, %s, irq num = %d\n", __LINE__, __func__, fpc->irq);
	//dev_dbg(dev, "requested irq %d\n", irq_num);

	/* Request that the interrupt should be wakeable */
	enable_irq_wake(fpc->irq);
	wake_lock_init(&fpc->ttw_wl, WAKE_LOCK_SUSPEND, "fpc_ttw_wl");

	rc = sysfs_create_group(&dev->kobj, &fpc_attribute_group);
	if (rc) {
		dev_err(dev, "could not create sysfs\n");
		goto exit;
	}

	(void)hw_reset(fpc);
	dev_info(dev, "%s: ok\n", __func__);
exit:
	return rc;
}

int fpc_remove(struct spi_device *pldev)
{
	struct  fpc_data *fpc = dev_get_drvdata(&pldev->dev);

	sysfs_remove_group(&pldev->dev.kobj, &fpc_attribute_group);
	wake_lock_destroy(&fpc->ttw_wl);
	dev_info(&pldev->dev, "%s\n", __func__);

	return 0;
}

MODULE_LICENSE("GPL");
