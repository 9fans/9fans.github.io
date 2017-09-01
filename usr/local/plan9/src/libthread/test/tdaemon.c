#include <u.h>
#include <libc.h>
#include <thread.h>

void
proc(void *v)
{
	sleep(5*1000);
	print("still running\n");
}

void
threadmain(int argc, char **argv)
{
	proccreate(proc, nil, 32768);
}
