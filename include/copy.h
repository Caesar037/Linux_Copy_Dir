#ifndef _COPY_H_
#define _COPY_H_

#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <dirent.h>

#include "thread_pool.h"

#define PATHSIZE 1024
#define SPLICSIZE 1024+256


void *copy_file(void *arg);
void *copy_dir(thread_pool * const pool, const char * const dir_src, const char * const dir_dst);

#endif