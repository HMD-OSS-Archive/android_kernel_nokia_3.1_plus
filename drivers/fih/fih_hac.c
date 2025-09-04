#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <fih/hwid.h>
#include <linux/seq_file.h>

static int fih_info_proc_hac_show(struct seq_file *m, void *v)
{
//	char msg[8], msg1[256];

//	strcpy(msg, "A1N");
//	strcpy(msg1, "G_850_900_1800_1900^W_1_2_5_8^L_1_2_3_4_5_7_8_12_13_17_20_28_38_40_41_66");
//	seq_printf(m, "%s-%s\n", msg, msg1);

	return 0;
}

static int fih_info_proc_hac(struct inode *inode, struct file *file)
{
	return single_open(file, fih_info_proc_hac_show, NULL);
}

/* This structure gather "function" that manage the /proc file
 */
static const struct file_operations hac_file_ops = {
	.owner   = THIS_MODULE,
	.open	 = fih_info_proc_hac,
	.read    = seq_read
};

static int __init fih_hac_init(void)
{
  //A1N US band support Hac
//  if ((fih_hwid_fetch(FIH_HWID_PRJ) == FIH_PRJ_A1N) 
//  	&& (fih_hwid_fetch(FIH_HWID_RF) == FIH_BAND_G_850_900_1800_1900_W_1_2_5_8_L_1_2_3_4_5_7_8_12_13_17_20_28_38_40_41_66_SS))
  {
	  if (proc_create("Hac", 0, NULL, &hac_file_ops) == NULL) {
		  pr_err("fail to create proc/Hac\n");
	  }
  }
	return (0);
}

static void __exit fih_hac_exit(void)
{
//  if ((fih_hwid_fetch(FIH_HWID_PRJ) == FIH_PRJ_A1N) 
//  	&& (fih_hwid_fetch(FIH_HWID_RF) == FIH_BAND_G_850_900_1800_1900_W_1_2_5_8_L_1_2_3_4_5_7_8_12_13_17_20_28_38_40_41_66_SS))
  {
	  remove_proc_entry("Hac", NULL);
  }
}

module_init(fih_hac_init);
module_exit(fih_hac_exit);
