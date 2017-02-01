#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <execinfo.h>

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
        
typedef struct tagSigInfo
{
	int signo;
	char signame[20];
}SigInfo_S;

SigInfo_S g_stSigCatch[] = {
	{1,  "SIGHUP"},
	{2,  "SIGINT"},
	{3,  "SIGQUIT"}, 
    {6,  "SIGABRT"},
    {8,  "SIGFPE"},
    {11, "SIGSEGV"} 
};

typedef void (*sa_handler_func)(int);

void sig_handler(int signo)
{
	void *buffer[100];
	char** sym_stacks;
	int cnt;

	printf("=====SIG INFO=====\n");
	printf("sig name:%s\n",strsignal(signo));
	printf("sig no:%d\n",signo);

	cnt = backtrace(buffer,100);
	printf("sym stack deep:%d\n",cnt);

	sym_stacks = backtrace_symbols(buffer,cnt);

	if (sym_stacks == NULL)
	{
		printf("backtrace_symbols error\n");
		return;
	}

	for (int i = 0; i < cnt; ++i)
	{
		printf("%s\n", sym_stacks[i]);
	}

	free(sym_stacks);

	exit(0);
}

int sig_stack_init()
{
	stack_t ss;

	ss.ss_sp = malloc(SIGSTKSZ);
	ss.ss_flags = 0;
	ss.ss_size = SIGSTKSZ;

	return sigaltstack(&ss, NULL);
}

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

int sig_init()
{
	int ret;

	ret = sig_stack_init();
	ret |= sig_handler_init(g_stSigCatch, sizeof(g_stSigCatch)/sizeof(SigInfo_S),
	                        sig_handler, SA_ONSTACK);

	return ret;
}

void foo()
{
	int a =0;
	int b = 0;
	int c;

	c=a/b;

	(void)c;

	return;
}

void bug_func()
{
	foo();
}

int main(int argc, char const *argv[])
{
	sig_init();

	bug_func();
	
	return 0;
}

