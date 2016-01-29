# nginx-xray

X-Ray is a simple yet efficient way of debugging issues in production.

Normally the error log is expected to have low verbosity, with only
critical errors being printed to disk on high volume servers.

With x-ray, we can query the server for more verbose error_log style
instrumentation, and gain insight on what decisions are being made and what
code path is executed.

X-Ray should be initialized in response to e.g. a special header,
or based on client address, or both. When initialized, log lines
are added to an in-memory buffer which is then pre-pended to the
response.

See [ngx_http_xray.h](nginx-xray/ngx_http_xray.h) for function reference
and available macros to get started.
Log/Severity levels mirror nginx's error log levels (NGX_LOG_STDERR=0 - NGX_LOG_INFO=7).

# sample-module

The sample module demonstrates basic xray usage in a custom module.


    $ curl http://server/foo/bar
    <html>
    <head><title>404 Not Found</title></head>
    <body bgcolor="white">
    <center><h1>404 Not Found</h1></center>
    <hr><center>nginx/1.6.0</center>
    </body>
    </html>

    $ curl -H "X-Ray-Level: 9" http://server/foo/bar
    5 ngx_http_sample_rewrite_phase_handler: Hello, X-Ray!
    7 ngx_http_get_input_header: get_header() key: X-Foo
    7 ngx_http_get_input_header: i: 0, part: 000000000091A108 // 'User-Agent'
    7 ngx_http_get_input_header: i: 1, part: 000000000091A108 // 'Host'
    7 ngx_http_get_input_header: i: 2, part: 000000000091A108 // 'Accept'
    7 ngx_http_get_input_header: i: 3, part: 000000000091A108 // 'X-Ray-Level'
    <html>
    <head><title>404 Not Found</title></head>
    <body bgcolor="white">
    <center><h1>404 Not Found</h1></center>
    <hr><center>nginx/1.6.0</center>
    </body>
    </html>

    $ curl  -H "X-Foo: on" -H "X-Ray-Level: 5" http://server/foo/bar
    5 ngx_http_sample_rewrite_phase_handler: Hello, X-Ray!
    4 ngx_http_sample_rewrite_phase_handler:              ,,__
    4 ngx_http_sample_rewrite_phase_handler:     ..  ..   / o._)                   .---.
    4 ngx_http_sample_rewrite_phase_handler:    /--'/--\  \-'||        .----.    .'     '.
    4 ngx_http_sample_rewrite_phase_handler:   /        \_/ / |      .'      '..'         '-.
    4 ngx_http_sample_rewrite_phase_handler: .'\  \__\  __.'.'     .'          ì-._
    4 ngx_http_sample_rewrite_phase_handler:   )\ |  )\ |      _.'
    4 ngx_http_sample_rewrite_phase_handler:  // \\ // \\
    4 ngx_http_sample_rewrite_phase_handler: ||_  \\|_  \\_
    4 ngx_http_sample_rewrite_phase_handler: '--' '--'' '--'
    <html>
    <head><title>404 Not Found</title></head>
    <body bgcolor="white">
    <center><h1>404 Not Found</h1></center>
    <hr><center>nginx/1.6.0</center>
    </body>
    </html>

# Copyright and License

Copyright (C) 2014 OpenDNS Inc.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

  * Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above
copyright notice, this list of conditions and the following disclaimer
in the documentation and/or other materials provided with the
distribution.

  * Neither the name of OpenDNS Inc. nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
