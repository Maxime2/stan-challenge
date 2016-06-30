#include <httpd.h>
#include <http_core.h>
#include <http_config.h>
#include <http_protocol.h>
#include <http_log.h>
#include <apr.h>
#include <apr_strings.h>
#include "stan.h"
 
#define __(s, ...)  ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, __VA_ARGS__)

static int stan_null(void * ctx) {
    stan_rec *stan = (stan_rec*) ctx;
    if (stan->map_level == 2 && !strcasecmp("slug", stan->map_key))
      stan->slug = NULL;
    if (stan->map_level == 2 && !strcasecmp("title", stan->map_key))
      stan->title = NULL;
    if (stan->in_image && !strcasecmp("showImage", stan->map_key))
      stan->image = NULL;
    return true;
}

static int stan_string(void * ctx, const unsigned char * stringVal,
                           size_t stringLen)
{
    stan_rec *stan = (stan_rec*) ctx;
    if (stan->map_level == 2) {
      if (!strcasecmp("slug", stan->map_key))
	stan->slug = apr_pstrndup(stan->pool, stringVal, stringLen);
      if (!strcasecmp("title", stan->map_key))
	stan->title = apr_pstrndup(stan->pool, stringVal, stringLen);
    }
    if (stan->in_image && !strcasecmp("showImage", stan->map_key))
      stan->image = apr_pstrndup(stan->pool, stringVal, stringLen);
    return true;
}

static int stan_boolean(void * ctx, int boolean) {
    stan_rec *stan = (stan_rec*) ctx;
    if (stan->map_level == 2 && !strcasecmp("drm", stan->map_key))
      stan->drm = boolean;
    return true;
}

static int stan_number(void * ctx, const char * s, size_t l)
{
    stan_rec *stan = (stan_rec*) ctx;
    if (stan->map_level == 2 && !strcasecmp("episodeCount", stan->map_key)) {
      char *buf = apr_pstrndup(stan->pool, s, l);
      if (sscanf(buf, "%d", &stan->episode_count) != 1) return false;
    }
      
    return true;
}

static int stan_start_map(void * ctx) {
    stan_rec *stan = (stan_rec*) ctx;
    stan->map_level++;
    __(stan->r->server, " ** start map|map_level:%d", stan->map_level);
    return true;
}

static int stan_end_map(void * ctx) {
    stan_rec *stan = (stan_rec*) ctx;
    __(stan->r->server, " ** end map|map_level:%d", stan->map_level);
    if (--stan->map_level < 0) return false;
    stan->in_image = false;
    if (stan->map_level == 1) {
      if (stan->in_payload == true && stan->drm == true && stan->episode_count > 0) {
	ap_rprintf(stan->r,
		   "%s{"
		   "\"image\": %s%s%s,"
		   "\"slug\": %s%s%s,"
		   "\"title\": %s%s%s"
		   "}",
		   stan->results?",":"",
		   stan->image?"\"":"", stan->image?stan->image:"null", stan->image?"\"":"",
		   stan->slug?"\"":"", stan->slug?stan->slug:"null", stan->slug?"\"":"",
		   stan->title?"\"":"", stan->title?stan->title:"null", stan->title?"\"":""
		   );
	stan->results++;
      }
      if (stan->pool != NULL) {
	apr_pool_destroy(stan->pool);
	stan->pool = NULL;
	stan->map_key = NULL;
	stan->image = NULL;
	stan->slug = NULL;
	stan->title = NULL;
	stan->drm = false;
	stan->episode_count = 0;
      }
      if (stan->array_level == 0) stan->in_payload = false;
    }
    return true;
}

static int stan_map_key(void * ctx, const unsigned char * stringVal,
                            size_t stringLen) {
    stan_rec *stan = (stan_rec*) ctx;

    if (stan->map_level == 1 && !strncasecmp("payload", stringVal, stringLen)) {
      stan->in_payload = true;
      __(stan->r->server, " ** in payload");
    }
    if (stan->pool == NULL) {
      if (APR_SUCCESS != apr_pool_create(&stan->pool, stan->r->pool)) {
	__(stan->r->server, " ** pool creation error");
	return false;
      }
    }
    stan->map_key = apr_pstrndup(stan->pool, stringVal, stringLen);
    if (stan->map_level == 2 && !strcasecmp("image", stan->map_key))
      stan->in_image = true;
    return true;
}

static int stan_start_array(void * ctx)
{
    stan_rec *stan = (stan_rec*) ctx;
    stan->array_level++;
    __(stan->r->server, " ** start array|array_level:%d", stan->array_level);
    return true;
}

static int stan_end_array(void * ctx)
{
    stan_rec *stan = (stan_rec*) ctx;
    __(stan->r->server, " ** end array|array_level:%d", stan->array_level);
    if (--stan->array_level < 0) return yajl_status_error;
    return true;
}


static yajl_callbacks callbacks = {
    stan_null,
    stan_boolean,
    NULL,
    NULL,
    stan_number,
    stan_string,
    stan_start_map,
    stan_map_key,
    stan_end_map,
    stan_start_array,
    stan_end_array
};

static int stan_handler (request_rec * r) {
   apr_bucket *b;
   apr_bucket_brigade *bb;
   apr_status_t status;
   const char *buf;
   int end = 0;
   int parse_result;
   apr_size_t bytes, count = 0;
   stan_rec stan = {r,
		    NULL,/*pool*/
		    NULL,/*map_key*/
		    NULL,/*image*/
		    NULL,/*slug*/
		    NULL,/*title*/
		    false,/*drm*/
		    0,/*episode_count*/
		    0,/*map_level*/
		    0,/*array_level*/
		    false,/*in_image*/
		    false,/*in_payload*/
		    0,/*results*/
		    0 /*g*/};
   yajl_handle hand;
   yajl_status json_status = yajl_status_ok;
 
   if (!r -> handler || strcmp (r -> handler, "stan-handler") ) return DECLINED;
   if (!r -> method || (strcmp (r -> method, "GET") && strcmp (r -> method, "POST")) ) return DECLINED;

   ap_set_content_type(r, "application/json");
   
   ap_rprintf(r, "{\"response\":[");

   stan.g = yajl_gen_alloc(NULL);
   yajl_gen_config(stan.g, yajl_gen_validate_utf8, 1);

   hand = yajl_alloc(&callbacks, NULL, (void *) &stan);
   yajl_config(hand, yajl_allow_comments, 1);
   yajl_config(hand, yajl_allow_partial_values, 1);


   bb = apr_brigade_create(r->pool, r->connection->bucket_alloc);
   do {
     status = ap_get_brigade(r->input_filters, bb, AP_MODE_READBYTES, APR_BLOCK_READ, BUFLEN);
     __(r->server, "** status: %d", status);
     if (status == APR_SUCCESS) {
       for (b = APR_BRIGADE_FIRST(bb);
	    b != APR_BRIGADE_SENTINEL(bb);
	    b = APR_BUCKET_NEXT(b) ) {
	 if (APR_BUCKET_IS_EOS(b)) {
	   __(r->server, "** EOS");
	   end = 1;
	   break;
	 } else if (APR_BUCKET_IS_METADATA(b)) {
	   __(r->server, "** METADATA");
	   continue;
	 } else if (APR_SUCCESS == apr_bucket_read(b, &buf, &bytes, APR_BLOCK_READ)) {
	   count += bytes;
	   __(r->server, " bytes: %lu -- %s", bytes, buf);
	   json_status = yajl_parse(hand, buf, bytes);
	   __(r->server, "** status: %d", json_status);
	   if (json_status != yajl_status_ok) {
	     unsigned char * str = yajl_get_error(hand, true, buf, bytes);
	     __(r->server, " ** error: %s", str); 
	     end = 1;
	     break;
	   }
	 } else {
	   __(r->server, "Request body read READ BUCKET ERROR");
	   status = APR_EGENERAL;
	 }
       }
     } else
       ap_log_error(APLOG_MARK, APLOG_ERR, 0, r->server, "Brigade error");
     apr_brigade_cleanup(bb);
   } while (!end && (status == APR_SUCCESS));


   ap_rprintf(r, "]");

     
   if (status != APR_SUCCESS) {
     ap_log_error(APLOG_MARK, APLOG_CRIT, 0, r->server, "error reading post input filters bucket brigade");
     ap_rprintf(r, ",\n\"error\": \"error reading bucket brigade\"");
     ap_custom_response(r, HTTP_INTERNAL_SERVER_ERROR, "{\"error\":\"Could not decode request: error reading bucket brigade\"}");
     return HTTP_INTERNAL_SERVER_ERROR;
   } else if (json_status != yajl_status_ok) {
     ap_rprintf(r, ",\n\"error\": \"JSON parsing error\"");
     ap_custom_response(r, HTTP_BAD_REQUEST, "{\"error\":\"Could not decode request: JSON parsing failed\"}");
   }

   ap_rprintf(r, "\n}\n");

   if (status != APR_SUCCESS || json_status != yajl_status_ok)
     return HTTP_BAD_REQUEST;
   return OK;
}

static void stan_hooks (apr_pool_t * pool) {
    ap_hook_handler (stan_handler, NULL, NULL, APR_HOOK_LAST);
}
module AP_MODULE_DECLARE_DATA stan_module = {
	STANDARD20_MODULE_STUFF,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL, /* cmds */
	stan_hooks
} ;
