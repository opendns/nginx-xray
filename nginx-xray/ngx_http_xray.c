// Copyright (C) OpenDNS Inc.

#include <nginx.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include "ngx_http_xray.h"

typedef struct {
    ngx_buf_t      *xray;
    ngx_int_t      xray_level;
    uint           xray_chained;
} ngx_http_xray_ctx_t;

typedef struct {
   size_t           xray_buffer_size;
} ngx_http_xray_main_conf_t;

static void *
ngx_http_xray_create_main_conf(ngx_conf_t *cf);

static ngx_int_t
ngx_http_worker_init(ngx_cycle_t *cycle);

static ngx_command_t ngx_http_xray_commands[] = {
    { ngx_string("xray_buffer_size"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_MAIN_CONF_OFFSET,
      offsetof(ngx_http_xray_main_conf_t, xray_buffer_size),
      NULL },

    ngx_null_command
};

static ngx_http_module_t  ngx_http_xray_module_ctx = {
    NULL,         /* preconfiguration */
    NULL,         /* postconfiguration */

    ngx_http_xray_create_main_conf,  /* create main configuration */
    NULL,         /* init main configuration */

    NULL,         /* create server configuration */
    NULL,         /* merge server configuration */

    NULL,         /* create location configuration */
    NULL          /* merge location configuration */
};


ngx_module_t  ngx_http_xray_module = {
    NGX_MODULE_V1,
    &ngx_http_xray_module_ctx,  /* module context */
    ngx_http_xray_commands,     /* module directives */
    NGX_HTTP_MODULE,            /* module type */
    NULL,                       /* init master */
    NULL,                       /* init module */
    ngx_http_worker_init,       /* init worker */
    NULL,                       /* init thread */
    NULL,                       /* exit thread */
    NULL,                       /* exit worker */
    NULL,                       /* exit master */
    NGX_MODULE_V1_PADDING
};

ngx_http_output_body_filter_pt    next_body_filter  ;
ngx_http_output_header_filter_pt  next_header_filter;


ngx_int_t
ngx_http_xray_header_output_filter(ngx_http_request_t *r)
{
    ngx_http_xray_ctx_t  *xctx = ngx_http_get_module_ctx(r, ngx_http_xray_module);

    if (!xctx || !xctx->xray || xctx->xray->last == xctx->xray->start) {
        /* nothing to be done */
        return next_header_filter(r);
    }

    /** WARN ** Dont add XRAY logs beyond this point! **/
    ngx_log_debug(NGX_LOG_DEBUG_HTTP, ngx_cycle->log, 0,
        "%s: xray buffer used: %i", __func__, xctx->xray->last - xctx->xray->start);

    if (r->headers_out.content_length_n >= 0) {

        r->headers_out.content_length_n += xctx->xray->last - xctx->xray->start;

        if (r->headers_out.content_length) {
            r->headers_out.content_length->hash = 0;
        }
        r->headers_out.content_length = NULL;

        ngx_log_debug(NGX_LOG_DEBUG_HTTP, ngx_cycle->log, 0,
            "%s: new content length: %i", __func__, r->headers_out.content_length_n);
    }

    return next_header_filter(r);
}

ngx_int_t
ngx_http_xray_body_output_filter(ngx_http_request_t *r, ngx_chain_t *in)
{
    ngx_chain_t            *cl;
    ngx_http_xray_ctx_t    *xctx = ngx_http_get_module_ctx(r, ngx_http_xray_module);

    if (in == NULL || r->header_only) {
        return next_body_filter(r, in);
    }

    if (!xctx || !xctx->xray || xctx->xray->last == xctx->xray->start) {
        /* nothing to be done */
        return next_body_filter(r, in);
    }

    if (xctx->xray_chained) {
        return next_body_filter(r, in);
    }
    xctx->xray_chained = 1;

    ngx_log_debug(NGX_LOG_DEBUG_HTTP, ngx_cycle->log, 0,
        "prepending %i bytes of xray data", xctx->xray->last - xctx->xray->start);

    cl = ngx_alloc_chain_link(r->pool);
    if (cl == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
            "ngx_alloc_chain_link failed");

        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    xctx->xray->memory = 1;
    cl->buf = xctx->xray;
    cl->next = in;
    in = cl;

    // if the request is not chunked, done
    if (r->headers_out.content_length_n >= 0) {
        return next_body_filter(r, in);
    }

    if (!r->chunked) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
            "content_length_n=%i but r->chunked=%i. Skipping xray append.",
            r->headers_out.content_length_n, (ngx_int_t) r->chunked);
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    ngx_log_debug(NGX_LOG_DEBUG_HTTP, ngx_cycle->log, 0,
        "response is chunked");

    // allocate and prepend the chunk header
    cl = ngx_alloc_chain_link(r->pool);
    if (cl == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
            "ngx_alloc_chain_link failed");

        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    cl->buf = ngx_create_temp_buf(r->pool, 32); // enough for length + CRLF
    if (cl->buf == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
            "ngx_create_temp_buf failed");

        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    cl->buf->memory = 1;
    cl->buf->last = ngx_slprintf(cl->buf->start, cl->buf->end,
        "%xO" CRLF, xctx->xray->last - xctx->xray->start);

    ngx_log_debug(NGX_LOG_DEBUG_HTTP, ngx_cycle->log, 0,
        "added chunk length header: %xO", xctx->xray->last - xctx->xray->start);

    // in->buf is pointing at the non-NULL Xray body, append a CRLF to the end
    if (in->buf->end - in->buf->last < 2) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
            "insufficient space in xray buffer to append CRLF");

        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    *(in->buf->last++) = CR;
    *(in->buf->last++) = LF;

    cl->next = in;
    in = cl;

    return next_body_filter(r, in);
}

static void *
ngx_http_xray_create_main_conf(ngx_conf_t *cf)
{
    ngx_http_xray_main_conf_t *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_xray_main_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    conf->xray_buffer_size = 2048;
    return conf;
}

static ngx_int_t
ngx_http_worker_init(ngx_cycle_t *cycle){
    // Ensure we are on top of response generation queue
    next_body_filter           = ngx_http_top_body_filter;
    ngx_http_top_body_filter   = ngx_http_xray_body_output_filter;

    next_header_filter         = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ngx_http_xray_header_output_filter;

    return NGX_OK;
}

inline void
ngx_http_xray_line(
    ngx_http_request_t *r,
    const ngx_int_t level,
    const char *format, ...)
{
    va_list                 args;
    ngx_http_xray_ctx_t    *xctx = ngx_http_get_module_ctx(r, ngx_http_xray_module);

    if (!xctx || !xctx->xray) {
        return;
    }

    if (xctx->xray_level < level) {
        return;
    }

    va_start(args, format);
    xctx->xray->last = ngx_vslprintf(xctx->xray->last, xctx->xray->end, format, args);
    va_end(args);

    if (xctx->xray->last < xctx->xray->end) {
        *xctx->xray->last++ = '\n';
    }
}

// Call to initialize xray buffer for this request
ngx_int_t
ngx_http_xray_init(ngx_http_request_t *r, ngx_int_t level)
{
    ngx_http_xray_main_conf_t *xmcf = ngx_http_get_module_main_conf(r, ngx_http_xray_module);
    ngx_http_xray_ctx_t       *xctx = ngx_http_get_module_ctx(r, ngx_http_xray_module);

    if (xctx == NULL) {
        xctx = ngx_pcalloc(r->pool, sizeof(ngx_http_xray_ctx_t));
    }

    if (xctx == NULL) {
        ngx_log_error(NGX_LOG_WARN, r->connection->log, 0,
            "%s: Error setting context", __func__);

        return NGX_ERROR;
    }

    ngx_http_set_ctx(r, xctx, ngx_http_xray_module);
    xctx->xray_level = level;

    if (level == 0) {
        return NGX_OK;
    }

    xctx->xray = ngx_create_temp_buf(r->pool, xmcf->xray_buffer_size);
    if (xctx->xray == NULL) {
        ngx_log_error(NGX_LOG_WARN, r->connection->log, 0,
            "%s: failed to ngx_create_temp_buf %z bytes", __func__, xmcf->xray_buffer_size);

        return NGX_ERROR;
    }

    ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
        "%s: level=%i", __func__, level);

    return NGX_OK;
}

// truncates the in string to the maximum length of out_cap, replacing the last 3 characters
// with an ellipsis if possible
u_char *
ngx_http_xray_truncate_with_ellipsis(ngx_str_t in, u_char *p, size_t cap, size_t width)
{
    int i;
    u_char *pend;

    if (width > cap) {
        width = cap;
    }
    pend = p + width;

    if (in.len > (size_t) (pend-p))  {

        for (i=0; i < 3 && pend > p; i++) {
            pend--;
            *pend = '.';
        }

    } else {

        pend = p + in.len;

    }

    return ngx_copy(p, in.data, pend-p);
}
