/*
 * THIS FILE IS NOT INCLUDED IN Makefile
 * We left this file only for debugging purpose
 */

#include "file_handler.h"
#include "network_server.h"
#include "memory_management.h"
#include "request_handler.h"
#include <linux/string.h>

static char file_dir[] = "~/mem_mode_profiles/";
static char file_pid_name[] = "mem_node_profile";

void open_file(struct file_info *finfo, unsigned int tgid, char* comm)
{
    char file_name[255] = {0};
    mm_segment_t old_fs;
    struct timeval t;
    struct tm broken;

    if (!finfo || !comm)
    {
        pr_info("** open_file - NULL finfo or program name\n");
        return;
    }

    do_gettimeofday(&t);
    time_to_tm(t.tv_sec, 0, &broken);

    sprintf(file_name, "%s%s_%04ld%02d%02d_%02d%02d%02d_P%u_%s.log", 
                file_dir, file_pid_name, 
                broken.tm_year, broken.tm_mon, broken.tm_mday, 
                broken.tm_hour, broken.tm_min, broken.tm_sec,
                tgid, skip_spaces(comm));
    pr_info("Try to open file: %s\n", file_name);
    barrier();

    old_fs = get_fs();
    set_fs(get_ds());
    finfo->file = filp_open(file_name, O_WRONLY|O_CREAT, 0644);
    set_fs(old_fs);
    if (IS_ERR(finfo->file))
    {
        pr_info("** open_file - cannot open file: 0x%lx\n", -(unsigned long)finfo->file);
        finfo->file = NULL;
        finfo->fd = -1;
    }
    finfo->pos = 0;
}

void reopen_file(struct file_info *finfo, unsigned int tgid, char* comm)
{
    if (finfo && finfo->file)
    {
        close_file(finfo);
        open_file(finfo, tgid, comm);
    }
}

void close_file(struct file_info *finfo)
{
    if (!finfo)
    {
        pr_info("** close_file - NULL finfo\n");
        return;
    }

    if (finfo->file)
    {
        filp_close(finfo->file, NULL);
        finfo->file = NULL;
    }
    finfo->fd = -1;
}

void write_file(struct file_info *finfo, char* data)
{
    (void)finfo;
    (void)data;
    // TODO: write into the file for debugging
}
