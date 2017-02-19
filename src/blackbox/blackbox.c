#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <execinfo.h>

#include <sys/ptrace.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/user.h>

/*
注意：

1.  使用backtrace函数时，在编译选项中需要加上 –rdynamic 选项，比如： 
        gcc –rdynamic blackbox.c –o blackbox 。
2.  backtrace_symbols函数会输出出错时的16进制的地址，此时我们可以使用addr2line命令将其转换为我们具体的代码行数，
    在编译选项中需要加上 –rdynamic -g 选项, 比如： 
        gcc –rdynamic -g blackbox.c –o blackbox 。
    命令格式为：
        addr2line –e execute_file  addr ，比如  addr2line –e ./a.out 0x400d62
*/
        
#define MAX_STACK_DEEP 20

typedef struct tagSigInfo
{
	int signo;
	char signame[20];
}SigInfo_S;

/* 需要处理的信号集合 */
SigInfo_S g_stSigCatch[] = {
	{1,  "SIGHUP"},
	{2,  "SIGINT"},
	{3,  "SIGQUIT"}, 
    {6,  "SIGABRT"},
    {8,  "SIGFPE"},
    {11, "SIGSEGV"} 
};

typedef void (*sa_handler_func)(int);


void print_func_trace(int max_stack_deep)
{
	void** buffer;
	char** sym_stacks;
	int deep;
    int i;

	buffer = malloc(max_stack_deep * sizeof(void*));

	if (buffer == NULL)
	{
		printf("print_func_trace malloc error\n");
		return;
	}

    /* 获取函数调用栈 */
	deep = backtrace(buffer,max_stack_deep);
	printf("function stack deep:%d\n",deep);

    /* 获取函数调用栈名称 */
	sym_stacks = backtrace_symbols(buffer,deep);

	if (sym_stacks == NULL)
	{
		printf("backtrace_symbols error\n");
		return;
	}

    /* 打印函数调用栈*/
	for (i = 0; i < deep; ++i)
	{
		printf("%s\n", sym_stacks[i]);
	}

	free(buffer);
	free(sym_stacks);

    return;
}

void sig_handler(int signo)
{
	printf("=====BLACKOBX INFO=====\n");

    /* 打印信号名称和信号值 */
	printf("sig name:%s\n",strsignal(signo));
	printf("sig no:%d\n",signo);

    /* 打印函数调用栈名称 */
    print_func_trace(MAX_STACK_DEEP);

    exit(0);

    return;
}

/* 避免栈溢出时无法处理，信号处理函数使用自定义的栈空间 */
int sig_stack_init()
{
	stack_t ss;

	ss.ss_sp = malloc(SIGSTKSZ);
	ss.ss_flags = 0;
	ss.ss_size = SIGSTKSZ;

	return sigaltstack(&ss, NULL);
}

/* 注册信号处理函数 */
int sig_handler_init(SigInfo_S* stSigInfos, int size, sa_handler_func handler, int sa_flags)
{
	int i = 0;
	int ret;
	struct sigaction sa;

	memset(&sa, 0 , sizeof(sa));

	sigemptyset(&sa.sa_mask);
	sa.sa_flags = sa_flags;
	sa.sa_handler = sig_handler;

	for (i = 0; i < size; ++i)
	{

		ret = sigaction(stSigInfos[i].signo, &sa, NULL);
		if ( ret < 0)
		{
			return ret;
		}
	}
	
	return 0;

}

/* 信号处理函数 */
int sig_init()
{
	int ret;

	/* 避免栈溢出时无法处理，信号处理函数使用自定义的栈空间 */
	ret = sig_stack_init();

	/* 注册信号处理函数 */
	ret |= sig_handler_init(g_stSigCatch, sizeof(g_stSigCatch)/sizeof(SigInfo_S),
	                        sig_handler, SA_ONSTACK);

	return ret;
}


void float_bug_func()
{
	int a=0xcc;
    int b=0;
    int c=0xdd;

    c = a/b;

    (void)c;
	return;
}

void stack_overflow_bug_func()
{
	int a[100];

    stack_overflow_bug_func();

    (void)a;
	return;
}

void write_error_bug_func(void * ptr)
{
    int a = 0xcc;
    memset(ptr, 0, a);
    return;
}

void bug_func(int bug_type)
{
    switch(bug_type)
    {
	    case 0:
            float_bug_func();
            break;

        case 1:
            stack_overflow_bug_func();
            break;
        case 2:
            write_error_bug_func(NULL);
            break;

        default:
            break;

    }

    return;
}

int main(int argc, char const *argv[])
{
    int bug_type = 0;

    /* 注册信号处理函数 */
	sig_init();

    if (argc > 1)
    {
        bug_type = atoi(argv[1]);
    }

	bug_func(bug_type);
	
	return 0;
}

