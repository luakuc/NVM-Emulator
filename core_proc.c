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

#define pr_fmt(fmt) "CORE PROC: " fmt

#include <asm/uaccess.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mutex.h>
#include <linux/percpu.h>

extern u64 pre_event_init_value;
DECLARE_PER_CPU(u64, PMU_EVENT_COUNT);

const char pmu_proc_format[] = "CPU %2d, NMI times = %lld\n";

static int core_pmu_proc_show(struct seq_file *m, void *v)
{
	int cpu;
	for_each_online_cpu(cpu) {
		seq_printf(m, pmu_proc_format, cpu,
			per_cpu(PMU_EVENT_COUNT, cpu));
	}
	
	return 0;
}

static int core_pmu_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, core_pmu_proc_show, NULL);
}

static void core_pmu_clear_counter(void)
{
	int cpu;
	for_each_online_cpu(cpu) {
		*per_cpu_ptr(&PMU_EVENT_COUNT, cpu) = 0;
	}
}

static DEFINE_MUTEX(core_pmu_proc_mutex);

/*
 * Control overflow threshold in a ugly way
 */
static ssize_t core_pmu_proc_write(struct file *file, const char __user *buf,
				   size_t count, loff_t *offs)
{
	char ctl[2];
	
	if (count !=2 || *offs)
		return -EINVAL;
	
	if (copy_from_user(ctl, buf, count))
		return -EFAULT;
	
	mutex_lock(&core_pmu_proc_mutex);
	switch (ctl[0]) {
		case '0': /* Clear per-cpu counter */
			core_pmu_clear_counter();
			break;
		case '1': /* -32 */
			pre_event_init_value = -32;
			break;
		case '2': /* -64 */
			pre_event_init_value = -64;
			break;
		case '3': /* -128 */
			pre_event_init_value = -128;
			break;
		case '4': /* -256 */
			pre_event_init_value = -256;
			break;
		default:
			count = -EINVAL;
	}
	mutex_unlock(&core_pmu_proc_mutex);

	return count;
}

const struct file_operations core_pmu_proc_fops = {
	.open		= core_pmu_proc_open,
	.read		= seq_read,
	.write		= core_pmu_proc_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

static bool is_proc_registed = false;

int __must_check core_pmu_proc_create(void)
{
	if (proc_create("core_pmu", 0644, NULL, &core_pmu_proc_fops)) {
		is_proc_registed = true;
		return 0;
	}

	return -ENOENT;
}

void core_pmu_proc_remove(void)
{
	if (is_proc_registed)
		remove_proc_entry("core_pmu", NULL);
}
