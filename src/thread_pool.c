#include "thread_pool.h"

void handler(void *arg)				
{
	pthread_mutex_unlock((pthread_mutex_t *)arg); //防止死锁，所以在这里添加解锁操作
}

/* 每一个线程池中的线程所执行的内容， arg就是线程池的地址 */
void *routine(void *arg)
{
	thread_pool *pool = (thread_pool *)arg;		//将线程池的地址存放进去pool	
	struct task *p;								//定义一个缓冲指针，后期任务队列遍历的时候使用

	while(1)
	{
		pthread_cleanup_push(handler, (void *)&pool->lock); //提前登记线程被取消后需要处理的事情
		pthread_mutex_lock(&pool->lock);					//由于需要操作线程池中的共有资源，所以加锁


		while(pool->waiting_tasks == 0 && pool->shutdown == false)	//判断是否没有需要运行的任务		
		{
			pthread_cond_wait(&pool->cond, &pool->lock);	//让线程睡眠		
		}


		if(pool->waiting_tasks == 0 && pool->shutdown == true)	//判断线程池是否没有任务并且需要关闭	
		{
			pthread_mutex_unlock(&pool->lock);	//解锁
			pthread_exit(NULL);					//退出线程					
		}


		p = pool->task_list->next;			//让p登记需要运行的任务节点						
		pool->task_list->next = p->next;	//将此任务节点从链表中删除							
		pool->waiting_tasks--;				//将等待运行的任务队列-1								

		pthread_mutex_unlock(&pool->lock);	//解锁						
		pthread_cleanup_pop(0);				//解除登记取消线程之后所做的函数									

		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);		//忽略线程的取消操作
		(p->task)(p->arg);											//函数调用										
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);		//重启线程取消操作

		free(p);													//释放任务节点
	}

	pthread_exit(NULL);												
}

/* pool：线程池结构体的地址，threads_number：一开始初始化的线程数量 */
bool init_pool(thread_pool *pool, unsigned int threads_number)			
{
	
	pthread_mutex_init(&pool->lock, NULL);			//初始化互斥锁				
	pthread_cond_init(&pool->cond, NULL);			//初始化条件变量				

	pool->shutdown = false;							//开启线程池的标志false：开	
	pool->task_list = malloc(sizeof(struct task));	//申请任务链表头					
	pool->tids = malloc(sizeof(pthread_t) * MAX_ACTIVE_THREADS); //申请最大线程数量的ID内存	

	if(pool->task_list == NULL || pool->tids == NULL) //判断两个申请的内存是否成功
	{
		perror("allocate memory error");
		return false;
	}

	pool->task_list->next = NULL; //初始化链表头，将下一个节点指向NULL

	pool->waiting_tasks = 0;	  //将等待运行的任务数量置0
	pool->active_threads = threads_number; //登记当前的线程数量

	int i;
	for(i=0; i<pool->active_threads; i++)				//创建线程池里面的线程
	{
		if(pthread_create(&((pool->tids)[i]), NULL,
					routine, (void *)pool) != 0)		//每一个线程都去跑routine这个函数的内容
		{
			perror("create threads error");
			return false;
		}
	}

	return true;
}


/* 投放任务：pool：线程池地址；task：任务需要运行的内容的函数指针； arg：传入给task函数的参数 */
bool add_task(thread_pool *pool,
			void *(*task)(void *arg), void *arg)
{
	struct task *new_task = malloc(sizeof(struct task));	//新建一个任务节点
	if(new_task == NULL)
	{
		perror("allocate memory error");
		return false;
	}
	new_task->task = task;								//将任务需要做的函数存进task指针中
	new_task->arg = arg;								//将任务函数参数记录在arg里面
	new_task->next = NULL;								//将任务节点的下一个位置指向NULL


	pthread_mutex_lock(&pool->lock);					//上锁
	if(pool->waiting_tasks >= MAX_WAITING_TASKS)		//判断任务数量有没有超标
	{
		pthread_mutex_unlock(&pool->lock);				//解锁

		fprintf(stderr, "too many tasks.\n");			//反馈太多任务了
		free(new_task);									//释放掉刚才登记的任务节点

		return false;									//返回添加不了任务到任务链表中
	}
	
	struct task *tmp = pool->task_list;					//将线程池中任务链表的头节点登记到tmp
	while(tmp->next != NULL)							//将tmp指向最后的节点的位置
		tmp = tmp->next;

	tmp->next = new_task;								//将新建的任务节点插入到链表中
	pool->waiting_tasks++;								//将等待的任务数量+1


	pthread_mutex_unlock(&pool->lock);					//解锁
	pthread_cond_signal(&pool->cond);					//唤醒正在睡眠中的线程

	return true;										//返回添加成功
}

/* 添加线程到线程池中 pool：线程池结构体地址， additional_threads:添加的线程的数量 */
int add_thread(thread_pool *pool, unsigned additional_threads)
{
	if(additional_threads == 0)
		return 0;

	unsigned total_threads =
		     pool->active_threads + additional_threads;	//将总数记录在这个变量中

	int i, actual_increment = 0;
	for(i = pool->active_threads;
	    i < total_threads && i < MAX_ACTIVE_THREADS;
	    i++)
	{
		if(pthread_create(&((pool->tids)[i]),
				NULL, routine, (void *)pool) != 0)	//新建线程
		{
			perror("add threads error");

			if(actual_increment == 0)
				return -1;

			break;
		}
		actual_increment++; 					//记录成功添加了多少条线程到线程池中
	}

	pool->active_threads += actual_increment;	//将最后成功添加到线程池的线程总数记录线程池中
	return actual_increment;					//返回新建了多少条线程
}

int remove_thread(thread_pool *pool, unsigned int removing_threads)
{
	if(removing_threads == 0)
		return pool->active_threads;

	int remain_threads = pool->active_threads - removing_threads;	//将移除线程之后的线程数量登记下来
	remain_threads = remain_threads>0 ? remain_threads:1;			//如果这个数量不大于0，则把它置1

	int i;
	for(i=pool->active_threads-1; i>remain_threads-1; i--)			//从id的最后一位线程开始取消
	{
		errno = pthread_cancel(pool->tids[i]);
		if(errno != 0)
			break;
	}

	if(i == pool->active_threads-1)				//判断是否取消掉要求的数量
		return -1;
	else
		pool->active_threads = i+1;					//将新的线程数量登记active_threads
	
	return pool->active_threads;					//返回剩下多少条线程在线程池中
}

bool destroy_pool(thread_pool *pool)
{

	pool->shutdown = true;					//使能线程池的退出开关
	pthread_cond_broadcast(&pool->cond);	//将所有的线程全部唤醒

	int i;
	for(i=0; i<pool->active_threads; i++)	//开始接合线程
	{
		errno = pthread_join(pool->tids[i], NULL);
		if(errno != 0)
		{
			printf("join tids[%d] error: %s\n",
					i, strerror(errno));
		}
		else
			printf("[%u] is joined\n", (unsigned)pool->tids[i]);
		
	}

	free(pool->task_list);		//释放掉任务头节点
	free(pool->tids);			//释放掉线程ID内存

	return true;
}
