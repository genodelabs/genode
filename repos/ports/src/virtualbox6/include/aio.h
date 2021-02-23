#ifndef _AIO_H_
#define _AIO_H_

#include <signal.h>

extern "C" {

#define LIO_NOWAIT 0x0
#define LIO_WRITE  0x1
#define LIO_READ   0x2

#define AIO_CANCELED    0x1
#define AIO_NOTCANCELED 0x2
#define AIO_ALLDONE     0x3

#define AIO_LISTIO_MAX 16

struct aiocb
{
	int    aio_fildes;
	size_t aio_nbytes;
	int    aio_lio_opcode;
	off_t  aio_offset;

	struct sigevent aio_sigevent;

	volatile void *aio_buf;
};

int aio_fsync(int op, struct aiocb *aiocbp);

ssize_t aio_return(struct aiocb *aiocbp);

int aio_error(const struct aiocb *aiocbp);

int aio_cancel(int fd, struct aiocb *aiocbp);

int aio_suspend(const struct aiocb * const aiocb_list[],
                int nitems, const struct timespec *timeout);

int lio_listio(int mode, struct aiocb *const aiocb_list[],
               int nitems, struct sigevent *sevp);

} /* extern "C" */

#endif /* _AIO_H_ */
