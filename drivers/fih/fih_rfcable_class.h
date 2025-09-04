#ifndef __LINUX_CLASS_FIH_RFCABLE_H__
#define __LINUX_CLASS_FIH_RFCABLE_H__

#include <linux/workqueue.h>
#include <linux/errno.h>

struct device;

enum {
	FIH_RFCABLE_PROP_FAKE_STATUS_RF_ENABLE = 0,
	FIH_RFCABLE_PROP_FAKE_STATUS_RF_DISABLE,
	FIH_RFCABLE_PROP_FAKE_STATUS_UNKNOW,
	FIH_RFCABLE_PROP_FAKE_STATUS_NOT_SET,
/*The following should be the last element*/
	FIH_RFCABLE_PROP_FAKE_STATUS_TOTAL,
};

enum fih_rfcable_property {
	FIH_RFCABLE_PROP_STATUS = 0,
	FIH_RFCABLE_PROP_FAKE_STATUS,
};

struct fih_rfcable_phy_instance;

/* Description of typec port */
struct fih_rfcable_phy_desc {
	/* /sys/class/fih_rfcable_usb/<name>/ */
	const char *name;
//	enum fih_rfcable_supported_modes supported_modes;
	enum fih_rfcable_property *properties;
	size_t num_properties;

	/* Callback for "cat /sys/class/fih_rfcable_usb/<name>/<property>" */
	int (*get_property)(struct fih_rfcable_phy_instance *fih_rfcable,
			     enum fih_rfcable_property prop,
			     unsigned int *val);
	/* Callback for "echo <value> >
	 *                      /sys/class/fih_rfcable_usb/<name>/<property>" */
	int (*set_property)(struct fih_rfcable_phy_instance *fih_rfcable,
			     enum fih_rfcable_property prop,
			     const unsigned int *val);
	/* Decides whether userspace can change a specific property */
	int (*property_is_writeable)(struct fih_rfcable_phy_instance *fih_rfcable,
				      enum fih_rfcable_property prop);
};

struct fih_rfcable_phy_instance {
	const struct fih_rfcable_phy_desc *desc;

	/* Driver private data */
	void *drv_data;

	struct device dev;
	struct work_struct changed_work;
};

extern void fih_rfcable_instance_changed(struct fih_rfcable_phy_instance *fih_rfcable);

extern struct fih_rfcable_phy_instance *__must_check
devm_fih_rfcable_instance_register(struct device *parent,
				 const struct fih_rfcable_phy_desc *desc);

extern void *fih_rfcable_get_drvdata(struct fih_rfcable_phy_instance *fih_rfcable);

#endif /* __LINUX_CLASS_FIH_RFCABLE_H__ */
