#include <u.h>
#define NOPLAN9DEFINES
#include <libc.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/un.h>
#include <errno.h>

#undef sun
#define sun sockun

int
_p9netfd(char *dir)
{
	int fd;

	if(strncmp(dir, "/dev/fd/", 8) != 0)
		return -1;
	fd = strtol(dir+8, &dir, 0);
	if(*dir != 0)
		return -1;
	return fd;
}

static void
putfd(char *dir, int fd)
{
	snprint(dir, NETPATHLEN, "/dev/fd/%d", fd);
}

#undef unix
#define unix sockunix

static int
addrlen(struct sockaddr_storage *ss)
{
	switch(ss->ss_family){
	case AF_INET:
		return sizeof(struct sockaddr_in);
	case AF_INET6:
		return sizeof(struct sockaddr_in6);
	case AF_UNIX:
		return sizeof(struct sockaddr_un);
	}
	return 0;
}

int
p9announce(char *addr, char *dir)
{
	int proto;
	char *buf, *unix;
	char *net;
	int port, s;
	int n;
	socklen_t sn;
	struct sockaddr_storage ss;

	buf = strdup(addr);
	if(buf == nil)
		return -1;

	if(p9dialparse(buf, &net, &unix, &ss, &port) < 0){
		free(buf);
		return -1;
	}
	if(strcmp(net, "tcp") == 0)
		proto = SOCK_STREAM;
	else if(strcmp(net, "udp") == 0)
		proto = SOCK_DGRAM;
	else if(strcmp(net, "unix") == 0)
		goto Unix;
	else{
		werrstr("can only handle tcp, udp, and unix: not %s", net);
		free(buf);
		return -1;
	}
	free(buf);

	if((s = socket(ss.ss_family, proto, 0)) < 0)
		return -1;
	sn = sizeof n;
	if(port && getsockopt(s, SOL_SOCKET, SO_TYPE, (void*)&n, &sn) >= 0
	&& n == SOCK_STREAM){
		n = 1;
		setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&n, sizeof n);
	}
	if(bind(s, (struct sockaddr*)&ss, addrlen(&ss)) < 0){
		close(s);
		return -1;
	}
	if(proto == SOCK_STREAM){
		listen(s, 8);
		putfd(dir, s);
	}
	return s;

Unix:
	if((s = socket(ss.ss_family, SOCK_STREAM, 0)) < 0)
		return -1;
	if(bind(s, (struct sockaddr*)&ss, addrlen(&ss)) < 0){
		if(errno == EADDRINUSE
		&& connect(s, (struct sockaddr*)&ss, addrlen(&ss)) < 0
		&& errno == ECONNREFUSED){
			/* dead socket, so remove it */
			remove(unix);
			close(s);
			if((s = socket(ss.ss_family, SOCK_STREAM, 0)) < 0)
				return -1;
			if(bind(s, (struct sockaddr*)&ss, addrlen(&ss)) >= 0)
				goto Success;
		}
		close(s);
		return -1;
	}
Success:
	listen(s, 8);
	putfd(dir, s);
	return s;
}

int
p9listen(char *dir, char *newdir)
{
	int fd, one;

	if((fd = _p9netfd(dir)) < 0){
		werrstr("bad 'directory' in listen: %s", dir);
		return -1;
	}

	if((fd = accept(fd, nil, nil)) < 0)
		return -1;

	one = 1;
	setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char*)&one, sizeof one);

	putfd(newdir, fd);
	return fd;
}

int
p9accept(int cfd, char *dir)
{
	int fd;

	if((fd = _p9netfd(dir)) < 0){
		werrstr("bad 'directory' in accept");
		return -1;
	}
	/* need to dup because the listen fd will be closed */
	return dup(fd);
}
