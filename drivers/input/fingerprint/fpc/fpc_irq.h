#ifndef _FPC_IRQ_H_

#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/wakelock.h>
#include <linux/regulator/consumer.h>

struct fpc_gpio_info;

struct fpc_data {
	struct device *dev;
	struct spi_device *pldev;

	int irq;	
	int irq_gpio;
	int rst_gpio;

	bool wakeup_enabled;

	struct wake_lock ttw_wl;

	struct regulator *vdd_tx;
	
	bool power_enabled;
	bool use_regulator_for_bezel;
	const struct fpc_gpio_info *hwabs;
//add by yanqiyang start
	struct pinctrl *pinctrl1;
	struct pinctrl_state *eint_as_int, *eint_in_low, *eint_in_high, *eint_in_float, *fp_rst_low, *fp_rst_high,
            *fp_spi_miso, *fp_spi_mosi, *fp_spi_clk, *fp_cs_low, *fp_cs_high,*pins_power_on, *pins_power_off;
//add by yanqiyang end
};

struct fpc_gpio_info {
	int (*init)(struct fpc_data *fpc);
	int (*configure)(struct fpc_data *fpc, int *irq_num, int *trigger_flags);
	int (*get_val)(unsigned gpio);
	void (*set_val)(unsigned gpio, int val);
	ssize_t (*clk_enable_set)(struct fpc_data *fpc, const char *buf, size_t count);
	void (*irq_handler)(int irq, struct fpc_data *fpc);
	void *priv;
};

extern int fpc_probe(struct spi_device *pldev,
			struct fpc_gpio_info *fpc_gpio_ops);

extern int fpc_remove(struct spi_device *pldev);
//add by yanqiyang start
extern void mt_spi_disable_master_clk(struct spi_device *spidev);
extern void mt_spi_enable_master_clk(struct spi_device *spidev);
//add by yanqiyang end
#endif
