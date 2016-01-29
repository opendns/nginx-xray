// Copyright (C) OpenDNS Inc.

#ifndef __NGX_HTTP_XRAY_H__
#define __NGX_HTTP_XRAY_H__

#ifndef NGX_HAVE_VARIADIC_MACROS
#error "need NGX_HAVE_VARIADIC_MACROS for Xray"
#endif

// Severity Level should be one of nginx's standard error log
// values. (ngx_log.h: NGX_LOG_STDERR=0 - NGX_LOG_INFO=7)

// Add Xray line
#define XRAYF(_r, _severity_level, _format, ...) \
    ngx_http_xray_line(_r, _severity_level, "%i %s: " _format, _severity_level, __func__, ##__VA_ARGS__)


// Add Xray line and also log to error_log
#define XLOGF(_r, _severity_level, _format, ...) \
    { \
        XRAYF(_r, _severity_level, _format, ##__VA_ARGS__); \
        ngx_log_error(_severity_level, ngx_cycle->log, 0, "%s: " _format, __func__, ##__VA_ARGS__); \
    }

// Call to initialize xray for a request
ngx_int_t ngx_http_xray_init(ngx_http_request_t *r, ngx_int_t level);


// Append log line to xray buffer.  Should be used with the macros above.
void ngx_http_xray_line(ngx_http_request_t *r, const ngx_int_t level,
    const char *format, ...);


// truncates the in string to the maximum length of out_cap, replacing the last
// 3 characters with an ellipsis if possible
u_char *ngx_http_xray_truncate_with_ellipsis(ngx_str_t in, u_char *p,
    size_t cap, size_t width);

#endif /* __NGX_HTTP_XRAY_H__ */
