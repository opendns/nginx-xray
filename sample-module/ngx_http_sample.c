// Copyright (C) OpenDNS Inc.

#include <nginx.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include "ngx_http_xray.h"

static ngx_int_t
ngx_http_postconfig(ngx_conf_t *cf);

static ngx_http_module_t  ngx_http_sample_module_ctx = {
    NULL,         /* preconfiguration */
    ngx_http_postconfig, /* postconfiguration */

    NULL,         /* create main configuration */
    NULL,         /* init main configuration */

    NULL,         /* create server configuration */
    NULL,         /* merge server configuration */

    NULL,         /* create location configuration */
    NULL          /* merge location configuration */
};


ngx_module_t  ngx_http_sample_module = {
    NGX_MODULE_V1,
    &ngx_http_sample_module_ctx,/* module context */
    NULL,                       /* module directives */
    NGX_HTTP_MODULE,            /* module type */
    NULL,                       /* init master */
    NULL,                       /* init module */
    NULL,                       /* init worker */
    NULL,                       /* init thread */
    NULL,                       /* exit thread */
    NULL,                       /* exit worker */
    NULL,                       /* exit master */
    NGX_MODULE_V1_PADDING
};

static ngx_int_t
ngx_http_get_input_header(ngx_http_request_t *r, ngx_str_t *key_in, ngx_str_t *value_out)
{
    ngx_table_elt_t             *h;
    ngx_list_part_t             *part;
    ngx_int_t                    i;

    XRAYF(r, NGX_LOG_INFO, "get_header() key: %V", key_in);

    part = &r->headers_in.headers.part;
    h = part->elts;

    for (i = 0; /* void */; i++) {
        if (i >= (int) part->nelts) {
            if (part->next == NULL) {
                break;
            }
            part = part->next;
            h = part->elts;
            i = 0;
        }

        XRAYF(r, NGX_LOG_INFO, "i: %i, part: %p // '%V'",
            (int) i, part, &h[i].key);

        if (h[i].key.len == key_in->len
            && ngx_strncasecmp(h[i].key.data, key_in->data, key_in->len) == 0) {
            *value_out = h[i].value;
            return NGX_OK;
        }
    }

    return NGX_ERROR;
}

ngx_int_t
ngx_http_sample_rewrite_phase_handler(ngx_http_request_t *r)
{
    ngx_str_t v;
    ngx_str_t header_xray = ngx_string("X-Ray-Level");
    ngx_str_t header_foo  = ngx_string("X-Foo");

    // For this example, we look for a special header
    // that enables xray trace output.
    v.len = 0;
    ngx_http_get_input_header(r, &header_xray, &v);
    if (v.len > 0) {
        ngx_http_xray_init(r, ngx_atoi(v.data, v.len));

        // If there is a location defined for error pages,
        // nginx does a recursive internal redirect
        // when returning an error. All contexts are cleared
        // and xray data gets lost (see ngx_http_internal_redirect()
        // in ngx_http_core_module.c).

        // This flag disables the recursive error pages and the
        // generic/canned error page appears incl. the xray data
        // (see ngx_http_special_response.c).
        r->error_page = 1;
    }

    XLOGF(r, NGX_LOG_WARN, "Hello, X-Ray!");

    v.len = 0;
    ngx_http_get_input_header(r, &header_foo, &v);
    if (v.len > 0) {
        XRAYF(r, NGX_LOG_INFO, "Some important business logic");
        XRAYF(r, NGX_LOG_ERR, "             ,,__");
        XRAYF(r, NGX_LOG_ERR, "    ..  ..   / o._)                   .---.");
        XRAYF(r, NGX_LOG_ERR, "   /--'/--\\  \\-'||        .----.    .'     '.");
        XRAYF(r, NGX_LOG_ERR, "  /        \\_/ / |      .'      '..'         '-.");
        XRAYF(r, NGX_LOG_ERR, ".'\\  \\__\\  __.'.'     .'          i-._");
        XRAYF(r, NGX_LOG_ERR, "  )\\ |  )\\ |      _.'");
        XRAYF(r, NGX_LOG_ERR, " // \\\\ // \\\\");
        XRAYF(r, NGX_LOG_ERR, "||_  \\\\|_  \\\\_");
        XRAYF(r, NGX_LOG_ERR, "'--' '--'' '--' ");
    }

    // call next handler
    return NGX_DECLINED;
}

static ngx_int_t
ngx_http_postconfig(ngx_conf_t *cf)
{
    ngx_http_handler_pt *h;
    ngx_http_core_main_conf_t *cmcf =
        ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_REWRITE_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }
    *h = ngx_http_sample_rewrite_phase_handler;

    return NGX_OK;
}
