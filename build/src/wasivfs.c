#include <stdlib.h>
#include "sqlite3.h"
#include "pcg.h"
#include "buffer.h"

// SQLite VFS component.
// Based on demoVFS from SQLlite.

// File-names simply index into the open_files array
#define MAXPATHNAME 1

#define TEMP_PATH '\0'
#define ID_FROM_PATH(path) ((int)(path[0]-1))

int access_count = 0;
int access_out = -1;
int __attribute__((used)) __attribute__ ((visibility ("default"))) debug_test() {
  return access_count;
}

/*
** When using this VFS, the sqlite3_file* handles that SQLite uses are
** actually pointers to instances of type WasiFile.
*/
typedef struct WasiFile WasiFile;
struct WasiFile {
  sqlite3_file base;
  char id;
  buffer* buf;
};

// For permanent files, this is a no-op. For temp
// files this deallocates the buffer.
static int wasiClose(sqlite3_file *pFile){
  WasiFile* p = (WasiFile*)pFile;
  if (p->id == TEMP_PATH)
    destroy_buffer(p->buf);
  return SQLITE_OK;
}

// Read data from a file.
static int wasiRead(sqlite3_file *pFile, void *zBuf, int iAmt, sqlite_int64 iOfst){
  WasiFile *p = (WasiFile*)pFile;
  return read_buffer(p->buf, (char* )zBuf, (int)iOfst, iAmt) ? SQLITE_OK : SQLITE_IOERR_READ;
}

// Write data to a file.
static int wasiWrite(sqlite3_file *pFile, const void *zBuf, int iAmt, sqlite_int64 iOfst){
  WasiFile *p = (WasiFile*)pFile;
  return write_buffer(p->buf, (char* )zBuf, (int)iOfst, iAmt) ? SQLITE_OK : SQLITE_IOERR_WRITE;
}

// We do not implement this. TODO: Should we?
static int wasiTruncate(sqlite3_file *pFile, sqlite_int64 size){
  return SQLITE_OK;
}

// We are completely in-memory.
static int wasiSync(sqlite3_file *pFile, int flags){
  return SQLITE_OK;
}

// Write the size of the file in bytes to *pSize.
static int wasiFileSize(sqlite3_file *pFile, sqlite_int64 *pSize){
  WasiFile *p = (WasiFile*)pFile;
  *pSize = (sqlite_int64)p->buf->size;
  return SQLITE_OK;
}

// We do not support nor need files locks.
static int wasiLock(sqlite3_file *pFile, int eLock){
  return SQLITE_OK;
}
static int wasiUnlock(sqlite3_file *pFile, int eLock){
  return SQLITE_OK;
}
static int wasiCheckReservedLock(sqlite3_file *pFile, int *pResOut){
  access_count ++;
  *pResOut = 0;
  return SQLITE_OK;
}

// No xFileControl() verbs are implemented by this VFS.
static int wasiFileControl(sqlite3_file *pFile, int op, void *pArg){
  return SQLITE_NOTFOUND;
}

// We are in-memory.
static int wasiSectorSize(sqlite3_file *pFile){
  return 0;
}
static int wasiDeviceCharacteristics(sqlite3_file *pFile){
  return 0;
}

// Open a file handle.
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

  access_count ++;

  if (zName == NULL) {
    p->buf = new_buffer();
  } else {
    p->buf = get_reg_buffer(ID_FROM_PATH(zName));
  }

  return p->buf != NULL ? SQLITE_OK : SQLITE_CANTOPEN;
}

// Delete the file at the path.
static int wasiDelete(sqlite3_vfs *pVfs, const char *zPath, int dirSync){
  access_count ++;
  delete_reg_buffer(ID_FROM_PATH(zPath));
  return SQLITE_OK;
}

// All valid id files are accessible.
static int wasiAccess(sqlite3_vfs *pVfs, const char *zPath, int flags, int *pResOut){
  access_count ++;
  switch (flags) {
    case SQLITE_ACCESS_EXISTS:
      *pResOut = in_use_reg_id(ID_FROM_PATH(zPath));
      break;
    default:
      *pResOut = valid_reg_id(ID_FROM_PATH(zPath));
      break;
  }
  return SQLITE_OK;
}

// This just copies the data, as file names are all one character long.
static int wasiFullPathname(sqlite3_vfs *pVfs, const char *zPath, int nPathOut, char *zPathOut){
  access_count ++;
  if (nPathOut > 1) {
    zPathOut[0] = zPath[0];
    zPathOut[1] = '\0';
    return SQLITE_OK;
  }
  return SQLITE_CANTOPEN;
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

// Generate pseudo-random data
static int wasiRandomness(sqlite3_vfs *pVfs, int nByte, char *zByte){
  pcg_bytes(zByte, nByte);
  return SQLITE_OK;
}

// TODO: Can anything be done here? Possibly if we get proper WASI support?
static int wasiSleep(sqlite3_vfs *pVfs, int nMicro){
  access_count ++;
  return 0;
}

// TODO: Can anything be done here? Possibly if we get proper WASI support?
static int wasiCurrentTime(sqlite3_vfs *pVfs, double *pTime){
  access_count ++;
  *pTime = 0;
  return SQLITE_ERROR;
}

// This function returns a pointer to the VFS implemented in this file.
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

// Initialize WASI in-memory fs
int sqlite3_os_init(void) {
  return sqlite3_vfs_register(sqlite3_wasivfs(), 1);
}

int sqlite3_os_end(void) {
  return SQLITE_OK;
}
