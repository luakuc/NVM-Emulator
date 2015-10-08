/*
 *	Copyright (C) 2015 Yizhou Shan <shanyizhou@ict.ac.cn>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License along
 *	with this program; if not, write to the Free Software Foundation, Inc.,
 *	51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#define pr_fmt(fmt) "UNCORE PMU: " fmt

#include "uncore_pmu.h"

#include <linux/pci.h>
#include <linux/list.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

/* CPU-Independent Data Structures */
struct uncore_box_type **uncore_msr_type;
struct uncore_box_type **uncore_pci_type;
struct pci_driver *uncore_pci_driver;

static bool pci_driver_registered = false;

static void uncore_event_show(struct uncore_event *event)
{
	unsigned long long v1, v2;

	if (!event | !event->ctl | !event->ctr)
		return;
	
	rdmsrl(event->ctl, v1);
	rdmsrl(event->ctr, v2);
	printk(KERN_INFO "SEL=%llx CNT=%llx", v1, v2);
}

static int uncore_pci_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	struct uncore_box_type *type;

	type = uncore_pci_type[UNCORE_PCI_DEV_TYPE(id->driver_data)];
	pr_notice("%s", type->name);
	
	return 0;
}

static void uncore_pci_init(void)
{
	int ret;

	hswep_pci_init();

	uncore_pci_driver->probe = uncore_pci_probe;
	ret = pci_register_driver(uncore_pci_driver);
	if (ret == 0)
		pci_driver_registered = true;
	else
		pr_err("Fail to register UNCORE_PCI_DRIVER");

}

static void uncore_cpu_init(void)
{
	hswep_cpu_init();
}

static int uncore_init(void)
{
	uncore_cpu_init();
	uncore_pci_init();
/*
	struct uncore_box cbox = {
		.idx = 0,
		.name = "C0",
		.box_type = &HSWEP_UNCORE_CBOX
	};

	struct uncore_event event = {
		.ctl = 0xe01,
		.ctr = 0xe08,
		.enable = (1<<22) | 0x0000 | 0x0000,
		.disable = 0
	};
	
	uncore_event_show(&event);

	uncore_init_box(&cbox);
	uncore_enable_box(&cbox);
	uncore_enable_event(&cbox, &event);
	udelay(100);
	uncore_disable_event(&cbox, &event);
	uncore_disable_box(&cbox);

	uncore_event_show(&event);
*/	
	return 0;
}

static void uncore_exit(void)
{
	pr_notice("exit");

	if (pci_driver_registered)
		pci_unregister_driver(uncore_pci_driver);
}

module_init(uncore_init);
module_exit(uncore_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("shanyizhou@ict.ac.cn");
