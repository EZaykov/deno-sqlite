/*
** 2010 April 7
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
**
** This file implements a simple in-memory VFS. Based on the SQLite demovfs.
*/

#include "sqlite3.h"

// #include <assert.h>
// #include <string.h>
// #include <sys/types.h>
// #include <sys/stat.h>
// #include <sys/file.h>
// #include <sys/param.h>
// #include <unistd.h>
// #include <time.h>
// #include <errno.h>
// #include <fcntl.h>

/*
** The maximum pathname length supported by this VFS.
*/
#define MAXPATHNAME 512

/*
** When using this VFS, the sqlite3_file* handles that SQLite uses are
** actually pointers to instances of type WasiFile.
*/
typedef struct WasiFile WasiFile;
struct WasiFile {
  sqlite3_file base;              /* Base class. Must be first. */
  int fd;                         /* File descriptor */

  char *aBuffer;                  /* Pointer to malloc'd buffer */
  int nBuffer;                    /* Valid bytes of data in zBuffer */
  sqlite3_int64 iBufferOfst;      /* Offset in file of zBuffer[0] */
};

/*
** Close a file.
*/
static int wasiClose(sqlite3_file *pFile){
  // TODO
  return SQLITE_IOERR_CLOSE;
}

/*
** Read data from a file.
*/
static int wasiRead(
  sqlite3_file *pFile, 
  void *zBuf, 
  int iAmt, 
  sqlite_int64 iOfst
){
  WasiFile *p = (WasiFile*)pFile;
  // TODO
  return SQLITE_IOERR_READ;
}

/*
** Write data to a file.
*/
static int wasiWrite(
  sqlite3_file *pFile, 
  const void *zBuf, 
  int iAmt, 
  sqlite_int64 iOfst
){
  WasiFile *p = (WasiFile*)pFile;
  // TODO
  return SQLITE_IOERR_WRITE;
}

/*
** Truncate a file. This is a no-op for this VFS (see header comments at
** the top of the file).
*/
static int wasiTruncate(sqlite3_file *pFile, sqlite_int64 size){
  // TODO: Do we need this?
  return SQLITE_OK;
}

/*
** Sync the contents of the file to the persistent media.
*/
static int wasiSync(sqlite3_file *pFile, int flags){
  // NO OP
  return SQLITE_OK;
}

/*
** Write the size of the file in bytes to *pSize.
*/
static int wasiFileSize(sqlite3_file *pFile, sqlite_int64 *pSize){
  WasiFile *p = (WasiFile*)pFile;
  // TODO
  return SQLITE_ERROR;
}

/*
** Locking functions. The xLock() and xUnlock() methods are both no-ops.
** The xCheckReservedLock() always indicates that no other process holds
** a reserved lock on the database file. This ensures that if a hot-journal
** file is found in the file-system it is rolled back.
*/
static int wasiLock(sqlite3_file *pFile, int eLock){
  return SQLITE_OK;
}
static int wasiUnlock(sqlite3_file *pFile, int eLock){
  return SQLITE_OK;
}
static int wasiCheckReservedLock(sqlite3_file *pFile, int *pResOut){
  *pResOut = 0;
  return SQLITE_OK;
}

/*
** No xFileControl() verbs are implemented by this VFS.
*/
static int wasiFileControl(sqlite3_file *pFile, int op, void *pArg){
  return SQLITE_NOTFOUND;
}

/*
** The xSectorSize() and xDeviceCharacteristics() methods. These two
** may return special values allowing SQLite to optimize file-system
** access to some extent. But it is also safe to simply return 0.
*/
static int wasiSectorSize(sqlite3_file *pFile){
  return 0;
}
static int wasiDeviceCharacteristics(sqlite3_file *pFile){
  return 0;
}

/*
** Open a file handle.
*/
static int wasiOpen(
  sqlite3_vfs *pVfs,              /* VFS */
  const char *zName,              /* File to open, or 0 for a temp file */
  sqlite3_file *pFile,            /* Pointer to WasiFile struct to populate */
  int flags,                      /* Input SQLITE_OPEN_XXX flags */
  int *pOutFlags                  /* Output SQLITE_OPEN_XXX flags (or NULL) */
){
  static const sqlite3_io_methods wasiio = {
    1,                            /* iVersion */
    wasiClose,                    /* xClose */
    wasiRead,                     /* xRead */
    wasiWrite,                    /* xWrite */
    wasiTruncate,                 /* xTruncate */
    wasiSync,                     /* xSync */
    wasiFileSize,                 /* xFileSize */
    wasiLock,                     /* xLock */
    wasiUnlock,                   /* xUnlock */
    wasiCheckReservedLock,        /* xCheckReservedLock */
    wasiFileControl,              /* xFileControl */
    wasiSectorSize,               /* xSectorSize */
    wasiDeviceCharacteristics     /* xDeviceCharacteristics */
  };

  WasiFile *p = (WasiFile*)pFile;
  p->base.pMethods = &wasiio;
  // TODO

  return SQLITE_ERROR;
}

/*
** Delete the file identified by argument zPath. If the dirSync parameter
** is non-zero, then ensure the file-system modification to delete the
** file has been synced to disk before returning.
*/
static int wasiDelete(sqlite3_vfs *pVfs, const char *zPath, int dirSync){
  // TODO
  return SQLITE_IOERR_DELETE;
}

/*
** Query the file-system to see if the named file exists, is readable or
** is both readable and writable. All files are valid.
*/
static int wasiAccess(
  sqlite3_vfs *pVfs, 
  const char *zPath, 
  int flags, 
  int *pResOut
){
  *pResOut = 1;
  return SQLITE_OK;
}

/*
** Argument zPath points to a nul-terminated string containing a file path.
** For the in-memory file system zPath is always copied to the output.
*/
static int wasiFullPathname(
  sqlite3_vfs *pVfs,              /* VFS */
  const char *zPath,              /* Input path (possibly a relative path) */
  int nPathOut,                   /* Size of output buffer in bytes */
  char *zPathOut                  /* Pointer to output buffer */
){
  if (nPathOut > 0) {
    sqlite3_snprintf(nPathOut, zPathOut, "%s", zPath);
    zPathOut[nPathOut-1] = '\0';
  }
  return SQLITE_OK;
}

/*
** The following four VFS methods:
**
**   xDlOpen
**   xDlError
**   xDlSym
**   xDlClose
**
** are supposed to implement the functionality needed by SQLite to load
** extensions compiled as shared objects. This simple VFS does not support
** this functionality, so the following functions are no-ops.
*/
static void *wasiDlOpen(sqlite3_vfs *pVfs, const char *zPath){
  return 0;
}
static void wasiDlError(sqlite3_vfs *pVfs, int nByte, char *zErrMsg){
  sqlite3_snprintf(nByte, zErrMsg, "Loadable extensions are not supported");
  zErrMsg[nByte-1] = '\0';
}
static void (*wasiDlSym(sqlite3_vfs *pVfs, void *pH, const char *z))(void){
  return 0;
}
static void wasiDlClose(sqlite3_vfs *pVfs, void *pHandle){
  return;
}

/*
** Parameter zByte points to a buffer nByte bytes in size. Populate this
** buffer with pseudo-random data.
*/
static int wasiRandomness(sqlite3_vfs *pVfs, int nByte, char *zByte){
  // TODO: Embed a RNG (PCG) and seed from JS side if wanted ...
  return SQLITE_OK;
}

/*
** Sleep for at least nMicro microseconds. Return the (approximate) number 
** of microseconds slept for.
*/
static int wasiSleep(sqlite3_vfs *pVfs, int nMicro){
  // TODO: Can anything be done here?
  return 0;
}

/*
** Set *pTime to the current UTC time expressed as a Julian day. Return
** SQLITE_OK if successful, or an error code otherwise.
*/
static int wasiCurrentTime(sqlite3_vfs *pVfs, double *pTime){
  *pTime = 0;
  return SQLITE_ERROR;
}

/*
** This function returns a pointer to the VFS implemented in this file.
*/
sqlite3_vfs *sqlite3_wasivfs(void){
  static sqlite3_vfs wasivfs = {
    1,                            /* iVersion */
    sizeof(WasiFile),             /* szOsFile */
    MAXPATHNAME,                  /* mxPathname */
    0,                            /* pNext */
    "wasi",                       /* zName */
    0,                            /* pAppData */
    wasiOpen,                     /* xOpen */
    wasiDelete,                   /* xDelete */
    wasiAccess,                   /* xAccess */
    wasiFullPathname,             /* xFullPathname */
    wasiDlOpen,                   /* xDlOpen */
    wasiDlError,                  /* xDlError */
    wasiDlSym,                    /* xDlSym */
    wasiDlClose,                  /* xDlClose */
    wasiRandomness,               /* xRandomness */
    wasiSleep,                    /* xSleep */
    wasiCurrentTime,              /* xCurrentTime */
  };
  return &wasivfs;
}

/*
** Initialize WASI in-memory fs
*/
int sqlite3_os_init(void) {
  return sqlite3_vfs_register(sqlite3_wasivfs(), 1);
  // return SQLITE_OK;
}

int sqlite3_os_end(void) {
  return SQLITE_OK;
}
