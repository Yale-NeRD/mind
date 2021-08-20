#ifndef __MN_FILE_HANDLER_H__
#define __MN_FILE_HANDLER_H__

#include <linux/sched.h>
#include <linux/types.h>
#include <linux/file.h>
#include <linux/fs.h>
//#include <linux/buffer_head.h>

#include <asm/segment.h>
#include <asm/uaccess.h>


struct file_info
{
    struct file *file;
    loff_t pos;
    int fd;
};

void open_file(struct file_info *finfo, unsigned int tgid, char* comm);
void reopen_file(struct file_info *finfo, unsigned int tgid, char* comm);
void close_file(struct file_info *finfo);
void write_file(struct file_info *finfo, char *data);

#endif  /* __MN_FILE_HANDLER_H__ */