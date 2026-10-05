// SPDX-License-Identifier: GPL-2.0
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#ifdef CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG
/*
 * Hide the unlocked/AVB state from ordinary apps: they read /proc/cmdline and
 * would otherwise see androidboot.verifiedbootstate=orange.  Only apps
 * (uid >= 2000) get the sanitised line; init/root and system daemons keep the
 * real one so the framework behaviour is unchanged.
 */
static const char *susfs_spoof_find(const char *s, const char *pat)
{
	return strstr(s, pat);
}
#endif

static int cmdline_proc_show(struct seq_file *m, void *v)
{
#ifdef CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG
	if (current_uid().val >= 2000) {
		const char *pat = "androidboot.verifiedbootstate=";
		const char *hit = susfs_spoof_find(saved_command_line, pat);

		if (hit) {
			const char *val = hit + strlen(pat);

			seq_write(m, saved_command_line, hit - saved_command_line);
			seq_puts(m, pat);
			seq_puts(m, "green");
			if (!strncmp(val, "orange", 6))
				val += 6;
			else
				while (*val && *val != ' ')
					val++;
			seq_puts(m, val);
			seq_putc(m, '\n');
			return 0;
		}
	}
#endif
	seq_puts(m, saved_command_line);
	seq_putc(m, '\n');
	return 0;
}

static int __init proc_cmdline_init(void)
{
	proc_create_single("cmdline", 0, NULL, cmdline_proc_show);
	return 0;
}
fs_initcall(proc_cmdline_init);
