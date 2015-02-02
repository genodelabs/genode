/*
 * \brief  SQLite VFS layer for Genode
 * \author Emery Hemingway
 * \date   2015-01-31
 *
 * In this file VFS refers to the SQLite VFS, not the Genode VFS.
 * See http://sqlite.org/vfs.html for more information.
 *
 * Filesystem calls wrap our libC, clock and timer calls use native
 * service sessions, and there is no file locking support.
 *
 * The filesystem interface is unoptimized and just simple
 * enough to meet the minimum requirements for a legacy VFS.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/lock_guard.h>
#include <file_system_session/file_system_session.h>
#include <rtc_session/connection.h>
#include <timer_session/connection.h>

/* jitterentropy includes */
namespace Jitter {
extern "C" {
#include <jitterentropy.h>
}
}

namespace Sqlite {

/* Sqlite includes */
extern "C" {
#include <sqlite3.h>
}

extern "C" {
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/param.h>
#include <unistd.h>
#include <errno.h>
}

/**
 * Convert an Rtc::Timestamp to a Julian Day Number.
 *
 * \param   ts   Timestamp to convert.
 *
 * \return       Julian Day Number, rounded down.
 *
 * The Julian Day starts at noon and this function rounds down,
 * so the return value is effectively 12 hours behind.
 *
 * https://en.wikipedia.org/wiki/Julian_day#Calculation
 */
unsigned julian_day(Rtc::Timestamp ts)
{
	unsigned a = (14 - ts.month) / 12;
	unsigned y = ts.year + 4800 - a;
	unsigned m = ts.month + 12*a - 3;

	return ts.day + (153*m + 2)/5 + 365*y + y/4 - y/100 + y/400 - 32046;
}

#define NOT_IMPLEMENTED PWRN("Sqlite::%s not implemented", __func__);

static Timer::Connection _timer;
static Jitter::rand_data *_jitter;

/*
** When using this VFS, the sqlite3_file* handles that SQLite uses are
** actually pointers to instances of type Genode_file.
*/
typedef struct Genode_file Genode_file;
struct Genode_file {
	sqlite3_file base;  /* Base class. Must be first. */
	int fd;             /* File descriptor */
	char *delete_path;  /* Delete this path on close */
};

static int genode_randomness(sqlite3_vfs *pVfs, int len, char *buf)
{
	static Genode::Lock lock;
	Genode::Lock::Guard guard(lock);

	return Jitter::jent_read_entropy(_jitter, buf, len);
}

static int genode_delete(sqlite3_vfs *pVfs, const char *pathname, int dirSync)
{
	int rc;

	rc = unlink(pathname);
	if (rc !=0 && errno==ENOENT)
		 return SQLITE_OK;

	if (rc == 0 && dirSync) {
		int dfd;
		int i;
		char dir[File_system::MAX_PATH_LEN];

		sqlite3_snprintf(File_system::MAX_PATH_LEN, dir, "%s", pathname);
		dir[File_system::MAX_PATH_LEN-1] = '\0';
		for (i = strlen(dir); i > 1 && dir[i] != '/'; i--)
			dir[i] = '\0';

		dfd = open(dir, O_RDONLY);
	if (dfd < 0) {
			rc = -1;
		} else {
			rc = fsync(dfd);
			close(dfd);
		}
	}

	return rc ?
		SQLITE_IOERR_DELETE : SQLITE_OK;
}

static int genode_close(sqlite3_file *pFile)
{
	int rc;
	Genode_file *p = (Genode_file*)pFile;

	rc = close(p->fd);
	if (rc)
		return SQLITE_IOERR_CLOSE;

	if (p->delete_path) {
		rc = genode_delete(NULL, p->delete_path, false);
		if (rc == SQLITE_OK) {
			sqlite3_free(p->delete_path);
		} else {
			return rc;
		}
	}

	return SQLITE_OK;
}

static int genode_write(sqlite3_file *pFile, const void *buf, int count, sqlite_int64 offset)
{
	Genode_file *p = (Genode_file*)pFile;

	if (lseek(p->fd, offset, SEEK_SET) != offset)
		return SQLITE_IOERR_SEEK;

	if (write(p->fd, buf, count) != count)
		return SQLITE_IOERR_WRITE;

	return SQLITE_OK;
}

static int genode_read(sqlite3_file *pFile, void *buf, int count, sqlite_int64 offset)
{
	Genode_file *p = (Genode_file*)pFile;

	if (lseek(p->fd, offset, SEEK_SET) != offset) {
		return SQLITE_IOERR_SEEK;
	}

	int n = read(p->fd, buf, count);
	if (n != count) {
		/* Unread parts of the buffer must be zero-filled */
		memset(&((char*)buf)[n], 0, count-n);
		return SQLITE_IOERR_SHORT_READ;
	}

	return SQLITE_OK;
}

static int genode_truncate(sqlite3_file *pFile, sqlite_int64 size)
{
	Genode_file *p = (Genode_file*)pFile;

	return (ftruncate(p->fd, size)) ?
		SQLITE_IOERR_FSYNC : SQLITE_OK;
}

static int genode_sync(sqlite3_file *pFile, int flags)
{
	Genode_file *p = (Genode_file*)pFile;

	return (fsync(p->fd)) ?
		SQLITE_IOERR_FSYNC : SQLITE_OK;
}

static int genode_file_size(sqlite3_file *pFile, sqlite_int64 *pSize)
{
	Genode_file *p = (Genode_file*)pFile;
	struct stat s;

	if (fstat(p->fd, &s) != 0)
		return SQLITE_IOERR_FSTAT;

	*pSize = s.st_size;

	return SQLITE_OK;
}

static int genode_lock(sqlite3_file *pFile, int eLock)
{
	NOT_IMPLEMENTED
	return SQLITE_OK;
}
static int genode_unlock(sqlite3_file *pFile, int eLock)
{
	NOT_IMPLEMENTED
	return SQLITE_OK;
}
static int genode_check_reserved_lock(sqlite3_file *pFile, int *pResOut)
{
	NOT_IMPLEMENTED
	*pResOut = 0;
	return SQLITE_OK;
}

/*
 * No xFileControl() verbs are implemented by this VFS.
 * Without greater control over writing, there isn't utility in processing this.
 * See https://www.sqlite.org/c3ref/c_fcntl_busyhandler.html
*/
static int genode_file_control(sqlite3_file *pFile, int op, void *pArg) { return SQLITE_OK; }

/*
** The xSectorSize() and xDeviceCharacteristics() methods. These two
** may return special values allowing SQLite to optimize file-system
** access to some extent. But it is also safe to simply return 0.
*/
static int genode_sector_size(sqlite3_file *pFile)
{
	NOT_IMPLEMENTED
	return 0;
}

static int genode_device_characteristics(sqlite3_file *pFile)
{
	NOT_IMPLEMENTED
	return 0;
}

static int random_string(char *buf, int len)
{
	const unsigned char chars[] =
		"abcdefghijklmnopqrstuvwxyz"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"0123456789";

	if (genode_randomness(NULL, len, buf)  != (len))
		return -1;

	for(int i=0; i <len; i++) {
		buf[i] = (char)chars[ ((unsigned char)buf[i]) % (sizeof(chars)-1) ];
	}
	return 0;
}

static int genode_open(
		sqlite3_vfs *pVfs,
		const char *name,               /* File to open, or 0 for a temp file */
		sqlite3_file *pFile,            /* Pointer to Genode_file struct to populate */
		int flags,                      /* Input SQLITE_OPEN_XXX flags */
		int *pOutFlags                  /* Output SQLITE_OPEN_XXX flags (or NULL) */
	)
{
	static const sqlite3_io_methods genodeio = {
		1,                               /* iVersion */
		genode_close,                    /* xClose */
		genode_read,                     /* xRead */
		genode_write,                    /* xWrite */
		genode_truncate,                 /* xTruncate */
		genode_sync,                     /* xSync */
		genode_file_size,                /* xFileSize */
		genode_lock,                     /* xLock */
		genode_unlock,                   /* xUnlock */
		genode_check_reserved_lock,      /* xCheckReservedLock */
		genode_file_control,             /* xFileControl */
		genode_sector_size,              /* xSectorSize */
		genode_device_characteristics    /* xDeviceCharacteristics */
	};

	Genode_file *p = (Genode_file*)pFile;
	memset(p, 0, sizeof(Genode_file));

	if (!name) {
		#define TEMP_PREFIX "sqlite_"
		#define TEMP_LEN 24

		char *temp = (char*)sqlite3_malloc(TEMP_LEN);

		strcpy(temp, TEMP_PREFIX);
		if (random_string(&temp[sizeof(TEMP_PREFIX)-1], TEMP_LEN-(sizeof(TEMP_PREFIX)))) {
			sqlite3_free(temp);
			return SQLITE_ERROR;
		}
		temp[TEMP_LEN-1] = '\0';

		name = temp;
		p->delete_path = temp;
	}

	int oflags = 0;
	if( flags&SQLITE_OPEN_EXCLUSIVE ) oflags |= O_EXCL;
	if( flags&SQLITE_OPEN_CREATE )    oflags |= O_CREAT;
	if( flags&SQLITE_OPEN_READONLY )  oflags |= O_RDONLY;
	if( flags&SQLITE_OPEN_READWRITE ) oflags |= O_RDWR;

	p->fd = open(name, oflags);
	if (p->fd <0)
		return SQLITE_CANTOPEN;

	if (pOutFlags)
		*pOutFlags = flags;

	p->base.pMethods = &genodeio;
	return SQLITE_OK;
}

static int genode_access(sqlite3_vfs *pVfs, const char *path, int flags, int *pResOut)
{
	int rc, mode;
	
	switch (flags) {
	case SQLITE_ACCESS_EXISTS:
		mode = F_OK;
		break;
	case SQLITE_ACCESS_READWRITE:
		mode = R_OK|W_OK;
		break;
	case SQLITE_ACCESS_READ:
		mode = R_OK;
		break;
	default:
		return SQLITE_INTERNAL;
	}

	*pResOut = (access(path, mode) == 0);
	return SQLITE_OK;
}

static int genode_full_pathname(sqlite3_vfs *pVfs, const char *path_in, int out_len, char *path_out)
{
	char dir[File_system::MAX_PATH_LEN];
	if (path_in[0]=='/') {
		strncpy(path_out, path_in, out_len);
	} else {
		if (getcwd(dir, sizeof(dir)) == 0)
			return SQLITE_IOERR;
		if (strcmp(dir, "/") == 0)
			sqlite3_snprintf(out_len, path_out, "/%s", path_in);
		else
			sqlite3_snprintf(out_len, path_out, "%s/%s", dir, path_in);
	}
	path_out[out_len-1] = '\0';
	return SQLITE_OK;
}

/*
 * The following four VFS methods:
 *
 *   xDlOpen
 *   xDlError
 *   xDlSym
 *   xDlClose
 *
 * are supposed to implement the functionality needed by SQLite to load
 * extensions compiled as shared objects. This simple VFS does not support
 * this functionality, so the following functions are no-ops.
 */
static void *genode_dl_open(sqlite3_vfs *pVfs, const char *zPath)
{
	NOT_IMPLEMENTED
	return 0;
}
static void genode_dl_error(sqlite3_vfs *pVfs, int nByte, char *zErrMsg)
{
	NOT_IMPLEMENTED
	sqlite3_snprintf(nByte, zErrMsg, "Loadable extensions are not implemented");
	zErrMsg[nByte-1] = '\0';
}
static void (*genode_dl_sym(sqlite3_vfs *pVfs, void *pH, const char *z))(void)
{
	NOT_IMPLEMENTED
	return 0;
}
static void genode_dl_close(sqlite3_vfs *pVfs, void *pHandle)
{
	NOT_IMPLEMENTED
	return;
}

/*
 * Sleep for at least nMicro microseconds. Return the (approximate) number
 * of microseconds slept for.
 */
static int genode_sleep(sqlite3_vfs *pVfs, int nMicro)
{
	unsigned long then, now;

	then = _timer.elapsed_ms();
	_timer.usleep(nMicro);
	now = _timer.elapsed_ms();

	return (now - then) / 1000;
}

/*
 * Write into *pTime the current time and date as a Julian Day
 * number times 86400000. In other words, write into *piTime
 * the number of milliseconds since the Julian epoch of noon in
 * Greenwich on November 24, 4714 B.C according to the proleptic
 * Gregorian calendar.
 *
 * JULIAN DATE IS NOT THE SAME AS THE JULIAN CALENDAR,
 * NOR WAS IT DEVISED BY SOMEONE NAMED JULIAN.
 */
static int genode_current_time(sqlite3_vfs *pVfs, double *pTime)
{
	try {
		Rtc::Connection rtc;
		Rtc::Timestamp ts = rtc.current_time();

		*pTime = julian_day(ts)  * 86400000.0;
		*pTime += (ts.hour + 12) *   360000.0;
		*pTime += ts.minute      *    60000.0;
		*pTime += ts.second      *     1000.0;
		*pTime += ts.microsecond /     1000.0;
	} catch (...) {
		PWRN("RTC not present, using dummy time");
		*pTime = _timer.elapsed_ms();
	}

	return SQLITE_OK;
}

/* See above. */
static int genode_current_time_int64(sqlite3_vfs *pVfs, sqlite3_int64 *pTime)
{
	try {
		Rtc::Connection rtc;
		Rtc::Timestamp ts = rtc.current_time();

		*pTime = julian_day(ts)  * 86400000;
		*pTime += (ts.hour + 12) *   360000;
		*pTime += ts.minute      *    60000;
		*pTime += ts.second      *     1000;
		*pTime += ts.microsecond /     1000;
	} catch (...) {
		PWRN("RTC not present, using dummy time");
		*pTime = _timer.elapsed_ms();
	}

	return SQLITE_OK;
}

/*****************************************
  ** Library initialization and cleanup **
  ****************************************/

#define VFS_NAME "genode"

int sqlite3_os_init(void)
{
	int ret = Jitter::jent_entropy_init();
	if(ret) {
		PERR("Jitter entropy initialization failed with error code %d", ret);
		return SQLITE_ERROR;
	}

	_jitter = Jitter::jent_entropy_collector_alloc(0, 0);
	if (!_jitter) {
		PERR("Jitter entropy collector initialization failed");
		return SQLITE_ERROR;
	}

	static sqlite3_vfs genode_vfs = {
		2,                         /* iVersion */
		sizeof(Genode_file),       /* szOsFile */
		File_system::MAX_PATH_LEN, /* mxPathname */
		NULL,                      /* pNext */
		VFS_NAME,                  /* zName */
		NULL,                      /* pAppData */
		genode_open,               /* xOpen */
		genode_delete,             /* xDelete */
		genode_access,             /* xAccess */
		genode_full_pathname,      /* xFullPathname */
		genode_dl_open,            /* xDlOpen */
		genode_dl_error,           /* xDlError */
		genode_dl_sym,             /* xDlSym */
		genode_dl_close,           /* xDlClose */
		genode_randomness,         /* xRandomness */
		genode_sleep,              /* xSleep */
		genode_current_time,       /* xCurrentTime */
		NULL,                      /* xGetLastError */
		genode_current_time_int64, /* xCurrentTimeInt64 */
	};

	sqlite3_vfs_register(&genode_vfs, 0);
	return SQLITE_OK;
}

int sqlite3_os_end(void)
{
	sqlite3_vfs *vfs = sqlite3_vfs_find(VFS_NAME);
	sqlite3_vfs_unregister(vfs);
	Jitter::jent_entropy_collector_free(_jitter);

	return SQLITE_OK;
}

} /* namespace Sqlite */
