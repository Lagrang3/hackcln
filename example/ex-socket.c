#include <assert.h>
#include <ccan/err/err.h>
#include <ccan/io/io.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

struct buffer {
	bool finished;
	size_t start, end, rlen, wlen;
	char buf[1024];
	char name[200];
};

struct buffer from_cln, from_cli;

static void finish_sock(struct io_conn *c, struct buffer *b)
{
	fprintf(stderr, "buffer (%s) connection fail\n", b->name);
	b->finished = true;
	io_wake(b);
}

static struct io_plan *read_sock(struct io_conn *c, struct buffer *b)
{
	fprintf(stderr, "last read (%s): \"%.*s\"\n", b->name, b->rlen,
		b->buf + b->end);
	b->end += b->rlen;
	assert(b->end <= sizeof(b->buf));
	assert(b->start <= b->end);

	if (b->rlen != 0)
		io_wake(b);

	b->rlen = 0;

	if (b->start == b->end)
		b->start = b->end = 0;

	if (b->end == sizeof(b->buf))
		return io_wait(c, b, read_sock, b);

	return io_read_partial(c, b->buf + b->end, sizeof(b->buf) - b->end,
			       &b->rlen, read_sock, b);
}

static struct io_plan *write_sock(struct io_conn *c, struct buffer *b)
{
	// fprintf(stderr, "last write (%s): \"%.*s\"\n", b->name, b->wlen,
	// 	b->buf + b->start);
	b->start += b->wlen;
	assert(b->end <= sizeof(b->buf));
	assert(b->start <= b->end);

	if (b->wlen != 0)
		io_wake(b);
	b->wlen = 0;

	if (b->end == b->start) {
		if (b->finished)
			return io_close(c);
		return io_wait(c, b, write_sock, b);
	}

	return io_write_partial(c, b->buf + b->start, b->end - b->start,
				&b->wlen, write_sock, b);
}

/* connect to cln through its unix socket */
int initialize_conn_cln(const char *sock_filename, int fd_cli)
{
	int fd_cln = socket(AF_UNIX, SOCK_STREAM, 0);
	struct io_conn *c;
	if (fd_cln == -1)
		errx(1, "Could not create socket");
	struct sockaddr_un addr;
	strcpy(addr.sun_path, sock_filename);
	addr.sun_family = AF_UNIX;

	if (connect(fd_cln, (struct sockaddr *)&addr, sizeof(addr)) != 0)
		errx(1, "Connecting to '%s'", sock_filename);

	memset(&from_cln, 0, sizeof(from_cln));
	memset(&from_cli, 0, sizeof(from_cli));
	strcpy(from_cln.name, "from_cln");
	strcpy(from_cli.name, "from_cli");

	/* read from CLN and write to CLI */
	c = io_new_conn(NULL, fd_cln, read_sock, &from_cln);
	// if the reading fails
	io_set_finish(c, finish_sock, &from_cln);
	io_new_conn(NULL, fd_cli, write_sock, &from_cln);

	/* read from CLI and write to CLN */
	c = io_new_conn(NULL, fd_cli, read_sock, &from_cli);
	// if the reading fails
	io_set_finish(c, finish_sock, &from_cli);
	io_new_conn(NULL, fd_cln, write_sock, &from_cli);

	return fd_cln;
}

/* open a new unix socket, wait for incoming connections */
int initialize_conn_cli(const char *sock_filename)
{
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd == -1)
		errx(1, "Could not create socket");
	struct sockaddr_un addr;
	strcpy(addr.sun_path, sock_filename);
	addr.sun_family = AF_UNIX;

	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)))
		errx(1, "Binding socket failed");

	listen(fd, 10);
	return fd;
}

int main(int argc, char *argv[])
{
	if (argc != 3)
		errx(1, "Usage: cmd sock_replace sock_with");

	/* set up a listening socket */
	int cli_sock = initialize_conn_cli(argv[2]);

	while (1) {
		/* for each new connection fork */
		int cli_client = accept(cli_sock, NULL, NULL);
		fprintf(stderr, "connected to new client\n");
		int cln_sock = initialize_conn_cln(argv[1], cli_client);
		io_loop(NULL, NULL);
		fprintf(stderr, "io_loop exited\n");
		close(cln_sock);
		close(cli_client);
	}

finish:
	close(cli_sock);
	return 0;
}
