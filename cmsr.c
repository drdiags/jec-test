/*
 * Copyright Cray, Inc. 2013-2016
 * A module to provide configurable userspace access to MSRs.
 */

#include <asm/cpu.h>
#include <asm/processor.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/sysfs.h>
#include <linux/version.h>

#define VEC_LENGTH	64

/*
 * Crude attempt at supporting SP2 kernel if needed in a rush
 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,1,0))
#define get_cpu_device(cpu) get_cpu_sysdev(cpu)
#endif

/*
 * The central struct to stitch individual MSR access in to the
 * attributes added to the "msr" directory.  Each time sysfs
 * infrastructure calls back to show/store a file this object will
 * allow us to identify which MSR is involved.
 */
typedef struct msr_attr mattr_t;
struct msr_attr {
	struct attribute ma_attr;
	ssize_t (*show)(struct kobject *kobj, struct attribute *attr,
			char *buf);
	ssize_t (*store)(struct kobject *kobj, struct attribute *attr,
			 const char *buf, size_t count);
	uint32_t ma_addr;
	char ma_name[16];
};

/*
 * Each cpu<X> directory will have an "msr" directory created which
 * will house the ASCII files and the binary blob file.  Since we're
 * mixing both, we'll have to create kobjects to parent the attributes
 * manually.  This struct is how we'll do it.
 */
typedef struct cpu_data cdata_t;
struct cpu_data {
	struct kobject cd_kobj;
	unsigned int cd_idx;
	char cd_good;
};

static cdata_t *cdata;

/* 
 * Convert field pointers as passed into the callbacks, to container
 * pointers for the above structures.
 */
#define to_cdata(p) container_of((p), cdata_t, cd_kobj)
#define to_mattr(p) container_of((p), mattr_t, ma_attr)

/*
 * This is the list of value passed in at load time to be exposed as
 * writable attributes.
 */
#ifdef CONFIG_CRAY_GEMINI
static uint rw_vec[VEC_LENGTH];
static int rw_cnt;
#else
static uint rw_vec_snb[VEC_LENGTH];
static uint rw_vec_ivb[VEC_LENGTH];
static uint rw_vec_hsw[VEC_LENGTH];
static uint rw_vec_bdw[VEC_LENGTH];
static uint rw_vec_knl[VEC_LENGTH];
static uint rw_vec_skl[VEC_LENGTH];
static int rw_cnt_snb;
static int rw_cnt_ivb;
static int rw_cnt_hsw;
static int rw_cnt_bdw;
static int rw_cnt_knl;
static int rw_cnt_skl;

static uint *rw_vec = NULL;
static int rw_cnt = 0;
#endif /* CONFIG_CRAY_GEMINI */

/*
 * This is the list of value passed in at load time to be exposed,
 * we'll default to a harmless TSC MSR.
 */
#ifdef CONFIG_CRAY_GEMINI
static uint ro_vec[VEC_LENGTH];
static int ro_cnt;

module_param_array(rw_vec, uint, &rw_cnt, S_IRUSR);
MODULE_PARM_DESC(rw_vec, "An array of up to 64 MSR addresses to make "
		 "visible and writable");
module_param_array(ro_vec, uint, &ro_cnt, S_IRUSR);
MODULE_PARM_DESC(ro_vec, "An array of up to 64 MSR addresses to make "
		 "available for reading");
#else
static uint ro_vec_snb[VEC_LENGTH];
static uint ro_vec_ivb[VEC_LENGTH];
static uint ro_vec_hsw[VEC_LENGTH];
static uint ro_vec_bdw[VEC_LENGTH];
static uint ro_vec_knl[VEC_LENGTH];
static uint ro_vec_skl[VEC_LENGTH];
static int ro_cnt_snb;
static int ro_cnt_ivb;
static int ro_cnt_hsw;
static int ro_cnt_bdw;
static int ro_cnt_knl;
static int ro_cnt_skl;

static uint *ro_vec = NULL;
static int ro_cnt = 0;

module_param_array(rw_vec_snb, uint, &rw_cnt_snb, S_IRUSR);
MODULE_PARM_DESC(rw_vec_snb, "An array of up to 64 MSR addresses to make "
		 "visible and writable for SandyBridge CPUs");
module_param_array(ro_vec_snb, uint, &ro_cnt_snb, S_IRUSR);
MODULE_PARM_DESC(ro_vec_snb, "An array of up to 64 MSR addresses to make "
		 "available for reading for SandyBridge CPUs");

module_param_array(rw_vec_ivb, uint, &rw_cnt_ivb, S_IRUSR);
MODULE_PARM_DESC(rw_vec_ivb, "An array of up to 64 MSR addresses to make "
		 "visible and writable for IvyBridge CPUs");
module_param_array(ro_vec_ivb, uint, &ro_cnt_ivb, S_IRUSR);
MODULE_PARM_DESC(ro_vec_ivb, "An array of up to 64 MSR addresses to make "
		 "available for reading for IvyBridge CPUs");

module_param_array(rw_vec_hsw, uint, &rw_cnt_hsw, S_IRUSR);
MODULE_PARM_DESC(rw_vec_hsw, "An array of up to 64 MSR addresses to make "
		 "visible and writable for Haswell CPUs");
module_param_array(ro_vec_hsw, uint, &ro_cnt_hsw, S_IRUSR);
MODULE_PARM_DESC(ro_vec_hsw, "An array of up to 64 MSR addresses to make "
		 "available for reading for Haswell CPUs");

module_param_array(rw_vec_bdw, uint, &rw_cnt_bdw, S_IRUSR);
MODULE_PARM_DESC(rw_vec_bdw, "An array of up to 64 MSR addresses to make "
		 "visible and writable for Broadwell CPUs");
module_param_array(ro_vec_bdw, uint, &ro_cnt_bdw, S_IRUSR);
MODULE_PARM_DESC(ro_vec_bdw, "An array of up to 64 MSR addresses to make "
		 "available for reading for Broadwell CPUs");

module_param_array(rw_vec_knl, uint, &rw_cnt_knl, S_IRUSR);
MODULE_PARM_DESC(rw_vec_knl, "An array of up to 64 MSR addresses to make "
		 "visible and writable for KNL CPUs");
module_param_array(ro_vec_knl, uint, &ro_cnt_knl, S_IRUSR);
MODULE_PARM_DESC(ro_vec_knl, "An array of up to 64 MSR addresses to make "
		 "available for reading for KNL CPUs");

module_param_array(rw_vec_skl, uint, &rw_cnt_skl, S_IRUSR);
MODULE_PARM_DESC(rw_vec_skl, "An array of up to 64 MSR addresses to make "
		 "visible and writable for Skylake CPUs");
module_param_array(ro_vec_skl, uint, &ro_cnt_skl, S_IRUSR);
MODULE_PARM_DESC(ro_vec_skl, "An array of up to 64 MSR addresses to make "
		 "available for reading for Skylake CPUs");
#endif /* CONFIG_CRAY_GEMINI */

/*
 * CPUID_* - Model/Family/Type/Model bits from the CPUID that we care about.
 */
#define CPUID_SANDYBRIDGE			0x000206d0
#define CPUID_IVYBRIDGE				0x000306e0
#define CPUID_HASWELL				0x000306f0
#define CPUID_BROADWELL				0x000406f0
#define CPUID_KNL				0x00050670 
#define CPUID_SKYLAKE				0x00050650
#define CPUID_MASK				0x000ffff0

/*
 * Cached CPUID of this node.
 */
static unsigned int my_cpuid = 0;

/*
 * is_snb - Returns true if running on a SandyBridge node.
 */
static inline int is_snb(void)
{
	return ((my_cpuid & CPUID_MASK) == CPUID_SANDYBRIDGE);
}

/*
 * is_ivb - Returns true if running on a IvyBridge node.
 */
static inline int is_ivb(void)
{
	return ((my_cpuid & CPUID_MASK) == CPUID_IVYBRIDGE);
}

/*
 * is_hsw - Returns true if running on a Haswell node.
 */
static inline int is_hsw(void)
{
	return ((my_cpuid & CPUID_MASK) == CPUID_HASWELL);
}

/*
 * is_bdw - Returns true if running on a Broadwell node.
 */
static inline int is_bdw(void)
{
	return ((my_cpuid & CPUID_MASK) == CPUID_BROADWELL);
}

/*
 * is_knl - Returns true if running on a KNL node.
 */
static inline int is_knl(void)
{
	return ((my_cpuid & CPUID_MASK) == CPUID_KNL);
}

/*
 * is_skl - Returns true if running on a Skylake node.
 */
static inline int is_skl(void)
{
	return ((my_cpuid & CPUID_MASK) == CPUID_SKYLAKE);
}

/*
 * The ASCII representation of a single MSR.
 */
static ssize_t show_msr(struct kobject *kobj,
			struct attribute *attr,
			char *buf)
{
	cdata_t *cdata = to_cdata(kobj);
	mattr_t *mattr = to_mattr(attr);

	unsigned long value;

	u32 cpu = cdata->cd_idx,
		*data = (u32 *)&value,
		addr = mattr->ma_addr;
	int res;

	res = rdmsr_safe_on_cpu(cpu, addr, &data[0], &data[1]);
	if (res) {
		printk (KERN_WARNING "%s: (%p) Reading MSR %#x on cpu %d failed: %d",
			__FUNCTION__, kobj, addr, cpu, res);
		res = -ENXIO;
	} else {
		res = scnprintf(buf, PAGE_SIZE, "%#lx\n", value);
	}

	return res;
}

/*
 * Update a single MSR by conversion of the ASCII string provided
 */
static ssize_t store_msr(struct kobject *kobj,
			 struct attribute *attr,
			 const char *buf, size_t cnt)
{
	cdata_t *cdata = to_cdata(kobj);
	mattr_t *mattr = to_mattr(attr);
	unsigned long value;
	u32 cpu = cdata->cd_idx,
		*data = (u32 *)&value,
		addr = mattr->ma_addr;
	int res;

	res = sscanf(buf, "0x%lx", &value);
	if (!res) {
		res = sscanf(buf, "%ld", &value);
	}

	if (1 != res) {
		return -EINVAL;
	}

	res = wrmsr_safe_on_cpu(cpu, addr, data[0], data[1]);
	if (res) {
		printk (KERN_WARNING
			"%s: Writing %#lx to MSR %#x on cpu %d failed: %d",
			__FUNCTION__, value, addr, cpu, res);
	}

	return strlen(buf)+1;
}

/*
 * The aggregation of all the ASCII files to be added to the "msr" directory.
 */
static struct attribute_group msr_attr_group;

/*
 * The "record" structure of the binary file.  A long for the address
 * followed by a long for the value.  This "fast" access to a number
 * of MSRs can be gained by opening the binary file, searching for the
 * MSR addresses of interest and writing 8 byte values to the
 * corresponding addresses.  This avoids opening multiple files and
 * doing ASCII translations.
 */
typedef struct {
    unsigned long cr_msr;
    union {
	unsigned long cr_value;
	u32 cr_word[2];
    };
} __attribute__((packed)) cbi_rec_t;

/*
 * Output callback for a range of the binary file.  The "offset div
 * sizeof cbi_rec_t" provides the index into the rw_vec of MSRs we're
 * exposing.  We work with one MSR at a time to provide up to 16 bytes
 * of data to the caller, repeating as often as needed to fill the
 * request.  The ro_vec MSRs are "appended" to the rw_vec in offset
 * range.
 */
static ssize_t
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0))
read_cbi(struct kobject *kobj, struct bin_attribute *attr,
	 char *buff, loff_t offs, size_t size)
#else
read_cbi(struct file *f, struct kobject *kobj, struct bin_attribute *attr,
	 char *buff, loff_t offs, size_t size)
#endif
{
	cdata_t *cdata = to_cdata(kobj);
	cbi_rec_t scratch;

	int mi = offs / sizeof scratch,
		output = offs % sizeof scratch,
		cnt = 0, res;

	while (size && mi < (rw_cnt + ro_cnt)) {
		int rem;

		scratch.cr_msr = mi < rw_cnt ? rw_vec[mi] : ro_vec[mi - rw_cnt];
		res = rdmsr_safe_on_cpu(cdata->cd_idx, scratch.cr_msr,
					&scratch.cr_word[0],
					&scratch.cr_word[1]);
		if (res) {
			cnt = -ENXIO;
			break;
		}

		rem = sizeof scratch - output;
		if (rem > size) {
			rem = size;
		}

		memcpy(buff, (char *)&scratch + output, rem);
		size -= rem;
		cnt += rem;
		++mi;
		output = 0;
		buff += rem;
	}

	return cnt;
};

/*
 * The input callback for the binary file.  We only accept 8 byte
 * writes to the "value" half of the cbi_rec_t records in the file and
 * simply use the same index as the read side to identify which MSR is
 * the target.  The ro_vec isn't appended but a check is made to
 * return -EPERM if the index would be to an offset that read would
 * return for ro_vec.
 */
static ssize_t
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0))
write_cbi(struct kobject *kobj, struct bin_attribute *attr,
	  char *buff, loff_t offs, size_t size)
#else
write_cbi(struct file *f, struct kobject *kobj, struct bin_attribute *attr,
	  char *buff, loff_t offs, size_t size)
#endif
{
	cdata_t *cdata = to_cdata(kobj);
	cbi_rec_t scratch;

	int mi = offs / sizeof scratch,
		output = offs % sizeof scratch,
		res;

	if (output != sizeof scratch.cr_msr ||
	    size != sizeof scratch.cr_value) {
	    return -EINVAL;
	}

	if (mi >= rw_cnt) {
	    return -EPERM;
	}

	memcpy(scratch.cr_word, buff, size);
	res = wrmsr_safe_on_cpu(cdata->cd_idx, rw_vec[mi],
				scratch.cr_word[0],scratch.cr_word[1]);

	if (res) {
		printk (KERN_WARNING
			"%s: Writing %#lx to MSR %#x on cpu %d failed: %d",
			__FUNCTION__, scratch.cr_value, rw_vec[mi],
			cdata->cd_idx, res);
	}

	return res ? -EIO : size;
}

/*
 * Initialize a struct suitable to add the binary file to the "msr"
 * kobject we'll call it "cbi" amd make it group/world writable.
 */
static struct bin_attribute cbi_attr = {
	.attr = {
		.name = "cbi",
		.mode = S_IRUGO | S_IWUGO,
	},
	.read = read_cbi,
	.write = write_cbi,
};

/*
 * Search for requested msr in ro_vec.
 */
static inline int ro_check(loff_t msr) {
	int i;
	
	for (i = 0; i < ro_cnt; ++i) {
		if (msr == ro_vec[i]) 
			return i;
		
	}
	
	return -1;
}

/*
 * Output callback for a range of approved read only msrs binary file. If the
 * requested offset is available in the ro_vec the data will be fetched in a
 * manner similar to how the /dev/cpu/<num>/msr functions.  If the requested
 * msr offset is not in the ro_vec it will return an EIO error, like how an 
 * invalid rdmsr_safe_on_cpu() would pass back the EIO. 
 */
static ssize_t
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0))
read_ro_msr(struct kobject *kobj, struct bin_attribute *attr,
	 char *buff, loff_t offs, size_t size)
#else
read_ro_msr(struct file *f, struct kobject *kobj, struct bin_attribute *attr,
	 char *buff, loff_t offs, size_t size)
#endif
{
	static int quantum = sizeof(u64);
	ssize_t bytes = 0;
	int cpu = to_cdata(kobj)->cd_idx;
	int res = 0;
	
	
	if (size % quantum)
		return -EINVAL;
	
	if (ro_check(offs) < 0) 
		return -EIO;

	while (size) {
		res = rdmsr_safe_on_cpu(cpu, offs,
					(u32 *)buff,
					(u32 *)(buff + (quantum / 2)));
		if (res) 
			break;

		bytes += quantum;
		buff += quantum;
		size -= quantum;
	}

	return bytes ? bytes : res;
}


/*
 * Input CallBack, just in case its requested in the future.
 */
static ssize_t
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0))
write_ro_msr(struct kobject *kobj, struct bin_attribute *attr,
	  char *buff, loff_t offs, size_t size)
#else
write_ro_msr(struct file *f, struct kobject *kobj, struct bin_attribute *attr,
	  char *buff, loff_t offs, size_t size)
#endif
{
	return -EIO;
}	

static struct bin_attribute ro_msr_attr = {
	.attr = {
		.name = "ro_msr",
		.mode = S_IRUGO,
	},
	.read = read_ro_msr,
	.write = write_ro_msr,
};

/*
 * This and ktype are just so we can create the kobjects we're using
 * to track the CPUs
 */
static struct sysfs_ops sysfs_ops = {
	.show	= show_msr,
	.store	= store_msr,
};

static struct kobj_type ktype = {
	.sysfs_ops	= &sysfs_ops,
};

/*
 * Initialize the aggregation of ASCII files ready for association
 * with each CPU.  We name the files after their address and set the
 * modes here, readonly names have "r" appended to prevent name
 * clashes.  They'll be instantiated per CPU later.
 */
static int
make_attributes(void)
{
	int i, allocsz = sizeof (mattr_t *)
		+ sizeof (mattr_t);
	mattr_t *attr;

	if (rw_cnt < 0 || rw_cnt > VEC_LENGTH
	    || ro_cnt < 0 || ro_cnt > VEC_LENGTH) {
		return -EINVAL;
	}

	msr_attr_group.attrs = kzalloc((ro_cnt + rw_cnt + 1) * allocsz,
				       GFP_KERNEL);
	if (!msr_attr_group.attrs) {
	    return -ENOMEM;
	}

	attr = (mattr_t *)&msr_attr_group.attrs[ro_cnt + rw_cnt + 1];
	for (i = 0; i < (ro_cnt + rw_cnt); ++attr, ++i) {
		attr->ma_attr.name = attr->ma_name;
		attr->ma_attr.mode = i < rw_cnt ? 0666 : 0444;
		attr->show = show_msr;
		attr->store = i < rw_cnt ? store_msr : 0;
		attr->ma_addr = i < rw_cnt ? rw_vec[i] : ro_vec[i - rw_cnt];
		scnprintf(attr->ma_name, sizeof attr->ma_name,
			  i < rw_cnt ? "%x" : "%xr",
			  i < rw_cnt ? rw_vec[i] : ro_vec[i - rw_cnt]);
		msr_attr_group.attrs[i] = (struct attribute *)attr;
	}

	cbi_attr.size = sizeof (cbi_rec_t) * (ro_cnt + rw_cnt);

	return 0;
}

/*
 * If we "lose" a CPU, either from offlining or module exit, we remove
 * all the files, binary and ASCII, then put the object so it gets
 * cleaned up.
 */
static void msr_remove_dev(cdata_t *cpu)
{
	if (cpu->cd_good) {
		sysfs_remove_bin_file(&cpu->cd_kobj, &ro_msr_attr);
		sysfs_remove_bin_file(&cpu->cd_kobj, &cbi_attr);
		sysfs_remove_group(&cpu->cd_kobj, &msr_attr_group);

		kobject_put(&cpu->cd_kobj);

		cpu->cd_good = 0;
	}
}

/*
 * Similarly, for each CPU we're going to provide MSRs for, we create
 * a kobject to host the attributes and add them if successful.
 */
static int msr_add_dev(cdata_t *cpu)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0)
	struct sys_device *dev = get_cpu_device(cpu->cd_idx);
#else
	struct device *dev = get_cpu_device(cpu->cd_idx);
#endif
	int res;

	if(!dev) {
		return -ENODEV;
	}

	res = kobject_init_and_add(&cpu->cd_kobj, &ktype, &dev->kobj, "msr");
	cpu->cd_good = (0 == res);
	if (res) {
		return -ENODEV;
	}

	res = sysfs_create_group(&cpu->cd_kobj, &msr_attr_group);

	if (!res) {
	    res = sysfs_create_bin_file(&cpu->cd_kobj, &cbi_attr);
	}

	if (!res && (ro_cnt > 0)) {
	    res = sysfs_create_bin_file(&cpu->cd_kobj, &ro_msr_attr);
	}

	if (res) {
	    msr_remove_dev(cpu);
	}

	return res;
}

/*
 * Called at module init/initcall fairly straightforward
 * initialisation then a walk through online CPUs to add the "msr"
 * tree to each.
 */
static int __init msr_init(void)
{
	int cpu, retval = 0;

#ifndef CONFIG_CRAY_GEMINI
	/*
	 * Figure out which CPU we're on and which MSRs to expose.
	 */
	my_cpuid = cpuid_eax(1);

	if (is_snb()) {
		rw_vec = rw_vec_snb;
		rw_cnt = rw_cnt_snb;
		ro_vec = ro_vec_snb;
		ro_cnt = ro_cnt_snb;
	} else if (is_ivb()) {
		rw_vec = rw_vec_ivb;
		rw_cnt = rw_cnt_ivb;
		ro_vec = ro_vec_ivb;
		ro_cnt = ro_cnt_ivb;
	} else if (is_hsw()) {
		rw_vec = rw_vec_hsw;
		rw_cnt = rw_cnt_hsw;
		ro_vec = ro_vec_hsw;
		ro_cnt = ro_cnt_hsw;
	} else if (is_bdw()) {
		rw_vec = rw_vec_bdw;
		rw_cnt = rw_cnt_bdw;
		ro_vec = ro_vec_bdw;
		ro_cnt = ro_cnt_bdw;
	} else if (is_knl()) {
		rw_vec = rw_vec_knl;
		rw_cnt = rw_cnt_knl;
		ro_vec = ro_vec_knl;
		ro_cnt = ro_cnt_knl;
	} else if (is_skl()) {
		rw_vec = rw_vec_skl;
		rw_cnt = rw_cnt_skl;
		ro_vec = ro_vec_skl;
		ro_cnt = ro_cnt_skl;
	} else {
		printk (KERN_ALERT "%s: Invalid CPUID=0x%x detected\n",
			__FUNCTION__, my_cpuid);
		return -EACCES;
	}
#endif /* !CONFIG_CRAY_GEMINI */

	retval = make_attributes();

	if (!retval) {
		cdata = kzalloc(sizeof *cdata * num_possible_cpus(),
				GFP_KERNEL);
		if (!cdata) {
			retval = -ENOMEM;
		} else {
			for_each_online_cpu(cpu) {
				cdata[cpu].cd_idx = cpu;
				retval = msr_add_dev(cdata + cpu);

				if (retval) {
					break;
				}
			}
		}
	}

	return retval;
}

/*
 * Pack it up we're done
 */
static void __exit msr_exit(void)
{
	int cpu;

	for_each_online_cpu(cpu) {
		msr_remove_dev(cdata + cpu);
	}

	kfree(msr_attr_group.attrs);
	kfree(cdata);
}

module_init(msr_init);
module_exit(msr_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anthony Booker <tb@cray.com>");
MODULE_DESCRIPTION("A module to expose specific MSRs to userspace via sysfs");
