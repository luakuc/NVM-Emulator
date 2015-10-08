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

/*
 * Support:
 * O	Platform:		Xeon® E5 v3, Xeon® E7 v3
 *	Microarchitecture:	Haswell-EP, Haswell-EX
 *
 * Ancient:
 * O	Platform:		Xeon® E5 v2, Xeon® E7 v2
 *	Microarchitecture:	Ivy Bridge-EP, Ivy Bridge-EX
 *
 * O	Platform:		Xeon® E5, Xeon® E7
 *	Microarchitecture:	Sandy Bridge-EP, Westmere-EX
 */

#include "uncore_pmu.h"

#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/types.h>

/* HSWEP Box-Level Control MSR Bit Layout */
#define HSWEP_MSR_BOX_CTL_RST_CTRL	(1 << 0)
#define HSWEP_MSR_BOX_CTL_RST_CTRS	(1 << 1)
#define HSWEP_MSR_BOX_CTL_FRZ		(1 << 8)
#define HSWEP_MSR_BOX_CTL_INIT		(HSWEP_MSR_BOX_CTL_RST_CTRL | \
					 HSWEP_MSR_BOX_CTL_RST_CTRS )

/* HSWEP Box-Level Control PCI Bit Layout */
#define HSWEP_PCI_BOX_CTL_FRZ		HSWEP_MSR_BOX_CTL_FRZ
#define HSWEP_PCI_BOX_CTL_INIT		HSWEP_MSR_BOX_CTL_INIT

/* HSWEP Event Select MSR Bit Layout */
#define HSWEP_MSR_EVNTSEL_EVENT		0x000000FF
#define HSWEP_MSR_EVNTSEL_UMASK		0x0000FF00
#define HSWEP_MSR_EVNTSEL_RST		(1 << 17)
#define HSWEP_MSR_EVNTSEL_EDGE_DET	(1 << 18)
#define HSWEP_MSR_EVNTSEL_TID_EN	(1 << 19)
#define HSWEP_MSR_EVNTSEL_EN		(1 << 22)
#define HSWEP_MSR_EVNTSEL_INVERT	(1 << 23)
#define HSWEP_MSR_EVNTSEL_THRESHOLD	0xFF000000
#define HSWEP_MSR_RAW_EVNTSEL_MASK	(HSWEP_MSR_EVNTSEL_EVENT	| \
					 HSWEP_MSR_EVNTSEL_UMASK	| \
					 HSWEP_MSR_EVNTSEL_EDGE_DET	| \
					 HSWEP_MSR_EVNTSEL_INVERT	| \
					 HSWEP_MSR_EVNTSEL_THRESHOLD)

/* HSWEP Uncore Global Per-Socket MSRs */
#define HSWEP_MSR_PMON_GLOBAL_CTL		0x700
#define HSWEP_MSR_PMON_GLOBAL_STATUS		0x701
#define HSWEP_MSR_PMON_GLOBAL_CONFIG		0x702

/* HSWEP Uncore U-box */
#define HSWEP_MSR_U_PMON_BOX_STATUS		0x708
#define HSWEP_MSR_U_PMON_UCLK_FIXED_CTL		0x703
#define HSWEP_MSR_U_PMON_UCLK_FIXED_CTR		0x704
#define HSWEP_MSR_U_PMON_EVNTSEL0		0x705
#define HSWEP_MSR_U_PMON_CTR0			0x709

/* HSWEP Uncore PCU-box */
#define HSWEP_MSR_PCU_PMON_BOX_CTL		0x710
#define HSWEP_MSR_PCU_PMON_BOX_FILTER		0x715
#define HSWEP_MSR_PCU_PMON_BOX_STATUS		0x716
#define HSWEP_MSR_PCU_PMON_EVNTSEL0		0x711
#define HSWEP_MSR_PCU_PMON_CTR0			0x717

/* HSWEP Uncore S-box */
#define HSWEP_MSR_S_PMON_BOX_CTL		0x720
#define HSWEP_MSR_S_PMON_BOX_FILTER		0x725
#define HSWEP_MSR_S_PMON_EVNTSEL0		0x721
#define HSWEP_MSR_S_PMON_CTR0			0x726
#define HSWEP_MSR_S_MSR_OFFSET			0xA

/* HSWEP Uncore C-box */
#define HSWEP_MSR_C_PMON_BOX_CTL		0xE00
#define HSWEP_MSR_C_PMON_BOX_FILTER0		0xE05
#define HSWEP_MSR_C_PMON_BOX_FILTER1		0xE06
#define HSWEP_MSR_C_PMON_BOX_STATUS		0xE07
#define HSWEP_MSR_C_PMON_EVNTSEL0		0xE01
#define HSWEP_MSR_C_PMON_CTR0			0xE08
#define HSWEP_MSR_C_MSR_OFFSET			0x10
#define HSWEP_MSR_C_EVENTSEL_MASK		(HSWEP_MSR_RAW_EVNTSEL_MASK | \
						 HSWEP_MSR_EVNTSEL_TID_EN)

/* HSWEP Uncore HA-box */
#define HSWEP_PCI_HA_PMON_BOX_STATUS		0xF8
#define HSWEP_PCI_HA_PMON_BOX_CTL		0xF4
#define HSWEP_PCI_HA_PMON_CTL0			0xD8
#define HSWEP_PCI_HA_PMON_CTR0			0xA0
#define HSWEP_PCI_HA_PMON_BOX_OPCODEMATCH	0x48
#define HSWEP_PCI_HA_PMON_BOX_ADDRMATCH1	0x44
#define HSWEP_PCI_HA_PMON_BOX_ADDRMATCH0	0x40

/* HSWEP Uncore IMC-box */
#define HSWEP_PCI_IMC_PMON_BOX_STATUS		0xF8
#define HSWEP_PCI_IMC_PMON_BOX_CTL		0xF4
#define HSWEP_PCI_IMC_PMON_CTL0			0xD8
#define HSWEP_PCI_IMC_PMON_CTR0			0xA0
#define HSWEP_PCI_IMC_PMON_FIXED_CTL		0xF0
#define HSWEP_PCI_IMC_PMON_FIXED_CTR		0xD0

/* HSWEP Uncore IRP-box */
#define HSWEP_PCI_IRP_PMON_BOX_STATUS		0xF8
#define HSWEP_PCI_IRP_PMON_BOX_CTL		0xF4

/* HSWEP Uncore QPI-box */
#define HSWEP_PCI_QPI_PMON_BOX_STATUS		0xF8
#define HSWEP_PCI_QPI_PMON_BOX_CTL		0xF4
#define HSWEP_PCI_QPI_PMON_CTL0			0xD8
#define HSWEP_PCI_QPI_PMON_CTR0			0xA0

/* HSWEP Uncore R2PCIE-box */
#define HSWEP_PCI_R2PCIE_PMON_BOX_STATUS	0xF8
#define HSWEP_PCI_R2PCIE_PMON_BOX_CTL		0xF4
#define HSWEP_PCI_R2PCIE_PMON_CTL0		0xD8
#define HSWEP_PCI_R2PCIE_PMON_CTR0		0xA0

/* HSWEP Uncore R3QPI-box */
#define HSWEP_PCI_R3QPI_PMON_BOX_STATUS		0xF8
#define HSWEP_PCI_R3QPI_PMON_BOX_CTL		0xF4
#define HSWEP_PCI_R3QPI_PMON_CTL0		0xD8
#define HSWEP_PCI_R3QPI_PMON_CTR0		0xA0

/*
 * MSR Box Part
 */

static void hswep_uncore_msr_init_box(struct uncore_box *box)
{
	unsigned int msr;

	msr = uncore_msr_box_ctl(box);
	if (msr)
		wrmsrl(msr, HSWEP_MSR_BOX_CTL_INIT);
}

static void hswep_uncore_msr_enable_box(struct uncore_box *box)
{
	unsigned long long config;
	unsigned int msr;

	msr = uncore_msr_box_ctl(box);
	if (msr) {
		rdmsrl(msr, config);
		config &= ~HSWEP_MSR_BOX_CTL_FRZ;
		wrmsrl(msr, config);
	}
}

static void hswep_uncore_msr_disable_box(struct uncore_box *box)
{
	unsigned long long config;
	unsigned int msr;

	msr = uncore_msr_box_ctl(box);
	if (msr) {
		rdmsrl(msr, config);
		config |= HSWEP_MSR_BOX_CTL_FRZ;
		wrmsrl(msr, config);
	}
}

/* FIXME Maybe merging two masks? */
static void hswep_uncore_msr_enable_event(struct uncore_box *box,
					struct uncore_event *event)
{
	wrmsrl(event->ctl, event->disable);
}

static void hswep_uncore_msr_disable_event(struct uncore_box *box,
					struct uncore_event *event)
{
	wrmsrl(event->ctl, event->enable);
}

#define HSWEP_UNCORE_MSR_BOX_OPS()				\
	.init_box	= hswep_uncore_msr_init_box,		\
	.enable_box	= hswep_uncore_msr_enable_box,		\
	.disable_box	= hswep_uncore_msr_disable_box,		\
	.enable_event	= hswep_uncore_msr_enable_event,	\
	.disable_event	= hswep_uncore_msr_disable_event	\

const struct uncore_box_ops HSWEP_UNCORE_UBOX_OPS = {
	HSWEP_UNCORE_MSR_BOX_OPS()
};

const struct uncore_box_ops HSWEP_UNCORE_PCUBOX_OPS = {
	HSWEP_UNCORE_MSR_BOX_OPS()
};

const struct uncore_box_ops HSWEP_UNCORE_SBOX_OPS = {
	HSWEP_UNCORE_MSR_BOX_OPS()
};

const struct uncore_box_ops HSWEP_UNCORE_CBOX_OPS = {
	HSWEP_UNCORE_MSR_BOX_OPS()
};

struct uncore_box_type HSWEP_UNCORE_UBOX = {
	.name		= "U-BOX MSR Type",
	.num_counters	= 2,
	.num_boxes	= 1,
	.perf_ctr_bits	= 48,
	.perf_ctr	= HSWEP_MSR_U_PMON_CTR0,
	.perf_ctl	= HSWEP_MSR_U_PMON_EVNTSEL0,
	.event_mask	= 0,
	.fixed_ctr_bits	= 48,
	.fixed_ctr	= HSWEP_MSR_U_PMON_UCLK_FIXED_CTR,
	.fixed_ctl	= HSWEP_MSR_U_PMON_UCLK_FIXED_CTL,
	.ops		= &HSWEP_UNCORE_UBOX_OPS
};

struct uncore_box_type HSWEP_UNCORE_PCUBOX = {
	.name		= "PCU-BOX MSR Type",
	.num_counters	= 4,
	.num_boxes	= 1,
	.perf_ctr_bits	= 48,
	.perf_ctr	= HSWEP_MSR_PCU_PMON_CTR0,
	.perf_ctl	= HSWEP_MSR_PCU_PMON_EVNTSEL0,
	.event_mask	= 0,
	.box_ctl	= HSWEP_MSR_PCU_PMON_BOX_CTL,
	.box_status	= HSWEP_MSR_PCU_PMON_BOX_STATUS,
	.ops		= &HSWEP_UNCORE_PCUBOX_OPS
};

struct uncore_box_type HSWEP_UNCORE_SBOX = {
	.name		= "S-BOX MSR Type",
	.num_counters	= 4,
	.num_boxes	= 4,
	.perf_ctr_bits	= 48,
	.perf_ctr	= HSWEP_MSR_S_PMON_CTR0,
	.perf_ctl	= HSWEP_MSR_S_PMON_EVNTSEL0,
	.event_mask	= 0,
	.box_ctl	= HSWEP_MSR_S_PMON_BOX_CTL,
	.msr_offset	= HSWEP_MSR_S_MSR_OFFSET,
	.ops		= &HSWEP_UNCORE_SBOX_OPS
};

struct uncore_box_type HSWEP_UNCORE_CBOX = {
	.name		= "C-BOX MSR Type",
	.num_counters	= 4,
	.num_boxes	= 18,
	.perf_ctr_bits	= 48,
	.perf_ctr	= HSWEP_MSR_C_PMON_CTR0,
	.perf_ctl	= HSWEP_MSR_C_PMON_EVNTSEL0,
	.event_mask	= HSWEP_MSR_C_EVENTSEL_MASK,
	.box_ctl	= HSWEP_MSR_C_PMON_BOX_CTL,
	.box_status	= HSWEP_MSR_C_PMON_BOX_STATUS,
	.msr_offset	= HSWEP_MSR_C_MSR_OFFSET,
	.ops		= &HSWEP_UNCORE_CBOX_OPS
};

enum {
	HSWEP_UNCORE_UBOX_ID,
	HSWEP_UNCORE_PCUBOX_ID,
	HSWEP_UNCORE_SBOX_ID,
	HSWEP_UNCORE_CBOX_ID
};

/* MSR Boxes */
struct uncore_box_type *HSWEP_UNCORE_MSR_TYPE[] = {
	[HSWEP_UNCORE_UBOX_ID]   = &HSWEP_UNCORE_UBOX,
	[HSWEP_UNCORE_PCUBOX_ID] = &HSWEP_UNCORE_PCUBOX,
	[HSWEP_UNCORE_SBOX_ID]   = &HSWEP_UNCORE_SBOX,
	[HSWEP_UNCORE_CBOX_ID]   = &HSWEP_UNCORE_CBOX,
	NULL
};

/*
 * PCI Box Part
 */

static void hswep_uncore_pci_init_box(struct uncore_box *box)
{
	pci_write_config_dword(box->pci_dev,
			       uncore_pci_box_ctl(box),
			       HSWEP_PCI_BOX_CTL_INIT);
}

static void hswep_uncore_pci_enable_box(struct uncore_box *box)
{
	struct pci_dev *dev = box->pci_dev;
	unsigned int ctl = uncore_pci_box_ctl(box);
	unsigned int config = 0; 
	
	if (!pci_read_config_dword(dev, ctl, &config)) {
		config &= ~HSWEP_PCI_BOX_CTL_FRZ;
		pci_write_config_dword(dev, ctl, config);
	}
}

static void hswep_uncore_pci_disable_box(struct uncore_box *box)
{
	struct pci_dev *dev = box->pci_dev;
	unsigned int ctl = uncore_pci_box_ctl(box);
	unsigned int config = 0; 
	
	if (!pci_read_config_dword(dev, ctl, &config)) {
		config |= HSWEP_PCI_BOX_CTL_FRZ;
		pci_write_config_dword(dev, ctl, config);
	}
}

static void hswep_uncore_pci_enable_event(struct uncore_box *box,
					  struct uncore_event *event)
{
	pci_write_config_dword(box->pci_dev,
			       event->ctl,
			       event->enable);
}

static void hswep_uncore_pci_disable_event(struct uncore_box *box,
					   struct uncore_event *event)
{
	pci_write_config_dword(box->pci_dev,
			       event->ctl,
			       event->disable);
}

#define HSWEP_UNCORE_PCI_BOX_OPS()				\
	.init_box	= hswep_uncore_pci_init_box,		\
	.enable_box	= hswep_uncore_pci_enable_box,		\
	.disable_box	= hswep_uncore_pci_disable_box,		\
	.enable_event	= hswep_uncore_pci_enable_event,	\
	.disable_event	= hswep_uncore_pci_disable_event	\

const struct uncore_box_ops HSWEP_UNCORE_HABOX_OPS = {
	HSWEP_UNCORE_PCI_BOX_OPS()
};

const struct uncore_box_ops HSWEP_UNCORE_IMCBOX_OPS = {
	HSWEP_UNCORE_PCI_BOX_OPS()
};

const struct uncore_box_ops HSWEP_UNCORE_IRPBOX_OPS = {
	HSWEP_UNCORE_PCI_BOX_OPS()
};

const struct uncore_box_ops HSWEP_UNCORE_QPIBOX_OPS = {
	HSWEP_UNCORE_PCI_BOX_OPS()
};

const struct uncore_box_ops HSWEP_UNCORE_R2PCIEBOX_OPS = {
	HSWEP_UNCORE_PCI_BOX_OPS()
};

const struct uncore_box_ops HSWEP_UNCORE_R3QPIBOX_OPS = {
	HSWEP_UNCORE_PCI_BOX_OPS()
};

struct uncore_box_type HSWEP_UNCORE_HA = {
	.name		= "HA-Box PCI Type",
	.num_counters	= 5,
	.num_boxes	= 2,
	.perf_ctr_bits  = 48,
	.perf_ctr	= HSWEP_PCI_HA_PMON_CTR0,
	.perf_ctl	= HSWEP_PCI_HA_PMON_CTL0,
	.event_mask	= 0,
	.box_ctl	= HSWEP_PCI_HA_PMON_BOX_CTL,
	.box_status	= HSWEP_PCI_HA_PMON_BOX_STATUS,
	.ops		= &HSWEP_UNCORE_HABOX_OPS
};

struct uncore_box_type HSWEP_UNCORE_IMC = {
	.name		= "IMC-Box PCI Type",
	.num_counters	= 5,
	.num_boxes	= 8,
	.perf_ctr_bits	= 48,
	.perf_ctr	= HSWEP_PCI_IMC_PMON_CTR0,
	.perf_ctl	= HSWEP_PCI_IMC_PMON_CTL0,
	.event_mask	= 0,
	.fixed_ctr_bits	= 48,
	.fixed_ctr	= HSWEP_PCI_IMC_PMON_FIXED_CTR,
	.fixed_ctl	= HSWEP_PCI_IMC_PMON_FIXED_CTL,
	.box_ctl	= HSWEP_PCI_IMC_PMON_BOX_CTL,
	.box_status	= HSWEP_PCI_IMC_PMON_BOX_STATUS,
	.ops		= &HSWEP_UNCORE_IMCBOX_OPS
};

struct uncore_box_type HSWEP_UNCORE_IRP = {
	.name		= "IRP-Box PCI Type",
	.num_counters	= 4,
	.num_boxes	= 1,
	.box_ctl	= HSWEP_PCI_IRP_PMON_BOX_CTL,
	.box_status	= HSWEP_PCI_IRP_PMON_BOX_STATUS,
	.ops		= &HSWEP_UNCORE_IRPBOX_OPS
};

struct uncore_box_type HSWEP_UNCORE_QPI = {
	.name		= "QPI-Box PCI Type",
	.num_counters	= 4,
	.num_boxes	= 2,
	.perf_ctr_bits	= 48,
	.perf_ctr	= HSWEP_PCI_QPI_PMON_CTR0,
	.perf_ctl	= HSWEP_PCI_QPI_PMON_CTL0,
	.event_mask	= 0,
	.box_ctl	= HSWEP_PCI_QPI_PMON_BOX_CTL,
	.box_status	= HSWEP_PCI_QPI_PMON_BOX_STATUS,
	.ops		= &HSWEP_UNCORE_QPIBOX_OPS
};

struct uncore_box_type HSWEP_UNCORE_R2PCIE = {
	.name		= "R2PCIE-Box PCI Type",
	.num_counters	= 4,
	.num_boxes	= 1,
	.perf_ctr_bits	= 48,
	.perf_ctr	= HSWEP_PCI_R2PCIE_PMON_CTR0,
	.perf_ctl	= HSWEP_PCI_R2PCIE_PMON_CTL0,
	.event_mask	= 0,
	.box_ctl	= HSWEP_PCI_R2PCIE_PMON_BOX_CTL,
	.box_status	= HSWEP_PCI_R2PCIE_PMON_BOX_STATUS,
	.ops		= &HSWEP_UNCORE_R2PCIEBOX_OPS
};

struct uncore_box_type HSWEP_UNCORE_R3QPI = {
	.name		= "R2PCIE-Box PCI Type",
	.num_counters	= 3,
	.num_boxes	= 2,
	.perf_ctr_bits	= 48,
	.perf_ctr	= HSWEP_PCI_R3QPI_PMON_CTR0,
	.perf_ctl	= HSWEP_PCI_R3QPI_PMON_CTL0,
	.event_mask	= 0,
	.box_ctl	= HSWEP_PCI_R3QPI_PMON_BOX_CTL,
	.box_status	= HSWEP_PCI_R3QPI_PMON_BOX_STATUS,
	.ops		= &HSWEP_UNCORE_R3QPIBOX_OPS
};

enum {
	HSWEP_UNCORE_PCI_HA_ID,
	HSWEP_UNCORE_PCI_IMC_ID,
	HSWEP_UNCORE_PCI_IRP_ID,
	HSWEP_UNCORE_PCI_QPI_ID,
	HSWEP_UNCORE_PCI_R2PCIE_ID,
	HSWEP_UNCORE_PCI_R3QPI_ID,
};

/* PCI Boxes */
struct uncore_box_type *HSWEP_UNCORE_PCI_TYPE[] = {
	[HSWEP_UNCORE_PCI_HA_ID]     = &HSWEP_UNCORE_HA,
	[HSWEP_UNCORE_PCI_IMC_ID]    = &HSWEP_UNCORE_IMC,
	[HSWEP_UNCORE_PCI_IRP_ID]    = &HSWEP_UNCORE_IRP,
	[HSWEP_UNCORE_PCI_QPI_ID]    = &HSWEP_UNCORE_QPI,
	[HSWEP_UNCORE_PCI_R2PCIE_ID] = &HSWEP_UNCORE_R2PCIE,
	[HSWEP_UNCORE_PCI_R3QPI_ID]  = &HSWEP_UNCORE_R3QPI,
	NULL
};

static const struct pci_device_id HSWEP_UNCORE_PCI_IDS[] = {
	{ /* Home Agent 0 */
		PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x2F30),
		.driver_data = UNCORE_PCI_DEV_DATA(HSWEP_UNCORE_PCI_HA_ID, 0),
	},
	{ /* Home Agent 1 */
		PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x2F38),
		.driver_data = UNCORE_PCI_DEV_DATA(HSWEP_UNCORE_PCI_HA_ID, 1),
	},
	{ /* MC0 Channel 0 */
		PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x2FB0),
		.driver_data = UNCORE_PCI_DEV_DATA(HSWEP_UNCORE_PCI_IMC_ID, 0),
	},
	{ /* MC0 Channel 1 */
		PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x2FB1),
		.driver_data = UNCORE_PCI_DEV_DATA(HSWEP_UNCORE_PCI_IMC_ID, 1),
	},
	{ /* MC0 Channel 2 */
		PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x2FB4),
		.driver_data = UNCORE_PCI_DEV_DATA(HSWEP_UNCORE_PCI_IMC_ID, 2),
	},
	{ /* MC0 Channel 3 */
		PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x2FB5),
		.driver_data = UNCORE_PCI_DEV_DATA(HSWEP_UNCORE_PCI_IMC_ID, 3),
	},
	{ /* MC1 Channel 0 */
		PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x2FD0),
		.driver_data = UNCORE_PCI_DEV_DATA(HSWEP_UNCORE_PCI_IMC_ID, 4),
	},
	{ /* MC1 Channel 1 */
		PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x2FD1),
		.driver_data = UNCORE_PCI_DEV_DATA(HSWEP_UNCORE_PCI_IMC_ID, 5),
	},
	{ /* MC1 Channel 2 */
		PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x2FD4),
		.driver_data = UNCORE_PCI_DEV_DATA(HSWEP_UNCORE_PCI_IMC_ID, 6),
	},
	{ /* MC1 Channel 3 */
		PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x2FD5),
		.driver_data = UNCORE_PCI_DEV_DATA(HSWEP_UNCORE_PCI_IMC_ID, 7),
	},
	{ /* IRP */
		PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x2F39),
		.driver_data = UNCORE_PCI_DEV_DATA(HSWEP_UNCORE_PCI_IRP_ID, 0),
	},
	{ /* QPI0 Port 0 */
		PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x2F32),
		.driver_data = UNCORE_PCI_DEV_DATA(HSWEP_UNCORE_PCI_QPI_ID, 0),
	},
	{ /* QPI0 Port 1 */
		PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x2F33),
		.driver_data = UNCORE_PCI_DEV_DATA(HSWEP_UNCORE_PCI_QPI_ID, 1),
	},
	{ /* R2PCIe */
		PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x2F34),
		.driver_data = UNCORE_PCI_DEV_DATA(HSWEP_UNCORE_PCI_R2PCIE_ID, 0),
	},
	{ /* R3QPI0 Link 0 */
		PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x2F36),
		.driver_data = UNCORE_PCI_DEV_DATA(HSWEP_UNCORE_PCI_R3QPI_ID, 0),
	},
	{ /* R3QPI0 Link 1 */
		PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x2F37),
		.driver_data = UNCORE_PCI_DEV_DATA(HSWEP_UNCORE_PCI_R3QPI_ID, 1),
	},
	{ /* All zero ;) */
		0,
	}
};

static struct pci_driver HSWEP_UNCORE_PCI_DRIVER = {
	.name		= "HSWEP UNCORE",
	.id_table	= HSWEP_UNCORE_PCI_IDS
};

void hswep_cpu_init(void)
{
	uncore_msr_type = HSWEP_UNCORE_MSR_TYPE;
}

void hswep_pci_init(void)
{
	uncore_pci_type = HSWEP_UNCORE_PCI_TYPE;
	uncore_pci_driver = &HSWEP_UNCORE_PCI_DRIVER;
}


