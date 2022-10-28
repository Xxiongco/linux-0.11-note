/*
 *  linux/fs/file_table.c
 *
 *  (C) 1991  Linus Torvalds
 */

#include <linux/fs.h>

// 文件表数组， 系统同时只能打开64个文件
struct file file_table[NR_FILE];
