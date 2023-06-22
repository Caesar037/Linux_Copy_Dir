#include "copy.h"
#include "thread_pool.h"


int main(int argc, char **argv)
{
	//格式判断
    if (argc != 3)
    {
        printf("format: copy <src> <dst>\n");
        return -1;
    }

	//源路径必须存在
	if (access(argv[1], F_OK) != 0)
	{
        printf("src not find\n");
        return -1;
	}
	
	thread_pool pool;
  	struct stat buf; 
	stat(argv[1], &buf);

   	if(S_IFDIR & buf.st_mode) //src为目录
	{ 
		//1.初始化线程池
		init_pool(&pool, 5);
		
		//2.递归扫描目录
		copy_dir(&pool, argv[1], argv[2]);
		
		//3.销毁线程池
		destroy_pool(&pool);

  	}else if(S_IFREG & buf.st_mode) //src为文件
	{ 
		//直接复制文件
		char arg[2][(SPLICSIZE)]; //拼接路径
		strcpy(arg[0], argv[1]);
		strcpy(arg[1], argv[2]);

    	copy_file((void *)arg);
  	} 
	
	
	return 0;
}
