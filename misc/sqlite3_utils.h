#include <stdbool.h>
#include "sqlite3.h"

bool          sqlite3UtilsExec(sqlite3 *handle, const char *fmt, ...);
sqlite3_stmt *sqlite3UtilsPrepare(sqlite3 *handle, const char *fmt, ...);
int           sqlite3UtilsRowsExist(sqlite3 *handle, const char *fmt, ...);
int           sqlite3UtilsFetch(sqlite3_stmt *stmt, char *types, ...);
