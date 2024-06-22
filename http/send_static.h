#include "../http/server.h"

bool serve_static_dir(char *dir,
                      char *prefix,
                      struct server *s,
                      struct request *r,
                      uint32_t handle,
                      bool dir_listing,
                      bool convert_md);

bool send_file(struct server *s,
               uint32_t handle,
               char *file,
               char *mime);

bool send_file_md(struct server *s,
                  uint32_t handle,
                  char *file);