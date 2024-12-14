/* Example:
 * Creates a child process from the execution of "command" and redirects stdin
 * to that process stdin, and reads the process stdout and prints it to stdout.
 * 
 * usage:
 * ex-pipe ./example/ex-pipe-app
 * 
 * */

#include <assert.h>
#include <ccan/err/err.h>
#include <ccan/io/io.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

struct buffer {
	bool finished;
	size_t start, end, rlen, wlen;
	char buf[1024];
	char name[200];
};

static void finish(struct io_conn *c, struct buffer *b)
{
	printf("LOG: %s, buffer (%s) connection fail\n", __func__, b->name);
	b->finished = true;
	io_wake(b);
}
static struct io_plan *read_in(struct io_conn *c, struct buffer *b)
{
	// add what we just read
	b->end += b->rlen;
	assert(b->end <= sizeof(b->buf));
	assert(b->start <= b->end);

	// if we just read something, wake writer
	if (b->rlen != 0)
		io_wake(b);

	b->rlen = 0;
	// if buffer is empty, return to start
	if (b->start == b->end)
		b->start = b->end = 0;

	// no room? wait for writer
	if (b->end == sizeof(b->buf))
		return io_wait(c, b, read_in, b);

	return io_read_partial(c, b->buf + b->end, sizeof(b->buf) - b->end,
			       &b->rlen, read_in, b);
}

static struct io_plan *write_out(struct io_conn *c, struct buffer *b)
{
	// remove what we just wrote
	b->start += b->wlen;
	assert(b->end <= sizeof(b->buf));
	assert(b->start <= b->end);

	// if we wrote something, wake reader
	if (b->wlen != 0)
		io_wake(b);
	b->wlen = 0;

	// nothing to write? wait for reader
	if (b->end == b->start) {
		if (b->finished)
			return io_close(c);
		return io_wait(c, b, write_out, b);
	}

	return io_write_partial(c, b->buf + b->start, b->end - b->start,
				&b->wlen, write_out, b);
}

int main(int argc, char *argv[])
{
	int tochild[2], fromchild[2];

	if (argc == 1)
		errx(1, "Ussage runner <cmdline> ...");

	// [0] if for reading, [1] is for writing
	if (pipe(tochild) != 0 || pipe(fromchild) != 0)
		err(1, "Creating pipes");

	switch (fork()) {
	case -1:
		errx(1, "fork");
	case 0: /* Child */

		close(fromchild[0]); // only write
		close(tochild[1]);   // only read

		dup2(fromchild[1], STDOUT_FILENO);
		dup2(tochild[0], STDIN_FILENO);

		execvp(argv[1], argv + 1);
		exit(127); // command not found
		break;

	default:		     /* Parent */
		close(fromchild[1]); // only read
		close(tochild[0]);   // only write

		signal(SIGPIPE, SIG_IGN); // ignore broken pipe signal
		break;
	}

	struct buffer to, from;
	struct io_conn *con;
	int status;

	// new connection that reads from stdin
	memset(&to, 0, sizeof(to));
	strcpy(to.name, "tochild");
	io_new_conn(NULL, STDIN_FILENO, read_in, &to);
	// write that to child
	con = io_new_conn(NULL, tochild[1], write_out, &to);
	// if child fails
	io_set_finish(con, finish, &to);

	// read from child
	memset(&from, 0, sizeof(from));
	strcpy(from.name, "fromchild");
	con = io_new_conn(NULL, fromchild[0], read_in, &from);
	// if child fails
	io_set_finish(con, finish, &from);
	// write that to stdout
	io_new_conn(NULL, STDOUT_FILENO, write_out, &from);

	io_loop(NULL, NULL);
	wait(&status); // ?
	
	// TODO: close tochild connection after fromchild fails

	return WIFEXITED(status) ? WEXITSTATUS(status) : 2;
}
