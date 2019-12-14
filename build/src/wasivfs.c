#include <string.h>
#include <stdlib.h>
#include "sqlite3.h"

// File-names simply index into the open_files array
#define MAXPATHNAME 1
#define MAXOPENFILES 256

// WasiBuf stores a memory buffer
typedef struct WasiBuf WasiBuf;
struct WasiBuf {
  char* b;
  int len;
};

WasiBuf** open_files;

WasiBuf* new_buffer() {
  WasiBuf* buf = malloc(sizeof(WasiBuf));
  if (buf == NULL)
    return NULL;
  buf->b = NULL;
  buf->len = 0;
  return buf;
}
int grow_buffer(WasiBuf* buf, int size) {
  if (buf->len <= size)
    return 1;
  if (buf->b == NULL) {
    buf->b = malloc(sizeof(char) * size);
    return buf->b != NULL;
  } else {
    char* b = realloc(buf->b, sizeof(char) * size);
    if (b == NULL)
      return 0;
    buf->b = b;
    return 1;
  }
}
void free_buffer(WasiBuf* buf) {
  if (buf == NULL)
    return;
  if (buf->b != NULL)
    free(buf->b);
  free(buf);
}

/*
** When using this VFS, the sqlite3_file* handles that SQLite uses are
** actually pointers to instances of type WasiFile.
*/
typedef struct WasiFile WasiFile;
struct WasiFile {
  sqlite3_file base;
  char id;
  WasiBuf* buf;
};

// For permanent files, this is a no-op. For temp
// files this deallocates the buffer.
static int wasiClose(sqlite3_file *pFile){
  WasiFile* p = (WasiFile*)pFile;
  if (p->id == '\0')
    free_buffer(p->buf);
  return SQLITE_OK;
}

// Read data from a file.
static int wasiRead(sqlite3_file *pFile, void *zBuf, int iAmt, sqlite_int64 iOfst){
  WasiFile *p = (WasiFile*)pFile;

  // If the read is to long, this is an error
  if ((sqlite_int64)iAmt + iOfst > p->buf->len)
    return SQLITE_IOERR_READ;

  memcpy(zBuf, &(p->buf->b[iOfst]), iAmt);
  return SQLITE_OK;
}

// Write data to a file.
static int wasiWrite(sqlite3_file *pFile, const void *zBuf, int iAmt, sqlite_int64 iOfst){
  WasiFile *p = (WasiFile*)pFile;

  // TODO: Do we need to zero the new memory?
  if (!grow_buffer(p->buf, (sqlite_int64)iAmt + iOfst))
    return SQLITE_IOERR_WRITE;

  memcpy(&(p->buf->b[iOfst]), zBuf, iAmt);
  return SQLITE_OK;
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
  *pSize = p->buf->len;
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

  // Create new buffer for temp-file
  if (zName == NULL) {
    p->id = '\0';
    WasiBuf* buf = new_buffer();
    if (buf == NULL)
      return SQLITE_NOMEM;
    p->buf = buf;
  } else {
    p->id = zName[0];
    if (open_files[zName[0]] == NULL) {
      open_files[zName[0]] = new_buffer();
      if (open_files[zName[0]] == NULL)
        return SQLITE_NOMEM;
    }
    p->buf = open_files[zName[0]];
  }

  return SQLITE_OK;
}

// Delete the file at the path.
static int wasiDelete(sqlite3_vfs *pVfs, const char *zPath, int dirSync){
  free_buffer(open_files[zPath[0]]);
  open_files[zPath[0]] = NULL;
  return SQLITE_OK;
}

// All valid length files exist and are accessible.
static int wasiAccess(sqlite3_vfs *pVfs, const char *zPath, int flags, int *pResOut){
  *pResOut = 1;
  return SQLITE_OK;
}

// This just copies the data, as file names are all one long.
static int wasiFullPathname(
  sqlite3_vfs *pVfs,              /* VFS */
  const char *zPath,              /* Input path (possibly a relative path) */
  int nPathOut,                   /* Size of output buffer in bytes */
  char *zPathOut                  /* Pointer to output buffer */
){
  if (nPathOut > 0) {
    zPathOut[0] = zPath[0];
    zPathOut[1] = '\0';
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

// Generate pseudo-random data
static int wasiRandomness(sqlite3_vfs *pVfs, int nByte, char *zByte){
  // TODO: Embed a RNG (PCG) and seed from JS side if wanted ...
  return SQLITE_OK;
}

// TODO: Can anything be done here?
static int wasiSleep(sqlite3_vfs *pVfs, int nMicro){
  return 0;
}

// TODO: Can anything be done here?
static int wasiCurrentTime(sqlite3_vfs *pVfs, double *pTime){
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
  // Create file registry
  open_files = malloc(sizeof(WasiBuf*) * MAXOPENFILES);
  if (open_files == NULL)
    return SQLITE_NOMEM;
  for (int i = 0; i < MAXOPENFILES; i ++)
    open_files[i] = NULL;
  // Register with SQLite
  return sqlite3_vfs_register(sqlite3_wasivfs(), 1);
}

int sqlite3_os_end(void) {
  return SQLITE_OK;
}
