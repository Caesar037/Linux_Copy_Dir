#include "copy.h"

void *copy_file(void *arg)
{
	char *path_src = (char *)arg;
	char *path_dst = (char *)arg + (SPLICSIZE);

    int fd_src = open(path_src, O_RDONLY); //打开已存在的文件
    if(fd_src < 0)
	{
		perror("path_src");
		return NULL;
	}
    int fd_dst = open(path_dst, O_RDWR | O_CREAT | O_TRUNC, 0777); //新建新文件
    if(fd_dst < 0)
	{
		perror("open_dst");
		return NULL;
	}

	char buf[PATHSIZE] = {0};
	int size = 0;
    while(1) //拷贝,系统IO的拷贝，根据读取的字节数写入  
    {
        size = read(fd_src, buf, PATHSIZE);
        if(size <= 0)
			break;

        write(fd_dst, buf, size);
    }
	printf("copy file to %s\n", path_dst); //提示文件复制完毕

    close(fd_src);
	close(fd_dst);

    return NULL;
}

void *copy_dir(thread_pool * const pool, const char * const dir_src, const char * const dir_dst)
{
	char abs_ori[PATHSIZE] = {0};
	char abs_src[PATHSIZE] = {0};
	char abs_dst[PATHSIZE] = {0};

	//获取当前的路径	
	getcwd(abs_ori, PATHSIZE);

	//获取源文件的路径存在buf里面	
	chdir(dir_src);
	getcwd(abs_src, PATHSIZE);
	chdir(abs_ori);

	//创建目标路径
	mkdir(dir_dst, 0777);
	printf("create dir at %s\n", dir_dst); //提示目录复制完毕
	
	//获取目标路径存在buf里面
	chdir(dir_dst);
	getcwd(abs_dst, PATHSIZE);

	DIR *dir;
	struct dirent *data;

	dir = opendir(abs_src);


    while (1)
    {
        data = readdir(dir);
		
		if(data == NULL)
		{
			break; //读完退出  
		}

		if(data->d_name[0] == '.')
		{
			continue; //跳过隐藏文件和返回文件
		}

		if(data->d_type == 8) //文件
		{
			char buf[2][(SPLICSIZE)]; //拼接路径

			sprintf(buf[0], "%s/%s", abs_src, data->d_name);
			sprintf(buf[1], "%s/%s", abs_dst, data->d_name);

			add_task(pool, copy_file, (void *)buf);
			sleep(1);
		}

        if(data->d_type == 4) //目录
        {
			char path_src[(SPLICSIZE)] = {0}, path_dst[(SPLICSIZE)] = {0}; //拼接路径

			sprintf(path_src, "%s/%s", abs_src, data->d_name);
            sprintf(path_dst, "%s/%s", abs_dst, data->d_name);

			copy_dir(pool, path_src, path_dst);
        }
    }

	closedir(dir);
	return NULL;
}