#ifndef MODULE_NAME
#define MODULE_NAME "profile_printer"
#endif

#include <linux/module.h> /* Needed by all modules */
#include <linux/kernel.h> /* Needed for KERN_ALERT */
#include "../../include/disagg/profile_points_disagg.h"

MODULE_LICENSE("GPL");

static int __init profile_printer_init(void)
{
	pr_info("Profile printer inserted... Let's print profile result (for now)\n");
	print_profile_points();
	return 0;
}

static void __exit profile_printer_exit(void)
{
	;
}

/* module init and exit */
module_init(profile_printer_init)
module_exit(profile_printer_exit)
