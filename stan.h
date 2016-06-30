#include <httpd.h>
#include <http_core.h>
#include <apr_strings.h>

#include <yajl/yajl_parse.h>
#include <yajl/yajl_gen.h>

#define BUFLEN 8192
#define true 1
#define false 0

typedef struct {
  request_rec *r;
  apr_pool_t *pool;
  char *map_key;
  char *image;
  char *slug;
  char *title;
  int drm;
  int episode_count;
  int map_level;
  int array_level;
  int in_image;
  int in_payload;
  int results;
  yajl_gen g;
} stan_rec;
