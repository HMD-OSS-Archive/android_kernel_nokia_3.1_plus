/*
 * fih_rfcable_class.c
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
#include "fih_rfcable_class.h"

#define FIH_RFCABLE_NOTIFICATION_TIMEOUT 2000

static ssize_t fih_rfcable_store_property(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count);
static ssize_t fih_rfcable_show_property(struct device *dev,
				       struct device_attribute *attr,
				       char *buf);

#define FIH_RFCABLE_ATTR(_name)				\
{							\
	.attr = { .name = #_name },			\
	.show = fih_rfcable_show_property,		\
	.store = fih_rfcable_store_property,		\
}

static struct device_attribute fih_rfcable_attrs[] = {
	FIH_RFCABLE_ATTR(status),
	FIH_RFCABLE_ATTR(fake_status),
};

struct class *fih_rfcable_class;
EXPORT_SYMBOL_GPL(fih_rfcable_class);

static struct device_type fih_rfcable_dev_type;

static char *kstrdupcase(const char *str, gfp_t gfp, bool to_upper)
{
	char *ret, *ustr;

	ustr = ret = kmalloc(strlen(str) + 1, gfp);

	if (!ret)
		return NULL;

	while (*str)
		*ustr++ = to_upper ? toupper(*str++) : tolower(*str++);

	*ustr = 0;

	return ret;
}

static void fih_rfcable_changed_work(struct work_struct *work);

void fih_rfcable_instance_changed(struct fih_rfcable_phy_instance *fih_rfcable)
{
	dev_dbg(&fih_rfcable->dev, "%s\n", __func__);
	pm_wakeup_event(&fih_rfcable->dev, FIH_RFCABLE_NOTIFICATION_TIMEOUT);
	schedule_work(&fih_rfcable->changed_work);
}
EXPORT_SYMBOL_GPL(fih_rfcable_instance_changed);

int fih_rfcable_get_property(struct fih_rfcable_phy_instance *fih_rfcable,
			   enum fih_rfcable_property prop,
			   unsigned int *val)
{
	return fih_rfcable->desc->get_property(fih_rfcable, prop, val);
}
EXPORT_SYMBOL_GPL(fih_rfcable_get_property);

int fih_rfcable_set_property(struct fih_rfcable_phy_instance *fih_rfcable,
			   enum fih_rfcable_property prop,
			   const unsigned int *val)
{
	if (!fih_rfcable->desc->set_property)
		return -ENODEV;

	return fih_rfcable->desc->set_property(fih_rfcable, prop, val);
}
EXPORT_SYMBOL_GPL(fih_rfcable_set_property);

int fih_rfcable_property_is_writeable(struct fih_rfcable_phy_instance *fih_rfcable,
				    enum fih_rfcable_property prop)
{
	if (!fih_rfcable->desc->property_is_writeable)
		return -ENODEV;

	return fih_rfcable->desc->property_is_writeable(fih_rfcable, prop);
}
EXPORT_SYMBOL_GPL(fih_rfcable_property_is_writeable);

static void fih_rfcable_dev_release(struct device *dev)
{
	struct fih_rfcable_phy_instance *fih_rfcable =
	    container_of(dev, struct fih_rfcable_phy_instance, dev);
	pr_debug("device: '%s': %s\n", dev_name(dev), __func__);
	kfree(fih_rfcable);
}

static struct fih_rfcable_phy_instance *__must_check
__fih_rfcable_register(struct device *parent,
		     const struct fih_rfcable_phy_desc *desc)
{
	struct device *dev;
	struct fih_rfcable_phy_instance *fih_rfcable;
	int rc;

	fih_rfcable = kzalloc(sizeof(*fih_rfcable), GFP_KERNEL);
	if (!fih_rfcable)
		return ERR_PTR(-ENOMEM);

	dev = &fih_rfcable->dev;

	device_initialize(dev);

	dev->class = fih_rfcable_class;
	dev->type = &fih_rfcable_dev_type;
	dev->parent = parent;
	dev->release = fih_rfcable_dev_release;
	dev_set_drvdata(dev, fih_rfcable);
	fih_rfcable->desc = desc;

	rc = dev_set_name(dev, "%s", desc->name);
	if (rc)
		goto dev_set_name_failed;

	INIT_WORK(&fih_rfcable->changed_work, fih_rfcable_changed_work);

	rc = device_init_wakeup(dev, true);
	if (rc)
		goto wakeup_init_failed;

	rc = device_add(dev);
	if (rc)
		goto device_add_failed;

	fih_rfcable_instance_changed(fih_rfcable);

	return fih_rfcable;

device_add_failed:
	device_init_wakeup(dev, false);
wakeup_init_failed:
dev_set_name_failed:
	put_device(dev);
	kfree(fih_rfcable);

	return ERR_PTR(rc);
}

static void fih_rfcable_instance_unregister(struct fih_rfcable_phy_instance
					  *fih_rfcable)
{
	cancel_work_sync(&fih_rfcable->changed_work);
	device_init_wakeup(&fih_rfcable->dev, false);
	device_unregister(&fih_rfcable->dev);
}

static void devm_fih_rfcable_release(struct device *dev, void *res)
{
	struct fih_rfcable_phy_instance **fih_rfcable = res;

	fih_rfcable_instance_unregister(*fih_rfcable);
}

struct fih_rfcable_phy_instance *__must_check
devm_fih_rfcable_instance_register(struct device *parent,
				 const struct fih_rfcable_phy_desc *desc)
{
	struct fih_rfcable_phy_instance **ptr, *fih_rfcable;

	ptr = devres_alloc(devm_fih_rfcable_release, sizeof(*ptr), GFP_KERNEL);

	if (!ptr)
		return ERR_PTR(-ENOMEM);
	fih_rfcable = __fih_rfcable_register(parent, desc);
	if (IS_ERR(fih_rfcable)) {
		devres_free(ptr);
	} else {
		*ptr = fih_rfcable;
		devres_add(parent, ptr);
	}
	return fih_rfcable;
}
EXPORT_SYMBOL_GPL(devm_fih_rfcable_instance_register);


void *fih_rfcable_get_drvdata(struct fih_rfcable_phy_instance *fih_rfcable)
{
	return fih_rfcable->drv_data;
}
EXPORT_SYMBOL_GPL(fih_rfcable_get_drvdata);

/***************** Device attribute functions **************************/

/* port type */
static char *status_text[] = {
	"inserted", "not-inserted", "unknow"
};

/* current mode */
static char *fake_status_text[] = {
	"0", "1", "unknow", "not set"
};

static ssize_t fih_rfcable_show_property(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	struct fih_rfcable_phy_instance *fih_rfcable = dev_get_drvdata(dev);
	const ptrdiff_t off = attr - fih_rfcable_attrs;
	unsigned int value;

	ret = fih_rfcable_get_property(fih_rfcable, off, &value);

	if (ret < 0) {
		if (ret == -ENODATA)
			pr_err("driver has no data for `%s' property\n",	attr->attr.name);
		else if (ret != -ENODEV)
			pr_err("driver failed to report `%s' property: %zd\n",attr->attr.name, ret);
		return ret;
	}

	if (off == FIH_RFCABLE_PROP_STATUS) {
			return snprintf(buf, PAGE_SIZE, "%s\n",
					status_text[value]);
	} else if (off == FIH_RFCABLE_PROP_FAKE_STATUS) {
			return snprintf(buf, PAGE_SIZE, "%s\n", 
					fake_status_text[value]);
	} else
		return -EIO;
}

static ssize_t fih_rfcable_store_property(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	ssize_t ret;
	struct fih_rfcable_phy_instance *fih_rfcable = dev_get_drvdata(dev);
	const ptrdiff_t off = attr - fih_rfcable_attrs;
	unsigned int value;
	int total, i;
	char *dup_buf, **text_array;

	dup_buf = kstrdupcase(buf, GFP_KERNEL, false);
	switch (off) {
	case FIH_RFCABLE_PROP_FAKE_STATUS:
		total = FIH_RFCABLE_PROP_FAKE_STATUS_TOTAL;
		text_array = fake_status_text;
		break;
	default:
		ret = -EINVAL;
		goto error;
	}

	for (i = 0; i <= total; i++) {
		if (i == total) {
			ret = -ENOTSUPP;
			goto error;
		}
		if (!strncmp(*(text_array + i), dup_buf,
			     strlen(*(text_array + i)))) {
			value = i;
			break;
		}
	}

	ret = fih_rfcable->desc->set_property(fih_rfcable, off, &value);

error:
	kfree(dup_buf);

	if (ret < 0)
		return ret;

	return count;
}

static struct attribute *__fih_rfcable_attrs[ARRAY_SIZE(fih_rfcable_attrs) + 1];

static struct attribute_group fih_rfcable_attr_group = {
	.attrs = __fih_rfcable_attrs,
};

static const struct attribute_group *fih_rfcable_attr_groups[] = {
	&fih_rfcable_attr_group,
	NULL,
};

void fih_rfcable_init_attrs(struct device_type *dev_type)
{
	int i;

	dev_type->groups = fih_rfcable_attr_groups;

	for (i = 0; i < ARRAY_SIZE(fih_rfcable_attrs); i++)
		__fih_rfcable_attrs[i] = &fih_rfcable_attrs[i].attr;
}

int fih_rfcable_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	struct fih_rfcable_phy_instance *fih_rfcable = dev_get_drvdata(dev);
	int ret = 0, j;
	char *prop_buf;
	char *attrname;

	if (!fih_rfcable || !fih_rfcable->desc) {
		pr_err("No fih_rfcable phy yet\n");
		return ret;
	}

	prop_buf = (char *)get_zeroed_page(GFP_KERNEL);
	if (!prop_buf)
		return -ENOMEM;

	for (j = 0; j < fih_rfcable->desc->num_properties; j++) {
		struct device_attribute *attr;
		char *line;

		attr = &fih_rfcable_attrs[fih_rfcable->desc->properties[j]];

		ret = fih_rfcable_show_property(dev, attr, prop_buf);
		if (ret == -ENODEV || ret == -ENODATA) {
			ret = 0;
			continue;
		}

		if (ret < 0)
			goto out;
		line = strnchr(prop_buf, PAGE_SIZE, '\n');
		if (line)
			*line = 0;

		attrname = kstrdupcase(attr->attr.name, GFP_KERNEL, true);
		if (!attrname)
			ret = -ENOMEM;

		pr_debug("prop %s=%s\n", attrname, prop_buf);

		ret = add_uevent_var(env, "FIH_RFCABLE_%s=%s", attrname,
				     prop_buf);
		kfree(attrname);
		if (ret)
			goto out;
	}

	ret = add_uevent_var(env, "FIH_RFCABLE_NAME=%s", fih_rfcable->desc->name);
	if (ret)
	{
		pr_err("[%s]  add_uevent_var fail, ret=%d \n",__func__, ret);
		goto out;
	}

out:
	free_page((unsigned long)prop_buf);

	return ret;
}

static void fih_rfcable_changed_work(struct work_struct *work)
{
	struct fih_rfcable_phy_instance *fih_rfcable =
	    container_of(work, struct fih_rfcable_phy_instance,
			 changed_work);

	sysfs_update_group(&fih_rfcable->dev.kobj, &fih_rfcable_attr_group);
	kobject_uevent(&fih_rfcable->dev.kobj, KOBJ_CHANGE);
}

/******************* Module Init ***********************************/

static int __init fih_rfcable_class_init(void)
{
	fih_rfcable_class = class_create(THIS_MODULE, "rfcable_det");

	if (IS_ERR(fih_rfcable_class))
		return PTR_ERR(fih_rfcable_class);

	fih_rfcable_class->dev_uevent = fih_rfcable_uevent;
	fih_rfcable_init_attrs(&fih_rfcable_dev_type);

	return 0;
}

static void __exit fih_rfcable_class_exit(void)
{
	class_destroy(fih_rfcable_class);
}

subsys_initcall(fih_rfcable_class_init);
module_exit(fih_rfcable_class_exit);
