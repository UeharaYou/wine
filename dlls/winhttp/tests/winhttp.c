/*
 * WinHTTP - tests
 *
 * Copyright 2008 Google (Zac Brown)
 * Copyright 2015 Dmitry Timoshkov
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#define COBJMACROS
#include <stdarg.h>
#include <windef.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <winhttp.h>
#include <wincrypt.h>
#include <winreg.h>
#include <initguid.h>
#include <httprequest.h>
#include <httprequestid.h>

#include "wine/test.h"

DEFINE_GUID(GUID_NULL,0,0,0,0,0,0,0,0,0,0,0);

static DWORD (WINAPI *pWinHttpWebSocketClose)(HINTERNET,USHORT,void*,DWORD);
static HINTERNET (WINAPI *pWinHttpWebSocketCompleteUpgrade)(HINTERNET,DWORD_PTR);
static DWORD (WINAPI *pWinHttpWebSocketQueryCloseStatus)(HINTERNET,USHORT*,void*,DWORD,DWORD*);
static DWORD (WINAPI *pWinHttpWebSocketReceive)(HINTERNET,void*,DWORD,DWORD*,WINHTTP_WEB_SOCKET_BUFFER_TYPE*);
static DWORD (WINAPI *pWinHttpWebSocketSend)(HINTERNET,WINHTTP_WEB_SOCKET_BUFFER_TYPE,void*,DWORD);
static DWORD (WINAPI *pWinHttpWebSocketShutdown)(HINTERNET,USHORT,void*,DWORD);

static BOOL proxy_active(void)
{
    WINHTTP_PROXY_INFO proxy_info;
    BOOL active = FALSE;

    SetLastError(0xdeadbeef);
    if (WinHttpGetDefaultProxyConfiguration(&proxy_info))
    {
        ok( GetLastError() == ERROR_SUCCESS || broken(GetLastError() == 0xdeadbeef) /* < win7 */,
            "got %lu\n", GetLastError() );
        active = (proxy_info.lpszProxy != NULL);
        if (active)
            GlobalFree(proxy_info.lpszProxy);
        if (proxy_info.lpszProxyBypass != NULL)
            GlobalFree(proxy_info.lpszProxyBypass);
    }
    else
       active = FALSE;

    return active;
}

static void test_WinHttpQueryOption(void)
{
    BOOL ret;
    HINTERNET session, request, connection;
    DWORD feature, size;

    SetLastError(0xdeadbeef);
    session = WinHttpOpen(L"winetest", 0, 0, 0, 0);
    ok( session != NULL, "WinHttpOpen failed to open session, error %lu\n", GetLastError() );

    SetLastError(0xdeadbeef);
    ret = WinHttpQueryOption(session, WINHTTP_OPTION_REDIRECT_POLICY, NULL, NULL);
    ok( !ret, "should fail to set redirect policy %lu\n", GetLastError() );
    ok( GetLastError() == ERROR_INVALID_PARAMETER, "expected ERROR_INVALID_PARAMETER, got %lu\n", GetLastError() );

    size = 0xdeadbeef;
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryOption(session, WINHTTP_OPTION_REDIRECT_POLICY, NULL, &size);
    ok(!ret, "should fail to query option\n");
    ok( GetLastError() == ERROR_INSUFFICIENT_BUFFER, "expected ERROR_INSUFFICIENT_BUFFER, got %lu\n", GetLastError() );
    ok( size == 4, "expected 4, got %lu\n", size );

    feature = 0xdeadbeef;
    size = sizeof(feature) - 1;
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryOption(session, WINHTTP_OPTION_REDIRECT_POLICY, &feature, &size);
    ok(!ret, "should fail to query option\n");
    ok( GetLastError() == ERROR_INSUFFICIENT_BUFFER, "expected ERROR_INSUFFICIENT_BUFFER, got %lu\n", GetLastError() );
    ok( size == 4, "expected 4, got %lu\n", size );

    feature = 0xdeadbeef;
    size = sizeof(feature) + 1;
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryOption(session, WINHTTP_OPTION_WORKER_THREAD_COUNT, &feature, &size);
    ok(ret, "failed to query option %lu\n", GetLastError());
    ok(GetLastError() == ERROR_SUCCESS, "got %lu\n", GetLastError());
    ok(size == sizeof(feature), "WinHttpQueryOption should set the size: %lu\n", size);
    ok(feature == 0, "got unexpected WINHTTP_OPTION_WORKER_THREAD_COUNT %#lx\n", feature);

    feature = 0xdeadbeef;
    size = sizeof(feature) + 1;
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryOption(session, WINHTTP_OPTION_REDIRECT_POLICY, &feature, &size);
    ok(ret, "failed to query option %lu\n", GetLastError());
    ok(GetLastError() == ERROR_SUCCESS || broken(GetLastError() == 0xdeadbeef) /* < win7 */,
       "got %lu\n", GetLastError());
    ok(size == sizeof(feature), "WinHttpQueryOption should set the size: %lu\n", size);
    ok(feature == WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP,
       "expected WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP, got %#lx\n", feature);

    SetLastError(0xdeadbeef);
    ret = WinHttpSetOption(session, WINHTTP_OPTION_REDIRECT_POLICY, NULL, sizeof(feature));
    ok(!ret, "should fail to set redirect policy %lu\n", GetLastError());
    ok(GetLastError() == ERROR_INVALID_PARAMETER,
       "expected ERROR_INVALID_PARAMETER, got %lu\n", GetLastError());

    feature = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    SetLastError(0xdeadbeef);
    ret = WinHttpSetOption(session, WINHTTP_OPTION_REDIRECT_POLICY, &feature, sizeof(feature) - 1);
    ok(!ret, "should fail to set redirect policy %lu\n", GetLastError());
    ok(GetLastError() == ERROR_INSUFFICIENT_BUFFER,
       "expected ERROR_INSUFFICIENT_BUFFER, got %lu\n", GetLastError());

    feature = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    SetLastError(0xdeadbeef);
    ret = WinHttpSetOption(session, WINHTTP_OPTION_REDIRECT_POLICY, &feature, sizeof(feature) + 1);
    ok(!ret, "should fail to set redirect policy %lu\n", GetLastError());
    ok(GetLastError() == ERROR_INSUFFICIENT_BUFFER,
       "expected ERROR_INSUFFICIENT_BUFFER, got %lu\n", GetLastError());

    feature = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    SetLastError(0xdeadbeef);
    ret = WinHttpSetOption(session, WINHTTP_OPTION_REDIRECT_POLICY, &feature, sizeof(feature));
    ok(ret, "failed to set redirect policy %lu\n", GetLastError());

    feature = 0xdeadbeef;
    size = sizeof(feature);
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryOption(session, WINHTTP_OPTION_REDIRECT_POLICY, &feature, &size);
    ok(ret, "failed to query option %lu\n", GetLastError());
    ok(feature == WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS,
       "expected WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS, got %#lx\n", feature);

    feature = WINHTTP_DISABLE_COOKIES;
    SetLastError(0xdeadbeef);
    ret = WinHttpSetOption(session, WINHTTP_OPTION_DISABLE_FEATURE, &feature, sizeof(feature));
    ok(!ret, "should fail to set disable feature for a session\n");
    ok(GetLastError() == ERROR_WINHTTP_INCORRECT_HANDLE_TYPE,
       "expected ERROR_WINHTTP_INCORRECT_HANDLE_TYPE, got %lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    connection = WinHttpConnect(session, L"test.winehq.org", INTERNET_DEFAULT_HTTP_PORT, 0);
    ok(connection != NULL, "WinHttpConnect failed to open a connection, error: %lu\n", GetLastError());

    feature = WINHTTP_DISABLE_COOKIES;
    SetLastError(0xdeadbeef);
    ret = WinHttpSetOption(connection, WINHTTP_OPTION_DISABLE_FEATURE, &feature, sizeof(feature));
    ok(!ret, "should fail to set disable feature for a connection\n");
    ok(GetLastError() == ERROR_WINHTTP_INCORRECT_HANDLE_TYPE,
       "expected ERROR_WINHTTP_INCORRECT_HANDLE_TYPE, got %lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    request = WinHttpOpenRequest(connection, NULL, NULL, NULL, WINHTTP_NO_REFERER,
                                 WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (request == NULL && GetLastError() == ERROR_WINHTTP_NAME_NOT_RESOLVED)
    {
        skip("Network unreachable, skipping the test\n");
        goto done;
    }

    feature = 0xdeadbeef;
    size = sizeof(feature);
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryOption(connection, WINHTTP_OPTION_WORKER_THREAD_COUNT, &feature, &size);
    ok(ret, "query WINHTTP_OPTION_WORKER_THREAD_COUNT failed for a request\n");
    ok(GetLastError() == ERROR_SUCCESS, "got unexpected error %lu\n", GetLastError());
    ok(size == sizeof(feature), "WinHttpQueryOption should set the size: %lu\n", size);
    ok(feature == 0, "got unexpected WINHTTP_OPTION_WORKER_THREAD_COUNT %#lx\n", feature);

    feature = 0xdeadbeef;
    size = sizeof(feature);
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryOption(request, WINHTTP_OPTION_WORKER_THREAD_COUNT, &feature, &size);
    ok(ret, "query WINHTTP_OPTION_WORKER_THREAD_COUNT failed for a request\n");
    ok(GetLastError() == ERROR_SUCCESS, "got unexpected error %lu\n", GetLastError());
    ok(size == sizeof(feature), "WinHttpQueryOption should set the size: %lu\n", size);
    ok(feature == 0, "got unexpected WINHTTP_OPTION_WORKER_THREAD_COUNT %#lx\n", feature);

    feature = 0xdeadbeef;
    size = sizeof(feature);
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryOption(request, WINHTTP_OPTION_DISABLE_FEATURE, &feature, &size);
    ok(!ret, "should fail to query disable feature for a request\n");
    ok(GetLastError() == ERROR_INVALID_PARAMETER,
       "expected ERROR_INVALID_PARAMETER, got %lu\n", GetLastError());

    feature = 0;
    size = sizeof(feature);
    SetLastError(0xdeadbeef);
    ret = WinHttpSetOption(request, WINHTTP_OPTION_DISABLE_FEATURE, &feature, sizeof(feature));
    ok(ret, "failed to set feature %lu\n", GetLastError());

    feature = 0xffffffff;
    size = sizeof(feature);
    SetLastError(0xdeadbeef);
    ret = WinHttpSetOption(request, WINHTTP_OPTION_DISABLE_FEATURE, &feature, sizeof(feature));
    ok(ret, "failed to set feature %lu\n", GetLastError());

    feature = WINHTTP_DISABLE_COOKIES;
    size = sizeof(feature);
    SetLastError(0xdeadbeef);
    ret = WinHttpSetOption(request, WINHTTP_OPTION_DISABLE_FEATURE, &feature, sizeof(feature));
    ok(ret, "failed to set feature %lu\n", GetLastError());

    size = 0;
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryOption(request, WINHTTP_OPTION_DISABLE_FEATURE, NULL, &size);
    ok(!ret, "should fail to query disable feature for a request\n");
    ok(GetLastError() == ERROR_INVALID_PARAMETER,
       "expected ERROR_INVALID_PARAMETER, got %lu\n", GetLastError());

    feature = 0xdeadbeef;
    size = sizeof(feature);
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryOption(request, WINHTTP_OPTION_ENABLE_FEATURE, &feature, &size);
    ok(!ret, "should fail to query enabled features for a request\n");
    ok(feature == 0xdeadbeef, "expect feature 0xdeadbeef, got %#lx\n", feature);
    ok(GetLastError() == ERROR_INVALID_PARAMETER, "expected ERROR_INVALID_PARAMETER, got %lu\n", GetLastError());

    feature = WINHTTP_ENABLE_SSL_REVOCATION;
    SetLastError(0xdeadbeef);
    ret = WinHttpSetOption(request, WINHTTP_OPTION_ENABLE_FEATURE, 0, sizeof(feature));
    ok(!ret, "should fail to enable WINHTTP_ENABLE_SSL_REVOCATION with invalid parameters\n");
    ok(GetLastError() == ERROR_INVALID_PARAMETER, "expected ERROR_INVALID_PARAMETER, got %lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = WinHttpSetOption(request, WINHTTP_OPTION_ENABLE_FEATURE, &feature, 0);
    ok(!ret, "should fail to enable WINHTTP_ENABLE_SSL_REVOCATION with invalid parameters\n");
    ok(GetLastError() == ERROR_INVALID_PARAMETER, "expected ERROR_INVALID_PARAMETER, got %lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = WinHttpSetOption(request, WINHTTP_OPTION_ENABLE_FEATURE, &feature, sizeof(feature));
    ok(ret, "failed to set feature\n");
    ok(GetLastError() == NO_ERROR || broken(GetLastError() == 0xdeadbeef), /* Doesn't set error code on Vista or older */
       "expected NO_ERROR, got %lu\n", GetLastError());

    feature = 0xdeadbeef;
    SetLastError(0xdeadbeef);
    ret = WinHttpSetOption(request, WINHTTP_OPTION_ENABLE_FEATURE, &feature, sizeof(feature));
    ok(!ret, "should fail to enable WINHTTP_ENABLE_SSL_REVOCATION with invalid parameters\n");
    ok(GetLastError() == ERROR_INVALID_PARAMETER, "expected ERROR_INVALID_PARAMETER, got %lu\n", GetLastError());

    feature = 6;
    size = sizeof(feature);
    ret = WinHttpSetOption(request, WINHTTP_OPTION_CONNECT_RETRIES, &feature, sizeof(feature));
    ok(ret, "failed to set WINHTTP_OPTION_CONNECT_RETRIES %lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = WinHttpCloseHandle(request);
    ok(ret, "WinHttpCloseHandle failed on closing request: %lu\n", GetLastError());

done:
    SetLastError(0xdeadbeef);
    ret = WinHttpCloseHandle(connection);
    ok(ret, "WinHttpCloseHandle failed on closing connection: %lu\n", GetLastError());
    SetLastError(0xdeadbeef);
    ret = WinHttpCloseHandle(session);
    ok(ret, "WinHttpCloseHandle failed on closing session: %lu\n", GetLastError());
}

static void test_WinHttpOpenRequest (void)
{
    BOOL ret;
    HINTERNET session, request, connection;
    DWORD err;

    SetLastError(0xdeadbeef);
    session = WinHttpOpen(L"winetest", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    err = GetLastError();
    ok(session != NULL, "WinHttpOpen failed to open session.\n");
    ok(err == ERROR_SUCCESS, "got %lu\n", err);

    /* Test with a bad server name */
    SetLastError(0xdeadbeef);
    connection = WinHttpConnect(session, NULL, INTERNET_DEFAULT_HTTP_PORT, 0);
    err = GetLastError();
    ok (connection == NULL, "WinHttpConnect succeeded in opening connection to NULL server argument.\n");
    ok(err == ERROR_INVALID_PARAMETER, "Expected ERROR_INVALID_PARAMETER, got %lu.\n", err);

    /* Test with a valid server name */
    SetLastError(0xdeadbeef);
    connection = WinHttpConnect (session, L"test.winehq.org", INTERNET_DEFAULT_HTTP_PORT, 0);
    err = GetLastError();
    ok(connection != NULL, "WinHttpConnect failed to open a connection, error: %lu.\n", err);
    ok(err == ERROR_SUCCESS || broken(err == WSAEINVAL) /* < win7 */, "got %lu\n", err);

    SetLastError(0xdeadbeef);
    request = WinHttpOpenRequest(connection, NULL, NULL, NULL, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    err = GetLastError();
    if (request == NULL && err == ERROR_WINHTTP_NAME_NOT_RESOLVED)
    {
        skip("Network unreachable, skipping.\n");
        goto done;
    }
    ok(request != NULL, "WinHttpOpenrequest failed to open a request, error: %lu.\n", err);
    ok(err == ERROR_SUCCESS, "got %lu\n", err);

    SetLastError(0xdeadbeef);
    ret = WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, NULL, 0, 0, 0);
    err = GetLastError();
    if (!ret && (err == ERROR_WINHTTP_CANNOT_CONNECT || err == ERROR_WINHTTP_TIMEOUT))
    {
        skip("Connection failed, skipping.\n");
        goto done;
    }
    ok(ret, "WinHttpSendRequest failed: %lu\n", err);
    ok(err == ERROR_SUCCESS, "got %lu\n", err);

    SetLastError(0xdeadbeef);
    ret = WinHttpCloseHandle(request);
    err = GetLastError();
    ok(ret, "WinHttpCloseHandle failed on closing request, got %lu.\n", err);
    ok(err == ERROR_SUCCESS, "got %lu\n", err);

 done:
    ret = WinHttpCloseHandle(connection);
    ok(ret == TRUE, "WinHttpCloseHandle failed on closing connection, got %d.\n", ret);
    ret = WinHttpCloseHandle(session);
    ok(ret == TRUE, "WinHttpCloseHandle failed on closing session, got %d.\n", ret);

}

static void test_empty_headers_param(void)
{
    HINTERNET ses, con, req;
    DWORD err;
    BOOL ret;

    ses = WinHttpOpen(L"winetest", 0, NULL, NULL, 0);
    ok(ses != NULL, "failed to open session %lu\n", GetLastError());

    con = WinHttpConnect(ses, L"test.winehq.org", 80, 0);
    ok(con != NULL, "failed to open a connection %lu\n", GetLastError());

    req = WinHttpOpenRequest(con, NULL, NULL, NULL, NULL, NULL, 0);
    ok(req != NULL, "failed to open a request %lu\n", GetLastError());

    ret = WinHttpSendRequest(req, L"", 0, NULL, 0, 0, 0);
    err = GetLastError();
    if (!ret && (err == ERROR_WINHTTP_CANNOT_CONNECT || err == ERROR_WINHTTP_TIMEOUT))
    {
        skip("connection failed, skipping\n");
        goto done;
    }
    ok(ret, "failed to send request %lu\n", GetLastError());

 done:
    WinHttpCloseHandle(req);
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
}

static void test_WinHttpSendRequest (void)
{
    static const WCHAR content_type[] = L"Content-Type: application/x-www-form-urlencoded";
    static char post_data[] = "mode=Test";
    static const char test_post[] = "mode => Test\0\n";
    HINTERNET session, request, connection;
    DWORD header_len, optional_len, total_len, bytes_rw, size, err, disable, len;
    DWORD_PTR context;
    BOOL ret;
    CHAR buffer[256];
    WCHAR method[8];
    int i;

    header_len = -1L;
    total_len = optional_len = sizeof(post_data);
    memset(buffer, 0xff, sizeof(buffer));

    session = WinHttpOpen(L"winetest", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    ok(session != NULL, "WinHttpOpen failed to open session.\n");

    connection = WinHttpConnect (session, L"test.winehq.org", INTERNET_DEFAULT_HTTP_PORT, 0);
    ok(connection != NULL, "WinHttpConnect failed to open a connection, error: %lu\n", GetLastError());

    request = WinHttpOpenRequest(connection, L"POST", L"tests/post.php", NULL, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_BYPASS_PROXY_CACHE);
    if (request == NULL && GetLastError() == ERROR_WINHTTP_NAME_NOT_RESOLVED)
    {
        skip("Network unreachable, skipping.\n");
        goto done;
    }
    ok(request != NULL, "WinHttpOpenrequest failed to open a request, error: %lu\n", GetLastError());
    if (!request) goto done;

    method[0] = 0;
    len = sizeof(method);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_REQUEST_METHOD, NULL, method, &len, NULL);
    ok(ret, "got %lu\n", GetLastError());
    ok(len == lstrlenW(L"POST") * sizeof(WCHAR), "got %lu\n", len);
    ok(!lstrcmpW(method, L"POST"), "got %s\n", wine_dbgstr_w(method));

    context = 0xdeadbeef;
    ret = WinHttpSetOption(request, WINHTTP_OPTION_CONTEXT_VALUE, &context, sizeof(context));
    ok(ret, "WinHttpSetOption failed: %lu\n", GetLastError());

    /* writing more data than promised by the content-length header causes an error when the connection
       is reused, so disable keep-alive */
    disable = WINHTTP_DISABLE_KEEP_ALIVE;
    ret = WinHttpSetOption(request, WINHTTP_OPTION_DISABLE_FEATURE, &disable, sizeof(disable));
    ok(ret, "WinHttpSetOption failed: %lu\n", GetLastError());

    context++;
    ret = WinHttpSendRequest(request, content_type, header_len, post_data, optional_len, total_len, context);
    err = GetLastError();
    if (!ret && (err == ERROR_WINHTTP_CANNOT_CONNECT || err == ERROR_WINHTTP_TIMEOUT))
    {
        skip("connection failed, skipping\n");
        goto done;
    }
    ok(ret == TRUE, "WinHttpSendRequest failed: %lu\n", GetLastError());

    context = 0;
    size = sizeof(context);
    ret = WinHttpQueryOption(request, WINHTTP_OPTION_CONTEXT_VALUE, &context, &size);
    ok(ret, "WinHttpQueryOption failed: %lu\n", GetLastError());
    ok(context == 0xdeadbef0, "expected 0xdeadbef0, got %#Ix\n", context);

    for (i = 3; post_data[i]; i++)
    {
        bytes_rw = -1;
        SetLastError(0xdeadbeef);
        ret = WinHttpWriteData(request, &post_data[i], 1, &bytes_rw);
        if (ret)
        {
          ok(GetLastError() == ERROR_SUCCESS, "Expected ERROR_SUCCESS got %lu\n", GetLastError());
          ok(bytes_rw == 1, "WinHttpWriteData failed, wrote %lu bytes instead of 1 byte\n", bytes_rw);
        }
        else /* Since we already passed all optional data in WinHttpSendRequest Win7 fails our WinHttpWriteData call */
        {
          ok(GetLastError() == ERROR_INVALID_PARAMETER, "Expected ERROR_INVALID_PARAMETER got %lu\n", GetLastError());
          ok(bytes_rw == -1, "Expected bytes_rw to remain unchanged.\n");
        }
    }

    SetLastError(0xdeadbeef);
    ret = WinHttpReceiveResponse(request, NULL);
    ok(GetLastError() == ERROR_SUCCESS || broken(GetLastError() == ERROR_NO_TOKEN) /* < win7 */,
       "Expected ERROR_SUCCESS got %lu\n", GetLastError());
    ok(ret == TRUE, "WinHttpReceiveResponse failed: %lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_ORIG_URI, NULL, NULL, &len, NULL);
    ok(!ret && GetLastError() == ERROR_WINHTTP_HEADER_NOT_FOUND, "got %lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_MAX + 1, NULL, NULL, &len, NULL);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "got %lu\n", GetLastError());

    bytes_rw = -1;
    ret = WinHttpReadData(request, buffer, sizeof(buffer) - 1, &bytes_rw);
    ok(ret == TRUE, "WinHttpReadData failed: %lu\n", GetLastError());

    ok(bytes_rw == sizeof(test_post) - 1, "Read %lu bytes\n", bytes_rw);
    ok(!memcmp(buffer, test_post, sizeof(test_post) - 1), "Data read did not match.\n");

 done:
    ret = WinHttpCloseHandle(request);
    ok(ret == TRUE, "WinHttpCloseHandle failed on closing request, got %d.\n", ret);
    ret = WinHttpCloseHandle(connection);
    ok(ret == TRUE, "WinHttpCloseHandle failed on closing connection, got %d.\n", ret);
    ret = WinHttpCloseHandle(session);
    ok(ret == TRUE, "WinHttpCloseHandle failed on closing session, got %d.\n", ret);
}

static void test_connect_error(void)
{
    static const WCHAR content_type[] = L"Content-Type: application/x-www-form-urlencoded";
    DWORD header_len, optional_len, total_len, err, t1, t2;
    HINTERNET session, request, connection;
    static char post_data[] = "mode=Test";
    BOOL ret;

    header_len = ~0u;
    total_len = optional_len = sizeof(post_data);

    session = WinHttpOpen(L"winetest", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    ok(!!session, "WinHttpOpen failed to open session.\n");

    connection = WinHttpConnect (session, L"127.0.0.1", 12345, 0);
    ok(!!connection, "WinHttpConnect failed to open a connection, error %lu.\n", GetLastError());

    request = WinHttpOpenRequest(connection, L"POST", L"tests/post.php", NULL, WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_BYPASS_PROXY_CACHE);
    ok(!!request, "WinHttpOpenrequest failed to open a request, error: %lu\n", GetLastError());

    t1 = GetTickCount();
    ret = WinHttpSendRequest(request, content_type, header_len, post_data, optional_len, total_len, 0);
    t2 = GetTickCount();
    err = GetLastError();
    ok(!ret, "WinHttpSendRequest() succeeded.\n");
    ok(err == ERROR_WINHTTP_CANNOT_CONNECT, "Got unexpected err %lu.\n", err);
    ok(t2 - t1 < 5000, "Unexpected connect failure delay %lums.\n", t2 - t1);

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
}

static void test_WinHttpTimeFromSystemTime(void)
{
    BOOL ret;
    static const SYSTEMTIME time = {2008, 7, 1, 28, 10, 5, 52, 0};
    WCHAR time_string[WINHTTP_TIME_FORMAT_BUFSIZE+1];
    DWORD err;

    SetLastError(0xdeadbeef);
    ret = WinHttpTimeFromSystemTime(&time, NULL);
    err = GetLastError();
    ok(!ret, "WinHttpTimeFromSystemTime succeeded\n");
    ok(err == ERROR_INVALID_PARAMETER, "got %lu\n", err);

    SetLastError(0xdeadbeef);
    ret = WinHttpTimeFromSystemTime(NULL, time_string);
    err = GetLastError();
    ok(!ret, "WinHttpTimeFromSystemTime succeeded\n");
    ok(err == ERROR_INVALID_PARAMETER, "got %lu\n", err);

    SetLastError(0xdeadbeef);
    ret = WinHttpTimeFromSystemTime(&time, time_string);
    err = GetLastError();
    ok(ret, "WinHttpTimeFromSystemTime failed: %lu\n", err);
    ok(err == ERROR_SUCCESS || broken(err == 0xdeadbeef) /* < win7 */, "got %lu\n", err);
    ok(!memcmp(time_string, L"Mon, 28 Jul 2008 10:05:52 GMT", sizeof(L"Mon, 28 Jul 2008 10:05:52 GMT")),
        "Time string returned did not match expected time string.\n");
}

static void test_WinHttpTimeToSystemTime(void)
{
    BOOL ret;
    SYSTEMTIME time;
    static const SYSTEMTIME expected_time = {2008, 7, 1, 28, 10, 5, 52, 0};
    DWORD err;

    SetLastError(0xdeadbeef);
    ret = WinHttpTimeToSystemTime(L"Mon, 28 Jul 2008 10:05:52 GMT\n", NULL);
    err = GetLastError();
    ok(!ret, "WinHttpTimeToSystemTime succeeded\n");
    ok(err == ERROR_INVALID_PARAMETER, "got %lu\n", err);

    SetLastError(0xdeadbeef);
    ret = WinHttpTimeToSystemTime(NULL, &time);
    err = GetLastError();
    ok(!ret, "WinHttpTimeToSystemTime succeeded\n");
    ok(err == ERROR_INVALID_PARAMETER, "got %lu\n", err);

    SetLastError(0xdeadbeef);
    ret = WinHttpTimeToSystemTime(L"Mon, 28 Jul 2008 10:05:52 GMT\n", &time);
    err = GetLastError();
    ok(ret, "WinHttpTimeToSystemTime failed: %lu\n", err);
    ok(err == ERROR_SUCCESS || broken(err == 0xdeadbeef) /* < win7 */, "got %lu\n", err);
    ok(memcmp(&time, &expected_time, sizeof(SYSTEMTIME)) == 0,
        "Returned SYSTEMTIME structure did not match expected SYSTEMTIME structure.\n");

    SetLastError(0xdeadbeef);
    ret = WinHttpTimeToSystemTime(L" mon 28 jul 2008 10 05 52\n", &time);
    err = GetLastError();
    ok(ret, "WinHttpTimeToSystemTime failed: %lu\n", err);
    ok(err == ERROR_SUCCESS || broken(err == 0xdeadbeef) /* < win7 */, "got %lu\n", err);
    ok(memcmp(&time, &expected_time, sizeof(SYSTEMTIME)) == 0,
        "Returned SYSTEMTIME structure did not match expected SYSTEMTIME structure.\n");
}

static void test_WinHttpAddHeaders(void)
{
    static const WCHAR test_header_begin[] =
        {'P','O','S','T',' ','/','p','o','s','t','t','e','s','t','.','p','h','p',' ','H','T','T','P','/','1'};
    HINTERNET session, request, connection;
    BOOL ret, reverse;
    WCHAR buffer[MAX_PATH];
    WCHAR check_buffer[MAX_PATH];
    DWORD err, index, len, oldlen;

    static const WCHAR test_headers[][14] =
    {
        L"Warning:test1",
        L"Warning:test2",
        L"Warning:test3",
        L"Warning:test4",
        L"Warning:test5",
        L"Warning:test6",
        L"Warning:test7",
        L"",
        L":",
        L"a:",
        L":b",
        L"cd",
        L" e :f",
        L"field: value ",
        L"name: value",
        L"name:",
        L"g : value",
    };
    static const WCHAR test_indices[][6] =
    {
        L"test1",
        L"test2",
        L"test3",
        L"test4",
    };

    session = WinHttpOpen(L"winetest", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    ok(session != NULL, "WinHttpOpen failed to open session.\n");

    connection = WinHttpConnect (session, L"test.winehq.org", INTERNET_DEFAULT_HTTP_PORT, 0);
    ok(connection != NULL, "WinHttpConnect failed to open a connection, error: %lu\n", GetLastError());

    request = WinHttpOpenRequest(connection, L"POST", L"/posttest.php", NULL, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (request == NULL && GetLastError() == ERROR_WINHTTP_NAME_NOT_RESOLVED)
    {
        skip("Network unreachable, skipping.\n");
        goto done;
    }
    ok(request != NULL, "WinHttpOpenRequest failed to open a request, error: %lu\n", GetLastError());

    index = 0;
    len = sizeof(buffer);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                              L"Warning", buffer, &len, &index);
    ok(ret == FALSE, "WinHttpQueryHeaders unexpectedly succeeded, found 'Warning' header.\n");
    SetLastError(0xdeadbeef);
    ret = WinHttpAddRequestHeaders(request, test_headers[0], -1L, WINHTTP_ADDREQ_FLAG_ADD);
    err = GetLastError();
    ok(ret, "WinHttpAddRequestHeaders failed to add new header, got %d with error %lu\n", ret, err);
    ok(err == ERROR_SUCCESS || broken(err == 0xdeadbeef) /* < win7 */, "got %lu\n", err);

    index = 0;
    len = sizeof(buffer);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                              L"Warning", buffer, &len, &index);
    ok(ret == TRUE, "WinHttpQueryHeaders failed: %lu\n", GetLastError());
    ok(index == 1, "WinHttpQueryHeaders failed: header index not incremented\n");
    ok(!memcmp(buffer, test_indices[0], sizeof(test_indices[0])),
       "WinHttpQueryHeaders failed: incorrect string returned\n");
    ok(len == 5 * sizeof(WCHAR), "WinHttpQueryHeaders failed: invalid length returned, expected 5, got %lu\n", len);

    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                              L"Warning", buffer, &len, &index);
    ok(ret == FALSE, "WinHttpQueryHeaders unexpectedly succeeded, second index should not exist.\n");

    /* Try to fetch the header info with a buffer that's big enough to fit the
     * string but not the NULL terminator.
     */
    index = 0;
    len = 5*sizeof(WCHAR);
    memset(check_buffer, 0xab, sizeof(check_buffer));
    memcpy(buffer, check_buffer, sizeof(buffer));
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                              L"Warning", buffer, &len, &index);
    ok(ret == FALSE, "WinHttpQueryHeaders unexpectedly succeeded with a buffer that's too small.\n");
    ok(memcmp(buffer, check_buffer, sizeof(buffer)) == 0,
            "WinHttpQueryHeaders failed, modified the buffer when it should not have.\n");
    ok(len == 6 * sizeof(WCHAR), "WinHttpQueryHeaders returned invalid length, expected 12, got %lu\n", len);

    /* Try with a NULL buffer */
    index = 0;
    len = sizeof(buffer);
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_RAW_HEADERS_CRLF | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                              L"Warning", NULL, &len, &index);
    ok(ret == FALSE, "WinHttpQueryHeaders unexpectedly succeeded.\n");
    ok(GetLastError() == ERROR_INSUFFICIENT_BUFFER, "Expected ERROR_INSUFFICIENT_BUFFER, got %lu\n", GetLastError());
    ok(len > 40, "WinHttpQueryHeaders returned invalid length: expected greater than 40, got %lu\n", len);
    ok(index == 0, "WinHttpQueryHeaders incorrectly incremented header index.\n");

    /* Try with a NULL buffer and a length that's too small */
    index = 0;
    len = 10;
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_RAW_HEADERS_CRLF | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                              L"Warning", NULL, &len, &index);
    ok(ret == FALSE, "WinHttpQueryHeaders unexpectedly succeeded.\n");
    ok(GetLastError() == ERROR_INSUFFICIENT_BUFFER,
        "WinHttpQueryHeaders set incorrect error: expected ERROR_INSUFFICIENT_BUFFER, got %lu\n", GetLastError());
    ok(len > 40, "WinHttpQueryHeaders returned invalid length: expected greater than 40, got %lu\n", len);
    ok(index == 0, "WinHttpQueryHeaders incorrectly incremented header index.\n");

    index = 0;
    len = 0;
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_RAW_HEADERS_CRLF | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                              L"Warning", NULL, &len, &index);
    ok(ret == FALSE, "WinHttpQueryHeaders unexpectedly succeeded.\n");
    ok(GetLastError() == ERROR_INSUFFICIENT_BUFFER,
        "WinHttpQueryHeaders set incorrect error: expected ERROR_INSUFFICIENT_BUFFER, got %lu\n", GetLastError());
    ok(len > 40, "WinHttpQueryHeaders returned invalid length: expected greater than 40, got %lu\n", len);
    ok(index == 0, "WinHttpQueryHeaders failed: index was incremented.\n");

    /* valid query */
    oldlen = len;
    index = 0;
    len = sizeof(buffer);
    memset(buffer, 0xff, sizeof(buffer));
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_RAW_HEADERS_CRLF | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                              L"Warning", buffer, &len, &index);
    ok(ret == TRUE, "WinHttpQueryHeaders failed: got %d\n", ret);
    ok(len + sizeof(WCHAR) <= oldlen, "WinHttpQueryHeaders resulting length longer than advertized.\n");
    ok((len < sizeof(buffer) - sizeof(WCHAR)) && !buffer[len / sizeof(WCHAR)],
       "WinHttpQueryHeaders did not append NULL terminator\n");
    ok(len == lstrlenW(buffer) * sizeof(WCHAR), "WinHttpQueryHeaders returned incorrect length.\n");
    ok(!memcmp(buffer, test_header_begin, sizeof(test_header_begin)), "invalid beginning of header string.\n");
    ok(!memcmp(buffer + lstrlenW(buffer) - 4, L"\r\n\r\n", sizeof(L"\r\n\r\n")),
        "WinHttpQueryHeaders returned invalid end of header string.\n");
    ok(index == 0, "WinHttpQueryHeaders incremented header index.\n");

    index = 0;
    len = 0;
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_RAW_HEADERS | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                              L"Warning", NULL, &len, &index);
    ok(ret == FALSE, "WinHttpQueryHeaders unexpectedly succeeded.\n");
    ok(GetLastError() == ERROR_INSUFFICIENT_BUFFER,
        "WinHttpQueryHeaders set incorrect error: expected ERROR_INSUFFICIENT_BUFFER, got %lu\n", GetLastError());
    ok(len > 40, "WinHttpQueryHeaders returned invalid length: expected greater than 40, got %lu\n", len);
    ok(index == 0, "WinHttpQueryHeaders failed: index was incremented.\n");

    oldlen = len;
    index = 0;
    len = sizeof(buffer);
    memset(buffer, 0xff, sizeof(buffer));
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_RAW_HEADERS | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                              L"Warning", buffer, &len, &index);
    ok(ret == TRUE, "WinHttpQueryHeaders failed %lu\n", GetLastError());
    ok(len + sizeof(WCHAR) <= oldlen, "resulting length longer than advertized\n");
    ok((len < sizeof(buffer) - sizeof(WCHAR)) && !buffer[len / sizeof(WCHAR)] && !buffer[len / sizeof(WCHAR) - 1],
        "no double NULL terminator\n");
    ok(!memcmp(buffer, test_header_begin, sizeof(test_header_begin)), "invalid beginning of header string.\n");
    ok(index == 0, "header index was incremented\n");

    /* tests for more indices */
    ret = WinHttpAddRequestHeaders(request, test_headers[1], -1L, WINHTTP_ADDREQ_FLAG_ADD);
    ok(ret == TRUE, "WinHttpAddRequestHeaders failed to add duplicate header: %d\n", ret);

    index = 0;
    len = sizeof(buffer);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                              L"Warning", buffer, &len, &index);
    ok(ret == TRUE, "WinHttpQueryHeaders failed: %lu\n", GetLastError());
    ok(index == 1, "WinHttpQueryHeaders failed to increment index.\n");
    ok(memcmp(buffer, test_indices[0], sizeof(test_indices[0])) == 0, "WinHttpQueryHeaders returned incorrect string.\n");

    len = sizeof(buffer);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                              L"Warning", buffer, &len, &index);
    ok(ret == TRUE, "WinHttpQueryHeaders failed: %lu\n", GetLastError());
    ok(index == 2, "WinHttpQueryHeaders failed to increment index.\n");
    ok(memcmp(buffer, test_indices[1], sizeof(test_indices[1])) == 0, "WinHttpQueryHeaders returned incorrect string.\n");

    ret = WinHttpAddRequestHeaders(request, test_headers[2], -1L, WINHTTP_ADDREQ_FLAG_REPLACE);
    ok(ret == TRUE, "WinHttpAddRequestHeaders failed to add duplicate header.\n");

    index = 0;
    len = sizeof(buffer);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                              L"Warning", buffer, &len, &index);
    ok(ret == TRUE, "WinHttpQueryHeaders failed: %lu\n", GetLastError());
    ok(index == 1, "WinHttpQueryHeaders failed to increment index.\n");
    reverse = (memcmp(buffer, test_indices[1], sizeof(test_indices[1])) != 0); /* Win7 returns values in reverse order of adding */
    ok(!memcmp(buffer, test_indices[reverse ? 2 : 1], sizeof(test_indices[reverse ? 2 : 1])),
       "WinHttpQueryHeaders returned incorrect string.\n");

    len = sizeof(buffer);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                              L"Warning", buffer, &len, &index);
    ok(ret == TRUE, "WinHttpQueryHeaders failed: %lu\n", GetLastError());
    ok(index == 2, "WinHttpQueryHeaders failed to increment index.\n");
    ok(!memcmp(buffer, test_indices[reverse ? 1 : 2], sizeof(test_indices[reverse ? 1 : 2])),
       "WinHttpQueryHeaders returned incorrect string.\n");

    /* add if new flag */
    ret = WinHttpAddRequestHeaders(request, test_headers[3], -1L, WINHTTP_ADDREQ_FLAG_ADD_IF_NEW);
    ok(ret == FALSE, "WinHttpAddRequestHeaders incorrectly replaced existing header.\n");

    index = 0;
    len = sizeof(buffer);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                              L"Warning", buffer, &len, &index);
    ok(ret == TRUE, "WinHttpQueryHeaders failed: %lu\n", GetLastError());
    ok(index == 1, "WinHttpQueryHeaders failed to increment index.\n");
    ok(!memcmp(buffer, test_indices[reverse ? 2 : 1], sizeof(test_indices[reverse ? 2 : 1])),
       "WinHttpQueryHeaders returned incorrect string.\n");

    len = sizeof(buffer);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                              L"Warning", buffer, &len, &index);
    ok(ret == TRUE, "WinHttpQueryHeaders failed: %lu\n", GetLastError());
    ok(index == 2, "WinHttpQueryHeaders failed to increment index.\n");
    ok(!memcmp(buffer, test_indices[reverse ? 1 : 2], sizeof(test_indices[reverse ? 1 : 2])),
       "WinHttpQueryHeaders returned incorrect string.\n");

    len = sizeof(buffer);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                              L"Warning", buffer, &len, &index);
    ok(ret == FALSE, "WinHttpQueryHeaders succeeded unexpectedly, found third header.\n");

    /* coalesce flag */
    ret = WinHttpAddRequestHeaders(request, test_headers[3], -1L, WINHTTP_ADDREQ_FLAG_COALESCE);
    ok(ret == TRUE, "WinHttpAddRequestHeaders failed with flag WINHTTP_ADDREQ_FLAG_COALESCE.\n");

    index = 0;
    len = sizeof(buffer);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                              L"Warning", buffer, &len, &index);
    ok(ret == TRUE, "WinHttpQueryHeaders failed: %lu\n", GetLastError());
    ok(index == 1, "WinHttpQueryHeaders failed to increment index.\n");
    ok(!memcmp(buffer, reverse ? L"test3, test4" : L"test2, test4",
                       reverse ? sizeof(L"test3, test4") : sizeof(L"test2, test4")),
       "WinHttpQueryHeaders returned incorrect string.\n");

    len = sizeof(buffer);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                              L"Warning", buffer, &len, &index);
    ok(ret == TRUE, "WinHttpQueryHeaders failed: %lu\n", GetLastError());
    ok(index == 2, "WinHttpQueryHeaders failed to increment index.\n");
    ok(!memcmp(buffer, test_indices[reverse ? 1 : 2], sizeof(test_indices[reverse ? 1 : 2])),
       "WinHttpQueryHeaders returned incorrect string.\n");

    len = sizeof(buffer);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                              L"Warning", buffer, &len, &index);
    ok(ret == FALSE, "WinHttpQueryHeaders succeeded unexpectedly, found third header.\n");

    /* coalesce with comma flag */
    ret = WinHttpAddRequestHeaders(request, test_headers[4], -1L, WINHTTP_ADDREQ_FLAG_COALESCE_WITH_COMMA);
    ok(ret == TRUE, "WinHttpAddRequestHeaders failed with flag WINHTTP_ADDREQ_FLAG_COALESCE_WITH_COMMA.\n");

    index = 0;
    len = sizeof(buffer);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                              L"Warning", buffer, &len, &index);
    ok(ret == TRUE, "WinHttpQueryHeaders failed: %lu\n", GetLastError());
    ok(index == 1, "WinHttpQueryHeaders failed to increment index.\n");

    ok(!memcmp(buffer, reverse ? L"test3, test4, test5" : L"test2, test4, test5",
                       reverse ? sizeof(L"test3, test4, test5") : sizeof(L"test2, test4, test5")),
        "WinHttpQueryHeaders returned incorrect string.\n");

    len = sizeof(buffer);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                              L"Warning", buffer, &len, &index);
    ok(ret == TRUE, "WinHttpQueryHeaders failed: %lu\n", GetLastError());
    ok(index == 2, "WinHttpQueryHeaders failed to increment index.\n");
    ok(!memcmp(buffer, test_indices[reverse ? 1 : 2], sizeof(test_indices[reverse ? 1 : 2])),
       "WinHttpQueryHeaders returned incorrect string.\n");

    len = sizeof(buffer);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                              L"Warning", buffer, &len, &index);
    ok(ret == FALSE, "WinHttpQueryHeaders succeeded unexpectedly, found third header.\n");


    /* coalesce with semicolon flag */
    ret = WinHttpAddRequestHeaders(request, test_headers[5], -1L, WINHTTP_ADDREQ_FLAG_COALESCE_WITH_SEMICOLON);
    ok(ret == TRUE, "WinHttpAddRequestHeaders failed with flag WINHTTP_ADDREQ_FLAG_COALESCE_WITH_SEMICOLON.\n");

    index = 0;
    len = sizeof(buffer);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                              L"Warning", buffer, &len, &index);
    ok(ret == TRUE, "WinHttpQueryHeaders failed: %lu\n", GetLastError());
    ok(index == 1, "WinHttpQueryHeaders failed to increment index.\n");

    ok(!memcmp(buffer, reverse ? L"test3, test4, test5; test6" : L"test2, test4, test5; test6",
                       reverse ? sizeof(L"test3, test4, test5; test6") : sizeof(L"test2, test4, test5; test6")),
       "WinHttpQueryHeaders returned incorrect string.\n");

    len = sizeof(buffer);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                              L"Warning", buffer, &len, &index);
    ok(ret == TRUE, "WinHttpQueryHeaders failed: %lu\n", GetLastError());
    ok(index == 2, "WinHttpQueryHeaders failed to increment index.\n");
    ok(!memcmp(buffer, test_indices[reverse ? 1 : 2], sizeof(test_indices[reverse ? 1 : 2])),
       "WinHttpQueryHeaders returned incorrect string.\n");

    len = sizeof(buffer);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                              L"Warning", buffer, &len, &index);
    ok(ret == FALSE, "WinHttpQueryHeaders succeeded unexpectedly, found third header.\n");

    /* add and replace flags */
    ret = WinHttpAddRequestHeaders(request, test_headers[3], -1L, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
    ok(ret == TRUE, "WinHttpAddRequestHeaders failed with flag WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE.\n");

    index = 0;
    len = sizeof(buffer);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                              L"Warning", buffer, &len, &index);
    ok(ret == TRUE, "WinHttpQueryHeaders failed: %lu\n", GetLastError());
    ok(index == 1, "WinHttpQueryHeaders failed to increment index.\n");
    ok(!memcmp(buffer, test_indices[reverse ? 3 : 2], sizeof(test_indices[reverse ? 3 : 2])),
       "WinHttpQueryHeaders returned incorrect string.\n");

    len = sizeof(buffer);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                              L"Warning", buffer, &len, &index);
    ok(ret == TRUE, "WinHttpQueryHeaders failed: %lu\n", GetLastError());
    ok(index == 2, "WinHttpQueryHeaders failed to increment index.\n");
    ok(!memcmp(buffer, test_indices[reverse ? 1 : 3], sizeof(test_indices[reverse ? 1 : 3])),
       "WinHttpQueryHeaders returned incorrect string.\n");

    len = sizeof(buffer);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                              L"Warning", buffer, &len, &index);
    ok(ret == FALSE, "WinHttpQueryHeaders succeeded unexpectedly, found third header.\n");

    ret = WinHttpAddRequestHeaders(request, test_headers[8], ~0u, WINHTTP_ADDREQ_FLAG_ADD);
    ok(!ret, "WinHttpAddRequestHeaders failed\n");

    ret = WinHttpAddRequestHeaders(request, test_headers[9], ~0u, WINHTTP_ADDREQ_FLAG_ADD);
    ok(ret, "WinHttpAddRequestHeaders failed\n");

    index = 0;
    memset(buffer, 0xff, sizeof(buffer));
    len = sizeof(buffer);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                              L"a", buffer, &len, &index);
    ok(ret, "WinHttpQueryHeaders failed: %lu\n", GetLastError());
    ok(!memcmp(buffer, L"", sizeof(L"")), "unexpected result\n");

    ret = WinHttpAddRequestHeaders(request, test_headers[10], ~0u, WINHTTP_ADDREQ_FLAG_ADD);
    ok(!ret, "WinHttpAddRequestHeaders failed\n");

    ret = WinHttpAddRequestHeaders(request, test_headers[11], ~0u, WINHTTP_ADDREQ_FLAG_ADD);
    ok(!ret, "WinHttpAddRequestHeaders failed\n");

    ret = WinHttpAddRequestHeaders(request, test_headers[12], ~0u, WINHTTP_ADDREQ_FLAG_ADD);
    ok(!ret, "WinHttpAddRequestHeaders failed\n");

    ret = WinHttpAddRequestHeaders(request, test_headers[13], ~0u, WINHTTP_ADDREQ_FLAG_ADD);
    ok(ret, "WinHttpAddRequestHeaders failed\n");

    ret = WinHttpAddRequestHeaders(request, test_headers[16], ~0u, WINHTTP_ADDREQ_FLAG_ADD);
    ok(!ret, "adding %s succeeded.\n", debugstr_w(test_headers[16]));

    index = 0;
    buffer[0] = 0;
    len = sizeof(buffer);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                              L"field", buffer, &len, &index);
    ok(ret, "WinHttpQueryHeaders failed: %lu\n", GetLastError());
    ok(!memcmp(buffer, L"value ", sizeof(L"value ")) || !memcmp(buffer, L"value", sizeof(L"value")),
       "unexpected result\n");

    SetLastError(0xdeadbeef);
    ret = WinHttpAddRequestHeaders(request, L"Range: bytes=0-773\r\n", 0,
                                   WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
    err = GetLastError();
    ok(!ret, "unexpected success\n");
    ok(err == ERROR_INVALID_PARAMETER, "got %lu\n", err);

    ret = WinHttpAddRequestHeaders(request, L"Range: bytes=0-773\r\n", ~0u,
                                   WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
    ok(ret, "failed to add header: %lu\n", GetLastError());

    index = 0;
    len = sizeof(buffer);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                              L"Range", buffer, &len, &index);
    ok(ret, "failed to get range header %lu\n", GetLastError());
    ok(!memcmp(buffer, L"bytes=0-773", sizeof(L"bytes=0-773")), "incorrect string returned\n");
    ok(len == lstrlenW(L"bytes=0-773") * sizeof(WCHAR), "wrong length %lu\n", len);
    ok(index == 1, "wrong index %lu\n", index);
    index = 0;
    len = sizeof(buffer);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                              L"name", buffer, &len, &index);
    ok(!ret, "unexpected success\n");

    SetLastError(0xdeadbeef);
    ret = WinHttpAddRequestHeaders(request, test_headers[14], ~0u, WINHTTP_ADDREQ_FLAG_REPLACE);
    err = GetLastError();
    ok(!ret, "unexpected success\n");
    ok(err == ERROR_WINHTTP_HEADER_NOT_FOUND, "got %lu\n", err);

    ret = WinHttpAddRequestHeaders(request, test_headers[14], ~0u, WINHTTP_ADDREQ_FLAG_ADD);
    ok(ret, "got %lu\n", GetLastError());

    index = 0;
    len = sizeof(buffer);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                              L"name", buffer, &len, &index);
    ok(ret, "got %lu\n", GetLastError());
    ok(index == 1, "wrong index %lu\n", index);
    ok(!memcmp(buffer, L"value", sizeof(L"value")), "incorrect string\n");

    ret = WinHttpAddRequestHeaders(request, test_headers[15], ~0u, WINHTTP_ADDREQ_FLAG_REPLACE);
    ok(ret, "got %lu\n", GetLastError());

    index = 0;
    len = sizeof(buffer);
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                              L"name", buffer, &len, &index);
    err = GetLastError();
    ok(!ret, "unexpected success\n");
    ok(err == ERROR_WINHTTP_HEADER_NOT_FOUND, "got %lu\n", err);

    ret = WinHttpAddRequestHeaders(request, test_headers[14], -1L, 0);
    ok(ret, "got %lu\n", GetLastError());

    index = 0;
    len = sizeof(buffer);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                              L"name", buffer, &len, &index);
    ok(ret, "got %lu\n", GetLastError());
    ok(index == 1, "wrong index %lu\n", index);
    ok(!memcmp(buffer, L"value", sizeof(L"value")), "incorrect string\n");

    ret = WinHttpCloseHandle(request);
    ok(ret == TRUE, "WinHttpCloseHandle failed on closing request, got %d.\n", ret);
 done:
    ret = WinHttpCloseHandle(connection);
    ok(ret == TRUE, "WinHttpCloseHandle failed on closing connection, got %d.\n", ret);
    ret = WinHttpCloseHandle(session);
    ok(ret == TRUE, "WinHttpCloseHandle failed on closing session, got %d.\n", ret);

}

static void CALLBACK cert_error(HINTERNET handle, DWORD_PTR ctx, DWORD status, LPVOID buf, DWORD len)
{
    DWORD flags = *(DWORD *)buf;

    if (!flags)
    {
        trace("WINHTTP_CALLBACK_STATUS_FLAG_SECURITY_CHANNEL_ERROR\n");
        return;
    }
#define X(x) if (flags & x) trace("%s\n", #x);
    X(WINHTTP_CALLBACK_STATUS_FLAG_CERT_REV_FAILED)
    X(WINHTTP_CALLBACK_STATUS_FLAG_INVALID_CERT)
    X(WINHTTP_CALLBACK_STATUS_FLAG_CERT_REVOKED)
    X(WINHTTP_CALLBACK_STATUS_FLAG_INVALID_CA)
    X(WINHTTP_CALLBACK_STATUS_FLAG_CERT_CN_INVALID)
    X(WINHTTP_CALLBACK_STATUS_FLAG_CERT_DATE_INVALID)
    X(WINHTTP_CALLBACK_STATUS_FLAG_CERT_WRONG_USAGE)
#undef X
}

static void test_secure_connection(void)
{
    static const char data_start[] = "<!DOCTYPE html PUBLIC";
    HINTERNET ses, con, req;
    DWORD size, status, policy, bitness, read_size, err, available_size, protocols, flags;
    BOOL ret;
    CERT_CONTEXT *cert;
    WINHTTP_CERTIFICATE_INFO info;
    char buffer[32];

    ses = WinHttpOpen(L"winetest", 0, NULL, NULL, 0);
    ok(ses != NULL, "failed to open session %lu\n", GetLastError());

    policy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    ret = WinHttpSetOption(ses, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy));
    ok(ret, "failed to set redirect policy %lu\n", GetLastError());

    protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
    ret = WinHttpSetOption(ses, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols));
    err = GetLastError();
    ok(ret || err == ERROR_INVALID_PARAMETER /* < win7 */, "failed to set protocols %lu\n", err);

    con = WinHttpConnect(ses, L"test.winehq.org", 443, 0);
    ok(con != NULL, "failed to open a connection %lu\n", GetLastError());

    SetLastError( 0xdeadbeef );
    protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
    ret = WinHttpSetOption(con, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols));
    err = GetLastError();
    ok(!ret, "unexpected success\n");
    ok(err == ERROR_WINHTTP_INCORRECT_HANDLE_TYPE, "got %lu\n", err);

    /* try without setting WINHTTP_FLAG_SECURE */
    req = WinHttpOpenRequest(con, NULL, NULL, NULL, NULL, NULL, 0);
    ok(req != NULL, "failed to open a request %lu\n", GetLastError());

    ret = WinHttpSetOption(req, WINHTTP_OPTION_CLIENT_CERT_CONTEXT, WINHTTP_NO_CLIENT_CERT_CONTEXT, 0);
    err = GetLastError();
    ok(!ret, "unexpected success\n");
    ok(err == ERROR_WINHTTP_INCORRECT_HANDLE_STATE || broken(err == ERROR_INVALID_PARAMETER) /* winxp */,
       "setting client cert context returned %lu\n", err);

    ret = WinHttpSendRequest(req, NULL, 0, NULL, 0, 0, 0);
    err = GetLastError();
    if (!ret && (err == ERROR_WINHTTP_CANNOT_CONNECT || err == ERROR_WINHTTP_TIMEOUT))
    {
        skip("Connection failed, skipping.\n");
        goto cleanup;
    }
    ok(ret, "failed to send request %lu\n", GetLastError());

    ret = WinHttpReceiveResponse(req, NULL);
    ok(ret, "failed to receive response %lu\n", GetLastError());

    status = 0xdeadbeef;
    size = sizeof(status);
    ret = WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &size, NULL);
    ok(ret, "header query failed %lu\n", GetLastError());
    ok(status == HTTP_STATUS_BAD_REQUEST, "got %lu\n", status);

    WinHttpCloseHandle(req);

    req = WinHttpOpenRequest(con, NULL, NULL, NULL, NULL, NULL, WINHTTP_FLAG_SECURE);
    ok(req != NULL, "failed to open a request %lu\n", GetLastError());

    flags = 0xdeadbeef;
    size = sizeof(flags);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_SECURITY_FLAGS, &flags, &size);
    ok(ret, "failed to query security flags %lu\n", GetLastError());
    ok(!flags, "got %#lx\n", flags);

    flags = SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
    ret = WinHttpSetOption(req, WINHTTP_OPTION_SECURITY_FLAGS, &flags, sizeof(flags));
    ok(ret, "failed to set security flags %lu\n", GetLastError());

    flags = SECURITY_FLAG_SECURE;
    ret = WinHttpSetOption(req, WINHTTP_OPTION_SECURITY_FLAGS, &flags, sizeof(flags));
    ok(!ret, "success\n");

    flags = SECURITY_FLAG_STRENGTH_STRONG;
    ret = WinHttpSetOption(req, WINHTTP_OPTION_SECURITY_FLAGS, &flags, sizeof(flags));
    ok(!ret, "success\n");

    flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
            SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
    ret = WinHttpSetOption(req, WINHTTP_OPTION_SECURITY_FLAGS, &flags, sizeof(flags));
    ok(ret, "failed to set security flags %lu\n", GetLastError());

    flags = 0;
    ret = WinHttpSetOption(req, WINHTTP_OPTION_SECURITY_FLAGS, &flags, sizeof(flags));
    ok(ret, "failed to set security flags %lu\n", GetLastError());

    ret = WinHttpSetOption(req, WINHTTP_OPTION_CLIENT_CERT_CONTEXT, WINHTTP_NO_CLIENT_CERT_CONTEXT, 0);
    err = GetLastError();
    ok(ret || broken(!ret && err == ERROR_INVALID_PARAMETER) /* winxp */, "failed to set client cert context %lu\n", err);

    WinHttpSetStatusCallback(req, cert_error, WINHTTP_CALLBACK_STATUS_SECURE_FAILURE, 0);

    ret = WinHttpSendRequest(req, NULL, 0, NULL, 0, 0, 0);
    err = GetLastError();
    if (!ret && (err == ERROR_WINHTTP_SECURE_FAILURE || err == ERROR_WINHTTP_CANNOT_CONNECT ||
                 err == ERROR_WINHTTP_TIMEOUT || err == SEC_E_ILLEGAL_MESSAGE))
    {
        skip("secure connection failed, skipping remaining secure tests\n");
        goto cleanup;
    }
    ok(ret, "failed to send request %lu\n", GetLastError());

    size = sizeof(cert);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_SERVER_CERT_CONTEXT, &cert, &size );
    ok(ret, "failed to retrieve certificate context %lu\n", GetLastError());
    if (ret) CertFreeCertificateContext(cert);

    size = sizeof(bitness);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_SECURITY_KEY_BITNESS, &bitness, &size );
    ok(ret, "failed to retrieve key bitness %lu\n", GetLastError());

    size = sizeof(info);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_SECURITY_CERTIFICATE_STRUCT, &info, &size );
    ok(ret, "failed to retrieve certificate info %lu\n", GetLastError());

    if (ret)
    {
        trace("lpszSubjectInfo %s\n", wine_dbgstr_w(info.lpszSubjectInfo));
        trace("lpszIssuerInfo %s\n", wine_dbgstr_w(info.lpszIssuerInfo));
        trace("lpszProtocolName %s\n", wine_dbgstr_w(info.lpszProtocolName));
        trace("lpszSignatureAlgName %s\n", wine_dbgstr_w(info.lpszSignatureAlgName));
        trace("lpszEncryptionAlgName %s\n", wine_dbgstr_w(info.lpszEncryptionAlgName));
        trace("dwKeySize %lu\n", info.dwKeySize);
        LocalFree( info.lpszSubjectInfo );
        LocalFree( info.lpszIssuerInfo );
    }

    ret = WinHttpReceiveResponse(req, NULL);
    if (!ret && GetLastError() == ERROR_WINHTTP_CONNECTION_ERROR)
    {
        skip("connection error, skipping remaining secure tests\n");
        goto cleanup;
    }
    ok(ret, "failed to receive response %lu\n", GetLastError());

    available_size = 0;
    ret = WinHttpQueryDataAvailable(req, &available_size);
    ok(ret, "failed to query available data %lu\n", GetLastError());
    ok(available_size > 2014, "available_size = %lu\n", available_size);

    status = 0xdeadbeef;
    size = sizeof(status);
    ret = WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &size, NULL);
    ok(ret, "failed unexpectedly %lu\n", GetLastError());
    ok(status == HTTP_STATUS_OK, "request failed unexpectedly %lu\n", status);

    size = 0;
    ret = WinHttpQueryHeaders(req, WINHTTP_QUERY_RAW_HEADERS_CRLF, NULL, NULL, &size, NULL);
    ok(!ret, "succeeded unexpectedly\n");

    read_size = 0;
    for (;;)
    {
        size = 0;
        ret = WinHttpReadData(req, buffer, sizeof(buffer), &size);
        ok(ret == TRUE, "WinHttpReadData failed: %lu\n", GetLastError());
        if (!size) break;
        read_size += size;

        if (read_size <= 32)
            ok(!memcmp(buffer, data_start, sizeof(data_start)-1), "not expected: %.32s\n", buffer);
    }
    ok(read_size >= available_size, "read_size = %lu, available_size = %lu\n", read_size, available_size);

    size = sizeof(cert);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_SERVER_CERT_CONTEXT, &cert, &size);
    ok(ret, "failed to retrieve certificate context %lu\n", GetLastError());
    if (ret) CertFreeCertificateContext(cert);

cleanup:
    WinHttpCloseHandle(req);
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
}

static void test_request_parameter_defaults(void)
{
    HINTERNET ses, con, req;
    DWORD size, status, error;
    WCHAR *version;
    BOOL ret;

    ses = WinHttpOpen(L"winetest", 0, NULL, NULL, 0);
    ok(ses != NULL, "failed to open session %lu\n", GetLastError());

    con = WinHttpConnect(ses, L"test.winehq.org", 0, 0);
    ok(con != NULL, "failed to open a connection %lu\n", GetLastError());

    req = WinHttpOpenRequest(con, NULL, NULL, NULL, NULL, NULL, 0);
    ok(req != NULL, "failed to open a request %lu\n", GetLastError());

    ret = WinHttpSendRequest(req, NULL, 0, NULL, 0, 0, 0);
    error = GetLastError();
    if (!ret && (error == ERROR_WINHTTP_CANNOT_CONNECT || error == ERROR_WINHTTP_TIMEOUT))
    {
        skip("connection failed, skipping\n");
        goto done;
    }
    ok(ret, "failed to send request %lu\n", GetLastError());

    ret = WinHttpReceiveResponse(req, NULL);
    ok(ret, "failed to receive response %lu\n", GetLastError());

    status = 0xdeadbeef;
    size = sizeof(status);
    ret = WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &size, NULL);
    ok(ret, "failed unexpectedly %lu\n", GetLastError());
    ok(status == HTTP_STATUS_OK, "request failed unexpectedly %lu\n", status);

    WinHttpCloseHandle(req);

    req = WinHttpOpenRequest(con, L"", L"", L"", NULL, NULL, 0);
    ok(req != NULL, "failed to open a request %lu\n", GetLastError());

    ret = WinHttpSendRequest(req, NULL, 0, NULL, 0, 0, 0);
    error = GetLastError();
    if (!ret && (error == ERROR_WINHTTP_CANNOT_CONNECT || error == ERROR_WINHTTP_TIMEOUT))
    {
        skip("connection failed, skipping\n");
        goto done;
    }
    ok(ret, "failed to send request %lu\n", GetLastError());

    ret = WinHttpReceiveResponse(req, NULL);
    ok(ret, "failed to receive response %lu\n", GetLastError());

    size = 0;
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryHeaders(req, WINHTTP_QUERY_VERSION, NULL, NULL, &size, NULL);
    error = GetLastError();
    ok(!ret, "succeeded unexpectedly\n");
    ok(error == ERROR_INSUFFICIENT_BUFFER, "expected ERROR_INSUFFICIENT_BUFFER, got %lu\n", error);

    version = HeapAlloc(GetProcessHeap(), 0, size);
    ret = WinHttpQueryHeaders(req, WINHTTP_QUERY_VERSION, NULL, version, &size, NULL);
    ok(ret, "failed unexpectedly %lu\n", GetLastError());
    ok(lstrlenW(version) == size / sizeof(WCHAR), "unexpected size %lu\n", size);
    HeapFree(GetProcessHeap(), 0, version);

    status = 0xdeadbeef;
    size = sizeof(status);
    ret = WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &size, NULL);
    ok(ret, "failed unexpectedly %lu\n", GetLastError());
    ok(status == HTTP_STATUS_OK, "request failed unexpectedly %lu\n", status);

done:
    WinHttpCloseHandle(req);
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
}

static const WCHAR Connections[] = {
    'S','o','f','t','w','a','r','e','\\',
    'M','i','c','r','o','s','o','f','t','\\',
    'W','i','n','d','o','w','s','\\',
    'C','u','r','r','e','n','t','V','e','r','s','i','o','n','\\',
    'I','n','t','e','r','n','e','t',' ','S','e','t','t','i','n','g','s','\\',
    'C','o','n','n','e','c','t','i','o','n','s',0 };
static const WCHAR WinHttpSettings[] = {
    'W','i','n','H','t','t','p','S','e','t','t','i','n','g','s',0 };

static DWORD get_default_proxy_reg_value( BYTE *buf, DWORD len, DWORD *type )
{
    LONG l;
    HKEY key;
    DWORD ret = 0;

    l = RegOpenKeyExW( HKEY_LOCAL_MACHINE, Connections, 0, KEY_READ, &key );
    if (!l)
    {
        DWORD size = 0;

        l = RegQueryValueExW( key, WinHttpSettings, NULL, type, NULL, &size );
        if (!l)
        {
            if (size <= len)
                l = RegQueryValueExW( key, WinHttpSettings, NULL, type, buf,
                    &size );
            if (!l)
                ret = size;
        }
        RegCloseKey( key );
    }
    return ret;
}

static void set_proxy( REGSAM access, BYTE *buf, DWORD len, DWORD type )
{
    HKEY hkey;
    if (!RegCreateKeyExW( HKEY_LOCAL_MACHINE, Connections, 0, NULL, 0, access, NULL, &hkey, NULL ))
    {
        if (len) RegSetValueExW( hkey, WinHttpSettings, 0, type, buf, len );
        else RegDeleteValueW( hkey, WinHttpSettings );
        RegCloseKey( hkey );
    }
}

static void set_default_proxy_reg_value( BYTE *buf, DWORD len, DWORD type )
{
    BOOL wow64;
    IsWow64Process( GetCurrentProcess(), &wow64 );
    if (sizeof(void *) > sizeof(int) || wow64)
    {
        set_proxy( KEY_WRITE|KEY_WOW64_64KEY, buf, len, type );
        set_proxy( KEY_WRITE|KEY_WOW64_32KEY, buf, len, type );
    }
    else
        set_proxy( KEY_WRITE, buf, len, type );
}

static void test_set_default_proxy_config(void)
{
    static WCHAR wideString[] = { 0x226f, 0x575b, 0 };
    static WCHAR normalString[] = { 'f','o','o',0 };
    DWORD type, len;
    BYTE *saved_proxy_settings = NULL;
    WINHTTP_PROXY_INFO info;
    BOOL ret;

    /* FIXME: it would be simpler to read the current settings using
     * WinHttpGetDefaultProxyConfiguration and save them using
     * WinHttpSetDefaultProxyConfiguration, but they appear to have a bug.
     *
     * If a proxy is configured in the registry, e.g. via 'proxcfg -p "foo"',
     * the access type reported by WinHttpGetDefaultProxyConfiguration is 1,
     * WINHTTP_ACCESS_TYPE_NO_PROXY, whereas it should be
     * WINHTTP_ACCESS_TYPE_NAMED_PROXY.
     * If WinHttpSetDefaultProxyConfiguration is called with dwAccessType = 1,
     * the lpszProxy and lpszProxyBypass values are ignored.
     * Thus, if a proxy is set with proxycfg, then calling
     * WinHttpGetDefaultProxyConfiguration followed by
     * WinHttpSetDefaultProxyConfiguration results in the proxy settings
     * getting deleted from the registry.
     *
     * Instead I read the current registry value and restore it directly.
     */
    len = get_default_proxy_reg_value( NULL, 0, &type );
    if (len)
    {
        saved_proxy_settings = HeapAlloc( GetProcessHeap(), 0, len );
        len = get_default_proxy_reg_value( saved_proxy_settings, len, &type );
    }

    if (0)
    {
        /* Crashes on Vista and higher */
        SetLastError(0xdeadbeef);
        ret = WinHttpSetDefaultProxyConfiguration(NULL);
        ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER,
            "expected ERROR_INVALID_PARAMETER, got %lu\n", GetLastError());
    }

    /* test with invalid access type */
    info.dwAccessType = 0xdeadbeef;
    info.lpszProxy = info.lpszProxyBypass = NULL;
    SetLastError(0xdeadbeef);
    ret = WinHttpSetDefaultProxyConfiguration(&info);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER,
        "expected ERROR_INVALID_PARAMETER, got %lu\n", GetLastError());

    /* at a minimum, the proxy server must be set */
    info.dwAccessType = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
    info.lpszProxy = info.lpszProxyBypass = NULL;
    SetLastError(0xdeadbeef);
    ret = WinHttpSetDefaultProxyConfiguration(&info);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER,
        "expected ERROR_INVALID_PARAMETER, got %lu\n", GetLastError());
    info.lpszProxyBypass = normalString;
    SetLastError(0xdeadbeef);
    ret = WinHttpSetDefaultProxyConfiguration(&info);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER,
        "expected ERROR_INVALID_PARAMETER, got %lu\n", GetLastError());

    /* the proxy server can't have wide characters */
    info.lpszProxy = wideString;
    SetLastError(0xdeadbeef);
    ret = WinHttpSetDefaultProxyConfiguration(&info);
    if (!ret && GetLastError() == ERROR_ACCESS_DENIED)
        skip("couldn't set default proxy configuration: access denied\n");
    else
        ok((!ret && GetLastError() == ERROR_INVALID_PARAMETER) ||
           broken(ret), /* Earlier winhttp versions on W2K/XP */
           "expected ERROR_INVALID_PARAMETER, got %lu\n", GetLastError());

    info.lpszProxy = normalString;
    SetLastError(0xdeadbeef);
    ret = WinHttpSetDefaultProxyConfiguration(&info);
    if (!ret && GetLastError() == ERROR_ACCESS_DENIED)
        skip("couldn't set default proxy configuration: access denied\n");
    else
    {
        ok(ret, "WinHttpSetDefaultProxyConfiguration failed: %lu\n", GetLastError());
        ok(GetLastError() == ERROR_SUCCESS || broken(GetLastError() == 0xdeadbeef) /* < win7 */,
           "got %lu\n", GetLastError());
    }
    set_default_proxy_reg_value( saved_proxy_settings, len, type );
}

static void test_timeouts(void)
{
    BOOL ret;
    DWORD value, size;
    HINTERNET ses, req, con;

    ses = WinHttpOpen(L"winetest", 0, NULL, NULL, 0);
    ok(ses != NULL, "failed to open session %lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = WinHttpSetTimeouts(ses, -2, 0, 0, 0);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER,
       "expected ERROR_INVALID_PARAMETER, got %lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = WinHttpSetTimeouts(ses, 0, -2, 0, 0);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER,
       "expected ERROR_INVALID_PARAMETER, got %lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = WinHttpSetTimeouts(ses, 0, 0, -2, 0);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER,
       "expected ERROR_INVALID_PARAMETER, got %lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = WinHttpSetTimeouts(ses, 0, 0, 0, -2);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER,
       "expected ERROR_INVALID_PARAMETER, got %lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = WinHttpSetTimeouts(ses, -1, -1, -1, -1);
    ok(ret, "%lu\n", GetLastError());
    ok(GetLastError() == ERROR_SUCCESS || broken(GetLastError() == 0xdeadbeef) /* < win7 */,
       "expected ERROR_SUCCESS, got %lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = WinHttpSetTimeouts(ses, 0, 0, 0, 0);
    ok(ret, "%lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = WinHttpSetTimeouts(ses, 0x0123, 0x4567, 0x89ab, 0xcdef);
    ok(ret, "%lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(ses, WINHTTP_OPTION_RESOLVE_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0x0123, "Expected 0x0123, got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(ses, WINHTTP_OPTION_CONNECT_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0x4567, "Expected 0x4567, got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(ses, WINHTTP_OPTION_SEND_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0x89ab, "Expected 0x89ab, got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(ses, WINHTTP_OPTION_RECEIVE_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0xcdef, "Expected 0xcdef, got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 0;
    ret = WinHttpSetOption(ses, WINHTTP_OPTION_RESOLVE_TIMEOUT, &value, sizeof(value));
    ok(ret, "%lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(ses, WINHTTP_OPTION_RESOLVE_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0, "Expected 0, got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 0;
    ret = WinHttpSetOption(ses, WINHTTP_OPTION_CONNECT_TIMEOUT, &value, sizeof(value));
    ok(ret, "%lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(ses, WINHTTP_OPTION_CONNECT_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0, "Expected 0, got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 0;
    ret = WinHttpSetOption(ses, WINHTTP_OPTION_SEND_TIMEOUT, &value, sizeof(value));
    ok(ret, "%lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(ses, WINHTTP_OPTION_SEND_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0, "Expected 0, got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 0;
    ret = WinHttpSetOption(ses, WINHTTP_OPTION_RECEIVE_TIMEOUT, &value, sizeof(value));
    ok(ret, "%lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(ses, WINHTTP_OPTION_RECEIVE_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0, "Expected 0, got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 0xbeefdead;
    ret = WinHttpSetOption(ses, WINHTTP_OPTION_RESOLVE_TIMEOUT, &value, sizeof(value));
    ok(ret, "%lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(ses, WINHTTP_OPTION_RESOLVE_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0xbeefdead, "Expected 0xbeefdead, got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 0xbeefdead;
    ret = WinHttpSetOption(ses, WINHTTP_OPTION_CONNECT_TIMEOUT, &value, sizeof(value));
    ok(ret, "%lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(ses, WINHTTP_OPTION_CONNECT_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0xbeefdead, "Expected 0xbeefdead, got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 0xbeefdead;
    ret = WinHttpSetOption(ses, WINHTTP_OPTION_SEND_TIMEOUT, &value, sizeof(value));
    ok(ret, "%lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(ses, WINHTTP_OPTION_SEND_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0xbeefdead, "Expected 0xbeefdead, got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 0xbeefdead;
    ret = WinHttpSetOption(ses, WINHTTP_OPTION_RECEIVE_TIMEOUT, &value, sizeof(value));
    ok(ret, "%lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(ses, WINHTTP_OPTION_RECEIVE_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0xbeefdead, "Expected 0xbeefdead, got %lu\n", value);

    con = WinHttpConnect(ses, L"test.winehq.org", 0, 0);
    ok(con != NULL, "failed to open a connection %lu\n", GetLastError());

    /* Timeout values should match the last one set for session */
    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(con, WINHTTP_OPTION_RESOLVE_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0xbeefdead, "Expected 0xbeefdead, got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(con, WINHTTP_OPTION_CONNECT_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0xbeefdead, "Expected 0xbeefdead, got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(con, WINHTTP_OPTION_SEND_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0xbeefdead, "Expected 0xbeefdead, got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(con, WINHTTP_OPTION_RECEIVE_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0xbeefdead, "Expected 0xbeefdead, got %lu\n", value);

    SetLastError(0xdeadbeef);
    ret = WinHttpSetTimeouts(con, -2, 0, 0, 0);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER,
       "expected ERROR_INVALID_PARAMETER, got %lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = WinHttpSetTimeouts(con, 0, -2, 0, 0);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER,
       "expected ERROR_INVALID_PARAMETER, got %lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = WinHttpSetTimeouts(con, 0, 0, -2, 0);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER,
       "expected ERROR_INVALID_PARAMETER, got %lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = WinHttpSetTimeouts(con, 0, 0, 0, -2);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER,
       "expected ERROR_INVALID_PARAMETER, got %lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = WinHttpSetTimeouts(con, -1, -1, -1, -1);
    ok(!ret && GetLastError() == ERROR_WINHTTP_INCORRECT_HANDLE_TYPE,
       "expected ERROR_WINHTTP_INVALID_TYPE, got %lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = WinHttpSetTimeouts(con, 0, 0, 0, 0);
    ok(!ret && GetLastError() == ERROR_WINHTTP_INCORRECT_HANDLE_TYPE,
       "expected ERROR_WINHTTP_INVALID_TYPE, got %lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    value = 0;
    ret = WinHttpSetOption(con, WINHTTP_OPTION_RESOLVE_TIMEOUT, &value, sizeof(value));
    ok(!ret && GetLastError() == ERROR_WINHTTP_INCORRECT_HANDLE_TYPE,
       "expected ERROR_WINHTTP_INVALID_TYPE, got %lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    value = 0;
    ret = WinHttpSetOption(con, WINHTTP_OPTION_CONNECT_TIMEOUT, &value, sizeof(value));
    ok(!ret && GetLastError() == ERROR_WINHTTP_INCORRECT_HANDLE_TYPE,
       "expected ERROR_WINHTTP_INVALID_TYPE, got %lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    value = 0;
    ret = WinHttpSetOption(con, WINHTTP_OPTION_SEND_TIMEOUT, &value, sizeof(value));
    ok(!ret && GetLastError() == ERROR_WINHTTP_INCORRECT_HANDLE_TYPE,
       "expected ERROR_WINHTTP_INVALID_TYPE, got %lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    value = 0;
    ret = WinHttpSetOption(con, WINHTTP_OPTION_RECEIVE_TIMEOUT, &value, sizeof(value));
    ok(!ret && GetLastError() == ERROR_WINHTTP_INCORRECT_HANDLE_TYPE,
       "expected ERROR_WINHTTP_INVALID_TYPE, got %lu\n", GetLastError());

    /* Changing timeout values for session should affect the values for connection */
    SetLastError(0xdeadbeef);
    value = 0xdead;
    ret = WinHttpSetOption(ses, WINHTTP_OPTION_RESOLVE_TIMEOUT, &value, sizeof(value));
    ok(ret, "%lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(con, WINHTTP_OPTION_RESOLVE_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0xdead, "Expected 0xdead, got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 0xdead;
    ret = WinHttpSetOption(ses, WINHTTP_OPTION_CONNECT_TIMEOUT, &value, sizeof(value));
    ok(ret, "%lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(con, WINHTTP_OPTION_CONNECT_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0xdead, "Expected 0xdead, got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 0xdead;
    ret = WinHttpSetOption(ses, WINHTTP_OPTION_SEND_TIMEOUT, &value, sizeof(value));
    ok(ret, "%lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(con, WINHTTP_OPTION_SEND_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0xdead, "Expected 0xdead, got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 0xdead;
    ret = WinHttpSetOption(ses, WINHTTP_OPTION_RECEIVE_TIMEOUT, &value, sizeof(value));
    ok(ret, "%lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(con, WINHTTP_OPTION_RECEIVE_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0xdead, "Expected 0xdead, got %lu\n", value);

    req = WinHttpOpenRequest(con, NULL, NULL, NULL, NULL, NULL, 0);
    ok(req != NULL, "failed to open a request %lu\n", GetLastError());

    /* Timeout values should match the last one set for session */
    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_RESOLVE_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0xdead, "Expected 0xdead, got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_CONNECT_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0xdead, "Expected 0xdead, got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_SEND_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0xdead, "Expected 0xdead, got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_RECEIVE_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0xdead, "Expected 0xdead, got %lu\n", value);

    SetLastError(0xdeadbeef);
    ret = WinHttpSetTimeouts(req, -2, 0, 0, 0);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER,
       "expected ERROR_INVALID_PARAMETER, got %lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = WinHttpSetTimeouts(req, 0, -2, 0, 0);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER,
       "expected ERROR_INVALID_PARAMETER, got %lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = WinHttpSetTimeouts(req, 0, 0, -2, 0);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER,
       "expected ERROR_INVALID_PARAMETER, got %lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = WinHttpSetTimeouts(req, 0, 0, 0, -2);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER,
       "expected ERROR_INVALID_PARAMETER, got %lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = WinHttpSetTimeouts(req, -1, -1, -1, -1);
    ok(ret, "%lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = WinHttpSetTimeouts(req, 0, 0, 0, 0);
    ok(ret, "%lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = WinHttpSetTimeouts(req, 0xcdef, 0x89ab, 0x4567, 0x0123);
    ok(ret, "%lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_RESOLVE_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0xcdef, "Expected 0xcdef, got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_CONNECT_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0x89ab, "Expected 0x89ab, got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_SEND_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0x4567, "Expected 0x4567, got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_RECEIVE_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0x0123, "Expected 0x0123, got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 0;
    ret = WinHttpSetOption(req, WINHTTP_OPTION_RESOLVE_TIMEOUT, &value, sizeof(value));
    ok(ret, "%lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_RESOLVE_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0, "Expected 0, got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 0;
    ret = WinHttpSetOption(req, WINHTTP_OPTION_CONNECT_TIMEOUT, &value, sizeof(value));
    ok(ret, "%lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_CONNECT_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0, "Expected 0, got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 0;
    ret = WinHttpSetOption(req, WINHTTP_OPTION_SEND_TIMEOUT, &value, sizeof(value));
    ok(ret, "%lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_SEND_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0, "Expected 0, got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 0;
    ret = WinHttpSetOption(req, WINHTTP_OPTION_RECEIVE_TIMEOUT, &value, sizeof(value));
    ok(ret, "%lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_RECEIVE_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0, "Expected 0, got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 0xbeefdead;
    ret = WinHttpSetOption(req, WINHTTP_OPTION_RESOLVE_TIMEOUT, &value, sizeof(value));
    ok(ret, "%lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_RESOLVE_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0xbeefdead, "Expected 0xbeefdead, got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 0xbeefdead;
    ret = WinHttpSetOption(req, WINHTTP_OPTION_CONNECT_TIMEOUT, &value, sizeof(value));
    ok(ret, "%lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_CONNECT_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0xbeefdead, "Expected 0xbeefdead, got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 0xbeefdead;
    ret = WinHttpSetOption(req, WINHTTP_OPTION_SEND_TIMEOUT, &value, sizeof(value));
    ok(ret, "%lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_SEND_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0xbeefdead, "Expected 0xbeefdead, got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 0xbeefdead;
    ret = WinHttpSetOption(req, WINHTTP_OPTION_RECEIVE_TIMEOUT, &value, sizeof(value));
    ok(ret, "%lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_RECEIVE_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0xbeefdead, "Expected 0xbeefdead, got %lu\n", value);

    /* Changing timeout values for session should not affect the values for a request,
     * neither should the other way around.
     */
    SetLastError(0xdeadbeef);
    value = 0xbeefdead;
    ret = WinHttpSetOption(req, WINHTTP_OPTION_RESOLVE_TIMEOUT, &value, sizeof(value));
    ok(ret, "%lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(ses, WINHTTP_OPTION_RESOLVE_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0xdead, "Expected 0xdead, got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 0xbeefdead;
    ret = WinHttpSetOption(req, WINHTTP_OPTION_CONNECT_TIMEOUT, &value, sizeof(value));
    ok(ret, "%lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(ses, WINHTTP_OPTION_CONNECT_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0xdead, "Expected 0xdead, got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 0xbeefdead;
    ret = WinHttpSetOption(req, WINHTTP_OPTION_SEND_TIMEOUT, &value, sizeof(value));
    ok(ret, "%lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(ses, WINHTTP_OPTION_SEND_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0xdead, "Expected 0xdead, got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 0xbeefdead;
    ret = WinHttpSetOption(req, WINHTTP_OPTION_RECEIVE_TIMEOUT, &value, sizeof(value));
    ok(ret, "%lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(ses, WINHTTP_OPTION_RECEIVE_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0xdead, "Expected 0xdead, got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 0xbeef;
    ret = WinHttpSetOption(ses, WINHTTP_OPTION_RESOLVE_TIMEOUT, &value, sizeof(value));
    ok(ret, "%lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_RESOLVE_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0xbeefdead, "Expected 0xbeefdead, got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 0xbeef;
    ret = WinHttpSetOption(ses, WINHTTP_OPTION_CONNECT_TIMEOUT, &value, sizeof(value));
    ok(ret, "%lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_CONNECT_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0xbeefdead, "Expected 0xbeefdead, got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 0xbeef;
    ret = WinHttpSetOption(ses, WINHTTP_OPTION_SEND_TIMEOUT, &value, sizeof(value));
    ok(ret, "%lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_SEND_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0xbeefdead, "Expected 0xbeefdead, got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 0xbeef;
    ret = WinHttpSetOption(ses, WINHTTP_OPTION_RECEIVE_TIMEOUT, &value, sizeof(value));
    ok(ret, "%lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(DWORD);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_RECEIVE_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 0xbeefdead, "Expected 0xbeefdead, got %lu\n", value);

    /* response timeout */
    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(value);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_RECEIVE_RESPONSE_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == ~0u, "got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 30000;
    ret = WinHttpSetOption(req, WINHTTP_OPTION_RECEIVE_RESPONSE_TIMEOUT, &value, sizeof(value));
    ok(ret, "%lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(value);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_RECEIVE_RESPONSE_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    todo_wine ok(value == 0xbeefdead, "got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(value);
    ret = WinHttpQueryOption(con, WINHTTP_OPTION_RECEIVE_RESPONSE_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == ~0u, "got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 30000;
    ret = WinHttpSetOption(con, WINHTTP_OPTION_RECEIVE_RESPONSE_TIMEOUT, &value, sizeof(value));
    ok(!ret, "expected failure\n");
    ok(GetLastError() == ERROR_WINHTTP_INCORRECT_HANDLE_TYPE, "got %lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(value);
    ret = WinHttpQueryOption(ses, WINHTTP_OPTION_RECEIVE_RESPONSE_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == ~0u, "got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 48878;
    ret = WinHttpSetOption(ses, WINHTTP_OPTION_RECEIVE_RESPONSE_TIMEOUT, &value, sizeof(value));
    ok(ret, "%lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(value);
    ret = WinHttpQueryOption(ses, WINHTTP_OPTION_RECEIVE_RESPONSE_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    todo_wine ok(value == 48879, "got %lu\n", value);

    SetLastError(0xdeadbeef);
    value = 48880;
    ret = WinHttpSetOption(ses, WINHTTP_OPTION_RECEIVE_RESPONSE_TIMEOUT, &value, sizeof(value));
    ok(ret, "%lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    value = 0xdeadbeef;
    size  = sizeof(value);
    ret = WinHttpQueryOption(ses, WINHTTP_OPTION_RECEIVE_RESPONSE_TIMEOUT, &value, &size);
    ok(ret, "%lu\n", GetLastError());
    ok(value == 48880, "got %lu\n", value);

    WinHttpCloseHandle(req);
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
}

static void test_resolve_timeout(void)
{
    HINTERNET ses, con, req;
    DWORD timeout;
    BOOL ret;

    if (! proxy_active())
    {
        ses = WinHttpOpen(L"winetest", 0, NULL, NULL, 0);
        ok(ses != NULL, "failed to open session %lu\n", GetLastError());

        timeout = 10000;
        ret = WinHttpSetOption(ses, WINHTTP_OPTION_RESOLVE_TIMEOUT, &timeout, sizeof(timeout));
        ok(ret, "failed to set resolve timeout %lu\n", GetLastError());

        con = WinHttpConnect(ses, L"nxdomain.winehq.org", 0, 0);
        ok(con != NULL, "failed to open a connection %lu\n", GetLastError());

        req = WinHttpOpenRequest(con, NULL, NULL, NULL, NULL, NULL, 0);
        ok(req != NULL, "failed to open a request %lu\n", GetLastError());

        SetLastError(0xdeadbeef);
        ret = WinHttpSendRequest(req, NULL, 0, NULL, 0, 0, 0);
        if (ret)
        {
            skip("nxdomain returned success. Broken ISP redirects?\n");
            goto done;
        }
        ok(GetLastError() == ERROR_WINHTTP_NAME_NOT_RESOLVED,
           "expected ERROR_WINHTTP_NAME_NOT_RESOLVED got %lu\n", GetLastError());

        ret = WinHttpReceiveResponse( req, NULL );
        ok( !ret && (GetLastError() == ERROR_WINHTTP_INCORRECT_HANDLE_STATE ||
                     GetLastError() == ERROR_WINHTTP_OPERATION_CANCELLED /* < win7 */),
            "got %lu\n", GetLastError() );

        WinHttpCloseHandle(req);
        WinHttpCloseHandle(con);
        WinHttpCloseHandle(ses);
    }
    else
       skip("Skipping host resolution tests, host resolution performed by proxy\n");

    ses = WinHttpOpen(L"winetest", 0, NULL, NULL, 0);
    ok(ses != NULL, "failed to open session %lu\n", GetLastError());

    timeout = 10000;
    ret = WinHttpSetOption(ses, WINHTTP_OPTION_RESOLVE_TIMEOUT, &timeout, sizeof(timeout));
    ok(ret, "failed to set resolve timeout %lu\n", GetLastError());

    con = WinHttpConnect(ses, L"test.winehq.org", 0, 0);
    ok(con != NULL, "failed to open a connection %lu\n", GetLastError());

    req = WinHttpOpenRequest(con, NULL, NULL, NULL, NULL, NULL, 0);
    ok(req != NULL, "failed to open a request %lu\n", GetLastError());

    ret = WinHttpSendRequest(req, NULL, 0, NULL, 0, 0, 0);
    if (!ret && GetLastError() == ERROR_WINHTTP_CANNOT_CONNECT)
    {
        skip("connection failed, skipping\n");
        goto done;
    }
    ok(ret, "failed to send request\n");

 done:
    WinHttpCloseHandle(req);
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
}

static const char page1[] =
"<HTML>\r\n"
"<HEAD><TITLE>winhttp test page</TITLE></HEAD>\r\n"
"<BODY>The quick brown fox jumped over the lazy dog<P></BODY>\r\n"
"</HTML>\r\n\r\n";

static const char okmsg[] =
"HTTP/1.1 200 OK\r\n"
"Server: winetest\r\n"
"\r\n";

static const char okmsg_length0[] =
"HTTP/1.1  200  OK\r\n"
"Server: winetest\r\n"
"Content-length: 0\r\n"
"\r\n";

static const char notokmsg[] =
"HTTP/1.1 400 Bad Request\r\n"
"\r\n";

static const char cookiemsg[] =
"HTTP/1.1 200 OK\r\n"
"Set-Cookie: name = value \r\n"
"Set-Cookie: NAME = value \r\n"
"\r\n";

static const char cookiemsg2[] =
"HTTP/1.1 200 OK\r\n"
"Set-Cookie: name2=value; Domain = localhost; Path=/cookie5;Expires=Wed, 13 Jan 2021 22:23:01 GMT; HttpOnly; \r\n"
"\r\n";

static const char nocontentmsg[] =
"HTTP/1.1 204 No Content\r\n"
"Server: winetest\r\n"
"\r\n";

static const char notmodified[] =
"HTTP/1.1 304 Not Modified\r\n"
"\r\n";

static const char noauthmsg[] =
"HTTP/1.1 401 Unauthorized\r\n"
"Server: winetest\r\n"
"Connection: close\r\n"
"WWW-Authenticate: Basic realm=\"placebo\"\r\n"
"Content-Length: 12\r\n"
"Content-Type: text/plain\r\n"
"\r\n";

static const char okauthmsg[] =
"HTTP/1.1 200 OK\r\n"
"Server: winetest\r\n"
"Connection: close\r\n"
"Content-Length: 11\r\n"
"Content-Type: text/plain\r\n"
"\r\n";

static const char headmsg[] =
"HTTP/1.1 200 OK\r\n"
"Content-Length: 100\r\n"
"\r\n";

static const char multiauth[] =
"HTTP/1.1 401 Unauthorized\r\n"
"Server: winetest\r\n"
"WWW-Authenticate: Bearer\r\n"
"WWW-Authenticate: Basic realm=\"placebo\"\r\n"
"WWW-Authenticate: NTLM\r\n"
"Content-Length: 10\r\n"
"Content-Type: text/plain\r\n"
"\r\n";

static const char largeauth[] =
"HTTP/1.1 401 Unauthorized\r\n"
"Server: winetest\r\n"
"WWW-Authenticate: Basic realm=\"placebo\"\r\n"
"WWW-Authenticate: NTLM\r\n"
"Content-Length: 10240\r\n"
"Content-Type: text/plain\r\n"
"\r\n";

static const char passportauth[] =
"HTTP/1.1 302 Found\r\n"
"Content-Length: 0\r\n"
"Location: /\r\n"
"WWW-Authenticate: Passport1.4\r\n"
"\r\n";

static const char switchprotocols[] =
"HTTP/1.1 101 Switching Protocols\r\n"
"Server: winetest\r\n"
"Upgrade: websocket\r\n"
"Connection: Upgrade\r\n";

static const char temp_redirectmsg[] =
"HTTP/1.1 307 Temporary Redirect\r\n"
"Content-Length: 0\r\n"
"Location: /temporary\r\n"
"Connection: close\r\n\r\n";

static const char perm_redirectmsg[] =
"HTTP/1.1 308 Permanent Redirect\r\n"
"Content-Length: 0\r\n"
"Location: /permanent\r\n"
"Connection: close\r\n\r\n";

static const char badreplyheadermsg[] =
"HTTP/1.1 200 OK\r\n"
"Server: winetest\r\n"
"SpaceAfterHdr  :   bad\r\n"
"OkHdr: ok\r\n"
"\r\n";

static const char nostatustext[] =
"HTTP/1.1 200\r\n"
"\r\n";

static const char proxy_pac[] =
"function FindProxyForURL(url, host) {\r\n"
"    url = url.replace(/[:/]/g, '_');\r\n"
"    return 'PROXY ' + url + '_' + host + ':8080';\r\n"
"}\r\n\r\n";

static const char unauthorized[] = "Unauthorized";
static const char hello_world[] = "Hello World";
static const char auth_unseen[] = "Auth Unseen";

struct server_info
{
    HANDLE event;
    int port;
};

#define BIG_BUFFER_LEN 0x2250

static void create_websocket_accept(const char *key, char *buf, unsigned int buflen)
{
    HCRYPTPROV provider;
    HCRYPTHASH hash;
    BYTE sha1[20];
    char data[128];
    DWORD len;

    strcpy(data, key);
    strcat(data, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");

    CryptAcquireContextW(&provider, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);
    CryptCreateHash(provider, CALG_SHA1, 0, 0, &hash);
    CryptHashData(hash, (BYTE *)data, strlen(data), 0);

    len = sizeof(sha1);
    CryptGetHashParam(hash, HP_HASHVAL, sha1, &len, 0);
    CryptDestroyHash(hash);
    CryptReleaseContext(provider, 0);

    buf[0] = 0;
    len = buflen;
    CryptBinaryToStringA( (BYTE *)sha1, sizeof(sha1), CRYPT_STRING_BASE64, buf, &len);
}

static int server_receive_request(int c, char *buffer, size_t buffer_size)
{
    int i, r;

    memset(buffer, 0, buffer_size);
    for(i = 0; i < buffer_size - 1; i++)
    {
        r = recv(c, &buffer[i], 1, 0);
        if (r != 1)
            break;
        if (i < 4) continue;
        if (buffer[i - 2] == '\n' && buffer[i] == '\n' &&
            buffer[i - 3] == '\r' && buffer[i - 1] == '\r')
            break;
    }
    return r;
}

static DWORD CALLBACK server_thread(LPVOID param)
{
    struct server_info *si = param;
    int r, c = -1, i, on;
    SOCKET s;
    struct sockaddr_in sa;
    char buffer[0x100];
    WSADATA wsaData;
    int last_request = 0;

    WSAStartup(MAKEWORD(1,1), &wsaData);

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET)
        return 1;

    on = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof on);

    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port = htons(si->port);
    sa.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");

    r = bind(s, (struct sockaddr *)&sa, sizeof(sa));
    if (r < 0)
        return 1;

    listen(s, 0);
    SetEvent(si->event);
    do
    {
        if (c == -1) c = accept(s, NULL, NULL);
        server_receive_request(c, buffer, sizeof(buffer));
        if (strstr(buffer, "GET /basic"))
        {
            send(c, okmsg, sizeof okmsg - 1, 0);
            send(c, page1, sizeof page1 - 1, 0);
        }
        if (strstr(buffer, "/auth_with_creds"))
        {
            send(c, okauthmsg, sizeof okauthmsg - 1, 0);
            if (strstr(buffer, "Authorization: Basic dXNlcjpwd2Q="))
                send(c, hello_world, sizeof hello_world - 1, 0);
            else
                send(c, auth_unseen, sizeof auth_unseen - 1, 0);
            continue;
        }
        if (strstr(buffer, "/auth"))
        {
            if (strstr(buffer, "Authorization: Basic dXNlcjpwd2Q="))
            {
                send(c, okauthmsg, sizeof okauthmsg - 1, 0);
                send(c, hello_world, sizeof hello_world - 1, 0);
            }
            else
            {
                send(c, noauthmsg, sizeof noauthmsg - 1, 0);
                send(c, unauthorized, sizeof unauthorized - 1, 0);
            }
            continue;
        }
        if (strstr(buffer, "/big"))
        {
            char msg[BIG_BUFFER_LEN];
            memset(msg, 'm', sizeof(msg));
            send(c, okmsg, sizeof(okmsg) - 1, 0);
            send(c, msg, sizeof(msg), 0);
        }
        if (strstr(buffer, "/no_headers"))
        {
            send(c, page1, sizeof page1 - 1, 0);
        }
        if (strstr(buffer, "GET /no_content"))
        {
            send(c, nocontentmsg, sizeof nocontentmsg - 1, 0);
            continue;
        }
        if (strstr(buffer, "GET /not_modified"))
        {
            if (strstr(buffer, "If-Modified-Since:")) send(c, notmodified, sizeof notmodified - 1, 0);
            else send(c, notokmsg, sizeof(notokmsg) - 1, 0);
            continue;
        }
        if (strstr(buffer, "HEAD /head"))
        {
            send(c, headmsg, sizeof headmsg - 1, 0);
            continue;
        }
        if (strstr(buffer, "GET /multiauth"))
        {
            send(c, multiauth, sizeof multiauth - 1, 0);
        }
        if (strstr(buffer, "GET /largeauth"))
        {
            if (strstr(buffer, "Authorization: NTLM"))
                send(c, okmsg, sizeof(okmsg) - 1, 0);
            else
            {
                send(c, largeauth, sizeof largeauth - 1, 0);
                for (i = 0; i < 10240; i++) send(c, "A", 1, 0);
                continue;
            }
        }
        if (strstr(buffer, "GET /cookie5"))
        {
            if (strstr(buffer, "Cookie: name2=value\r\n"))
                send(c, okmsg, sizeof(okmsg) - 1, 0);
            else
                send(c, notokmsg, sizeof(notokmsg) - 1, 0);
        }
        if (strstr(buffer, "GET /cookie4"))
        {
            send(c, cookiemsg2, sizeof(cookiemsg2) - 1, 0);
        }
        if (strstr(buffer, "GET /cookie3"))
        {
            if (strstr(buffer, "Cookie: name=value2; NAME=value; name=value\r\n") ||
                broken(strstr(buffer, "Cookie: name=value2; name=value; NAME=value\r\n") != NULL))
                send(c, okmsg, sizeof(okmsg) - 1, 0);
            else
                send(c, notokmsg, sizeof(notokmsg) - 1, 0);
        }
        if (strstr(buffer, "GET /cookie2"))
        {
            if (strstr(buffer, "Cookie: NAME=value; name=value\r\n") ||
                broken(strstr(buffer, "Cookie: name=value; NAME=value\r\n") != NULL))
                send(c, okmsg, sizeof(okmsg) - 1, 0);
            else
                send(c, notokmsg, sizeof(notokmsg) - 1, 0);
        }
        else if (strstr(buffer, "GET /cookie"))
        {
            if (!strstr(buffer, "Cookie: name=value\r\n")) send(c, cookiemsg, sizeof(cookiemsg) - 1, 0);
            else send(c, notokmsg, sizeof(notokmsg) - 1, 0);
        }
        else if (strstr(buffer, "GET /escape"))
        {
            static const char res[] = "%0D%0A%1F%7F%3C%20%one?%1F%7F%20!%22%23$%&'()*+,-./:;%3C=%3E?@%5B%5C%5D"
                                      "%5E_%60%7B%7C%7D~%0D%0A ";
            static const char res2[] = "%0D%0A%1F%7F%3C%20%25two?%1F%7F%20!%22%23$%25&'()*+,-./:;%3C=%3E?@%5B%5C%5D"
                                       "%5E_%60%7B%7C%7D~%0D%0A ";
            static const char res3[] = "\x1f\x7f<%20%three?\x1f\x7f%20!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~ ";
            static const char res4[] = "%0D%0A%1F%7F%3C%20%four?\x1f\x7f%20!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~ ";
            static const char res5[] = "&text=one%C2%80%7F~";
            static const char res6[] = "&text=two%C2%80\x7f~";
            static const char res7[] = "&text=%E5%90%9B%E3%81%AE%E5%90%8D%E3%81%AF";

            if (strstr(buffer + 11, res) || strstr(buffer + 11, res2) || strstr(buffer + 11, res3) ||
                strstr(buffer + 11, res4) || strstr(buffer + 11, res5) || strstr(buffer + 11, res6) ||
                strstr(buffer + 11, res7))
            {
                send(c, okmsg, sizeof(okmsg) - 1, 0);
            }
            else send(c, notokmsg, sizeof(notokmsg) - 1, 0);
        }
        else if (strstr(buffer, "GET /passport"))
        {
            send(c, passportauth, sizeof(passportauth) - 1, 0);
        }
        else if (strstr(buffer, "GET /websocket"))
        {
            char headers[256], key[32], accept[64];
            const char *pos = strstr(buffer, "Sec-WebSocket-Key: ");
            if (pos && strstr(buffer, "Connection: Upgrade\r\n") &&
                (strstr(buffer, "Upgrade: websocket\r\n") || strstr(buffer, "Upgrade: Websocket\r\n")) &&
                strstr(buffer, "Host: ") && strstr(buffer, "Sec-WebSocket-Version: 13\r\n"))
            {
                memcpy(headers, switchprotocols, sizeof(switchprotocols));
                memcpy(key, pos + 19, 24);
                key[24] = 0;

                create_websocket_accept(key, accept, sizeof(accept));

                strcat(headers, "Sec-WebSocket-Accept: ");
                strcat(headers, accept);
                strcat(headers, "\r\n\r\n");

                send(c, headers, strlen(headers), 0);
                continue;
            }
            else send(c, notokmsg, sizeof(notokmsg) - 1, 0);
        }
        else if (strstr(buffer, "POST /redirect-temp"))
        {
            send(c, temp_redirectmsg, sizeof temp_redirectmsg - 1, 0);
        }
        else if (strstr(buffer, "POST /redirect-perm"))
        {
            send(c, perm_redirectmsg, sizeof perm_redirectmsg - 1, 0);
        }
        else if (strstr(buffer, "POST /temporary") || strstr(buffer, "POST /permanent"))
        {
            char buf[32];
            recv(c, buf, sizeof(buf), 0);
            send(c, okmsg, sizeof okmsg - 1, 0);
            send(c, page1, sizeof page1 - 1, 0);
        }
        if (strstr(buffer, "GET /quit"))
        {
            send(c, okmsg, sizeof okmsg - 1, 0);
            send(c, page1, sizeof page1 - 1, 0);
            last_request = 1;
        }
        if (strstr(buffer, "POST /bad_headers"))
        {
            ok(!!strstr(buffer, "Content-Type: text/html\r\n"), "Header missing from request %s.\n", debugstr_a(buffer));
            ok(!!strstr(buffer, "Test1: Value1\r\n"), "Header missing from request %s.\n", debugstr_a(buffer));
            ok(!!strstr(buffer, "Test2: Value2\r\n"), "Header missing from request %s.\n", debugstr_a(buffer));
            ok(!!strstr(buffer, "Test3: Value3\r\n"), "Header missing from request %s.\n", debugstr_a(buffer));
            ok(!!strstr(buffer, "Test4: Value4\r\n"), "Header missing from request %s.\n", debugstr_a(buffer));
            ok(!!strstr(buffer, "Test5: Value5\r\n"), "Header missing from request %s.\n", debugstr_a(buffer));
            ok(!!strstr(buffer, "Test6: Value6\r\n"), "Header missing from request %s.\n", debugstr_a(buffer));
            ok(!!strstr(buffer, "Cookie: 111\r\n"), "Header missing from request %s.\n", debugstr_a(buffer));
            send(c, badreplyheadermsg, sizeof(badreplyheadermsg) - 1, 0);
        }
        if (strstr(buffer, "GET /proxy.pac"))
        {
            send(c, okmsg, sizeof(okmsg) - 1, 0);
            send(c, proxy_pac, sizeof(proxy_pac) - 1, 0);
        }

        if (strstr(buffer, "PUT /test") || strstr(buffer, "POST /test"))
        {
            if (strstr(buffer, "Transfer-Encoding: chunked\r\n"))
            {
                ok(!strstr(buffer, "Content-Length:"), "Unexpected Content-Length in request %s.\n", debugstr_a(buffer));
                r = recv(c, buffer, sizeof(buffer), 0);
                ok(r == 4, "got %d.\n", r);
                buffer[r] = 0;
                ok(!strcmp(buffer, "post"), "got %s.\n", debugstr_a(buffer));
            }
            else
            {
                ok(!!strstr(buffer, "Content-Length: 0\r\n"), "Header missing from request %s.\n", debugstr_a(buffer));
            }
            send(c, okmsg, sizeof(okmsg) - 1, 0);
        }

        if (strstr(buffer, "GET /cached"))
        {
            send(c, okmsg_length0, sizeof okmsg_length0 - 1, 0);
            r = server_receive_request(c, buffer, sizeof(buffer));
            ok(r > 0, "got %d.\n", r);
            ok(!!strstr(buffer, "GET /cached"), "request not found.\n");
            send(c, okmsg_length0, sizeof okmsg_length0 - 1, 0);
            r = server_receive_request(c, buffer, sizeof(buffer));
            ok(!r, "got %d, buffer[0] %d.\n", r, buffer[0]);
        }
        if (strstr(buffer, "GET /notcached"))
        {
            send(c, okmsg, sizeof okmsg - 1, 0);
            r = server_receive_request(c, buffer, sizeof(buffer));
            ok(!r, "got %d, buffer[0] %d.\n", r, buffer[0] );
        }
        if (strstr(buffer, "GET /nostatustext"))
        {
            send(c, nostatustext, sizeof nostatustext - 1, 0);
            r = server_receive_request(c, buffer, sizeof(buffer));
            ok(!r, "got %d, buffer[0] %d.\n", r, buffer[0] );
        }
        shutdown(c, 2);
        closesocket(c);
        c = -1;

    } while (!last_request);

    closesocket(s);
    return 0;
}

static void test_basic_request(int port, const WCHAR *verb, const WCHAR *path)
{
    HINTERNET ses, con, req;
    char buffer[0x100];
    WCHAR buffer2[0x100];
    DWORD count, status, size, error, supported, first, target;
    BOOL ret;

    ses = WinHttpOpen(L"winetest", WINHTTP_ACCESS_TYPE_NO_PROXY, NULL, NULL, 0);
    ok(ses != NULL, "failed to open session %lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = WinHttpSetOption(ses, 0, buffer, sizeof(buffer));
    ok(!ret && GetLastError() == ERROR_WINHTTP_INVALID_OPTION, "got %lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = WinHttpQueryOption(ses, 0, buffer, &size);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "got %lu\n", GetLastError());

    con = WinHttpConnect(ses, L"localhost", port, 0);
    ok(con != NULL, "failed to open a connection %lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = WinHttpSetOption(con, 0, buffer, sizeof(buffer));
    todo_wine ok(!ret && GetLastError() == ERROR_WINHTTP_INVALID_OPTION, "got %lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = WinHttpQueryOption(con, 0, buffer, &size);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "got %lu\n", GetLastError());

    req = WinHttpOpenRequest(con, verb, path, NULL, NULL, NULL, 0);
    ok(req != NULL, "failed to open a request %lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = WinHttpSetOption(req, 0, buffer, sizeof(buffer));
    ok(!ret && GetLastError() == ERROR_WINHTTP_INVALID_OPTION, "got %lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = WinHttpQueryOption(req, 0, buffer, &size);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "got %lu\n", GetLastError());

    ret = WinHttpSendRequest(req, NULL, 0, NULL, 0, 0, 0);
    ok(ret, "failed to send request %lu\n", GetLastError());

    ret = WinHttpReceiveResponse(req, NULL);
    ok(ret, "failed to receive response %lu\n", GetLastError());

    status = 0xdeadbeef;
    size = sizeof(status);
    ret = WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &size, NULL);
    ok(ret, "failed to query status code %lu\n", GetLastError());
    ok(status == HTTP_STATUS_OK, "request failed unexpectedly %lu\n", status);

    supported = first = target = 0xdeadbeef;
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryAuthSchemes(req, &supported, &first, &target);
    error = GetLastError();
    ok(!ret, "unexpected success\n");
    ok(error == ERROR_INVALID_OPERATION, "expected ERROR_INVALID_OPERATION, got %lu\n", error);
    ok(supported == 0xdeadbeef, "got %lu\n", supported);
    ok(first == 0xdeadbeef, "got %lu\n", first);
    ok(target == 0xdeadbeef, "got %lu\n", target);

    size = sizeof(buffer2);
    memset(buffer2, 0, sizeof(buffer2));
    ret = WinHttpQueryHeaders(req, WINHTTP_QUERY_RAW_HEADERS_CRLF, NULL, buffer2, &size, NULL);
    ok(ret, "failed to query for raw headers: %lu\n", GetLastError());
    ok(!memcmp(buffer2 + lstrlenW(buffer2) - 4, L"\r\n\r\n", sizeof(L"\r\n\r\n")),
       "WinHttpQueryHeaders returned invalid end of header string\n");

    size = sizeof(buffer2);
    memset(buffer2, 0, sizeof(buffer2));
    ret = WinHttpQueryHeaders(req, WINHTTP_QUERY_RAW_HEADERS, NULL, buffer2, &size, NULL);
    ok(ret, "failed to query for raw headers: %lu\n", GetLastError());
    ok(!memcmp(buffer2 + (size / sizeof(WCHAR)) - 1, L"", sizeof(L"")),
       "WinHttpQueryHeaders returned invalid end of header string\n");
    ok(buffer2[(size / sizeof(WCHAR)) - 2] != 0, "returned string has too many NULL characters\n");

    count = 0;
    memset(buffer, 0, sizeof(buffer));
    ret = WinHttpReadData(req, buffer, sizeof buffer, &count);
    ok(ret, "failed to read data %lu\n", GetLastError());
    if (verb && !wcscmp(verb, L"PUT"))
    {
        ok(!count, "got count %ld\n", count);
    }
    else
    {
        ok(count == sizeof page1 - 1, "got count %ld\n", count);
        ok(!memcmp(buffer, page1, sizeof page1), "http data wrong\n");
    }

    WinHttpCloseHandle(req);
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
}

static void test_chunked_request(int port)
{
    static const WCHAR *methods[] = {L"POST", L"PUT"};
    HINTERNET ses, con, req;
    char buffer[0x100];
    unsigned int i;
    DWORD count;
    BOOL ret;

    ses = WinHttpOpen(L"winetest", WINHTTP_ACCESS_TYPE_NO_PROXY, NULL, NULL, 0);
    ok(ses != NULL, "failed to open session %lu\n", GetLastError());

    con = WinHttpConnect(ses, L"localhost", port, 0);
    ok(con != NULL, "failed to open a connection %lu\n", GetLastError());
    for (i = 0; i < ARRAY_SIZE(methods); ++i)
    {
        req = WinHttpOpenRequest(con, methods[i], L"/test", NULL, NULL, NULL, 0);
        ok(req != NULL, "failed to open a request %lu\n", GetLastError());

        ret = WinHttpAddRequestHeaders(req, L"Transfer-Encoding: chunked", -1, WINHTTP_ADDREQ_FLAG_ADD);
        ok(ret, "failed to add header %lu\n", GetLastError());

        strcpy(buffer, "post");
        ret = WinHttpSendRequest(req, NULL, 0, buffer, 4, 4, 0);
        ok(ret, "failed to send request %lu\n", GetLastError());
        ret = WinHttpReceiveResponse(req, NULL);
        ok(ret, "failed to receive response %lu\n", GetLastError());
        count = 0;
        memset(buffer, 0, sizeof(buffer));
        ret = WinHttpReadData(req, buffer, sizeof buffer, &count);
        ok(ret, "failed to read data %lu\n", GetLastError());
        ok(!count, "got count %ld\n", count);
        WinHttpCloseHandle(req);
    }
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
}

static void test_basic_authentication(int port)
{
    HINTERNET ses, con, req;
    DWORD status, size, error, supported, first, target;
    char buffer[32];
    BOOL ret;

    ses = WinHttpOpen(L"winetest", WINHTTP_ACCESS_TYPE_NO_PROXY, NULL, NULL, 0);
    ok(ses != NULL, "failed to open session %lu\n", GetLastError());

    con = WinHttpConnect(ses, L"localhost", port, 0);
    ok(con != NULL, "failed to open a connection %lu\n", GetLastError());

    req = WinHttpOpenRequest(con, NULL, L"/auth", NULL, NULL, NULL, 0);
    ok(req != NULL, "failed to open a request %lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = WinHttpQueryAuthSchemes(NULL, NULL, NULL, NULL);
    error = GetLastError();
    ok(!ret, "expected failure\n");
    ok(error == ERROR_INVALID_HANDLE, "expected ERROR_INVALID_HANDLE, got %lu\n", error);

    SetLastError(0xdeadbeef);
    ret = WinHttpQueryAuthSchemes(req, NULL, NULL, NULL);
    error = GetLastError();
    ok(!ret, "expected failure\n");
    ok(error == ERROR_INVALID_PARAMETER || error == ERROR_INVALID_OPERATION, "got %lu\n", error);

    supported = 0xdeadbeef;
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryAuthSchemes(req, &supported, NULL, NULL);
    error = GetLastError();
    ok(!ret, "expected failure\n");
    ok(error == ERROR_INVALID_PARAMETER || error == ERROR_INVALID_OPERATION, "got %lu\n", error);
    ok(supported == 0xdeadbeef, "got %lu\n", supported);

    supported = first = 0xdeadbeef;
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryAuthSchemes(req, &supported, &first, NULL);
    error = GetLastError();
    ok(!ret, "expected failure\n");
    ok(error == ERROR_INVALID_PARAMETER || error == ERROR_INVALID_OPERATION, "got %lu\n", error);
    ok(supported == 0xdeadbeef, "got %lu\n", supported);
    ok(first == 0xdeadbeef, "got %lu\n", first);

    supported = first = target = 0xdeadbeef;
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryAuthSchemes(req, &supported, &first, &target);
    error = GetLastError();
    ok(!ret, "expected failure\n");
    ok(error == ERROR_INVALID_OPERATION, "expected ERROR_INVALID_OPERATION, got %lu\n", error);
    ok(supported == 0xdeadbeef, "got %lu\n", supported);
    ok(first == 0xdeadbeef, "got %lu\n", first);
    ok(target == 0xdeadbeef, "got %lu\n", target);

    supported = first = target = 0xdeadbeef;
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryAuthSchemes(NULL, &supported, &first, &target);
    error = GetLastError();
    ok(!ret, "expected failure\n");
    ok(error == ERROR_INVALID_HANDLE, "expected ERROR_INVALID_HANDLE, got %lu\n", error);
    ok(supported == 0xdeadbeef, "got %lu\n", supported);
    ok(first == 0xdeadbeef, "got %lu\n", first);
    ok(target == 0xdeadbeef, "got %lu\n", target);

    ret = WinHttpSendRequest(req, NULL, 0, NULL, 0, 0, 0);
    ok(ret, "failed to send request %lu\n", GetLastError());

    ret = WinHttpReceiveResponse(req, NULL);
    ok(ret, "failed to receive response %lu\n", GetLastError());

    status = 0xdeadbeef;
    size = sizeof(status);
    ret = WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &size, NULL);
    ok(ret, "failed to query status code %lu\n", GetLastError());
    ok(status == HTTP_STATUS_DENIED, "request failed unexpectedly %lu\n", status);

    size = 0;
    ret = WinHttpReadData(req, buffer, sizeof(buffer), &size);
    error = GetLastError();
    ok(ret || broken(error == ERROR_WINHTTP_SHUTDOWN || error == ERROR_WINHTTP_TIMEOUT) /* XP */, "failed to read data %lu\n", GetLastError());
    if (ret)
    {
        ok(size == 12, "expected 12, got %lu\n", size);
        ok(!memcmp(buffer, unauthorized, 12), "got %s\n", buffer);
    }

    supported = first = target = 0xdeadbeef;
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryAuthSchemes(req, &supported, &first, &target);
    error = GetLastError();
    ok(ret, "failed to query authentication schemes %lu\n", error);
    ok(error == ERROR_SUCCESS || broken(error == 0xdeadbeef) /* < win7 */, "expected ERROR_SUCCESS, got %lu\n", error);
    ok(supported == WINHTTP_AUTH_SCHEME_BASIC, "got %lu\n", supported);
    ok(first == WINHTTP_AUTH_SCHEME_BASIC, "got %lu\n", first);
    ok(target == WINHTTP_AUTH_TARGET_SERVER, "got %lu\n", target);

    SetLastError(0xdeadbeef);
    ret = WinHttpSetCredentials(req, WINHTTP_AUTH_TARGET_SERVER, WINHTTP_AUTH_SCHEME_NTLM, NULL, NULL, NULL);
    error = GetLastError();
    ok(ret, "failed to set credentials %lu\n", error);
    ok(error == ERROR_SUCCESS || broken(error == 0xdeadbeef) /* < win7 */, "expected ERROR_SUCCESS, got %lu\n", error);

    ret = WinHttpSetCredentials(req, WINHTTP_AUTH_TARGET_SERVER, WINHTTP_AUTH_SCHEME_PASSPORT, NULL, NULL, NULL);
    ok(ret, "failed to set credentials %lu\n", GetLastError());

    ret = WinHttpSetCredentials(req, WINHTTP_AUTH_TARGET_SERVER, WINHTTP_AUTH_SCHEME_NEGOTIATE, NULL, NULL, NULL);
    ok(ret, "failed to set credentials %lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = WinHttpSetCredentials(req, WINHTTP_AUTH_TARGET_SERVER, WINHTTP_AUTH_SCHEME_DIGEST, NULL, NULL, NULL);
    error = GetLastError();
    ok(!ret, "expected failure\n");
    ok(error == ERROR_INVALID_PARAMETER, "expected ERROR_INVALID_PARAMETER, got %lu\n", error);

    SetLastError(0xdeadbeef);
    ret = WinHttpSetCredentials(req, WINHTTP_AUTH_TARGET_SERVER, WINHTTP_AUTH_SCHEME_BASIC, NULL, NULL, NULL);
    error = GetLastError();
    ok(!ret, "expected failure\n");
    ok(error == ERROR_INVALID_PARAMETER, "expected ERROR_INVALID_PARAMETER, got %lu\n", error);

    SetLastError(0xdeadbeef);
    ret = WinHttpSetCredentials(req, WINHTTP_AUTH_TARGET_SERVER, WINHTTP_AUTH_SCHEME_BASIC, L"user", NULL, NULL);
    error = GetLastError();
    ok(!ret, "expected failure\n");
    ok(error == ERROR_INVALID_PARAMETER, "expected ERROR_INVALID_PARAMETER, got %lu\n", error);

    SetLastError(0xdeadbeef);
    ret = WinHttpSetCredentials(req, WINHTTP_AUTH_TARGET_SERVER, WINHTTP_AUTH_SCHEME_BASIC, NULL, L"pwd", NULL);
    error = GetLastError();
    ok(!ret, "expected failure\n");
    ok(error == ERROR_INVALID_PARAMETER, "expected ERROR_INVALID_PARAMETER, got %lu\n", error);

    ret = WinHttpSetCredentials(req, WINHTTP_AUTH_TARGET_SERVER, WINHTTP_AUTH_SCHEME_BASIC, L"user", L"pwd", NULL);
    ok(ret, "failed to set credentials %lu\n", GetLastError());

    ret = WinHttpSendRequest(req, NULL, 0, NULL, 0, 0, 0);
    ok(ret, "failed to send request %lu\n", GetLastError());

    ret = WinHttpReceiveResponse(req, NULL);
    ok(ret, "failed to receive response %lu\n", GetLastError());

    status = 0xdeadbeef;
    size = sizeof(status);
    ret = WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &size, NULL);
    ok(ret, "failed to query status code %lu\n", GetLastError());
    ok(status == HTTP_STATUS_OK, "request failed unexpectedly %lu\n", status);

    size = 0;
    ret = WinHttpReadData(req, buffer, sizeof(buffer), &size);
    error = GetLastError();
    ok(ret || broken(error == ERROR_WINHTTP_SHUTDOWN || error == ERROR_WINHTTP_TIMEOUT) /* XP */, "failed to read data %lu\n", GetLastError());
    if (ret)
    {
        ok(size == 11, "expected 11, got %lu\n", size);
        ok(!memcmp(buffer, hello_world, 11), "got %s\n", buffer);
    }

    WinHttpCloseHandle(req);
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);

    /* now set the credentials first to show that they get sent with the first request */
    ses = WinHttpOpen(L"winetest", WINHTTP_ACCESS_TYPE_NO_PROXY, NULL, NULL, 0);
    ok(ses != NULL, "failed to open session %lu\n", GetLastError());

    con = WinHttpConnect(ses, L"localhost", port, 0);
    ok(con != NULL, "failed to open a connection %lu\n", GetLastError());

    req = WinHttpOpenRequest(con, NULL, L"/auth_with_creds", NULL, NULL, NULL, 0);
    ok(req != NULL, "failed to open a request %lu\n", GetLastError());

    ret = WinHttpSetCredentials(req, WINHTTP_AUTH_TARGET_SERVER, WINHTTP_AUTH_SCHEME_BASIC, L"user", L"pwd", NULL);
    ok(ret, "failed to set credentials %lu\n", GetLastError());

    ret = WinHttpSendRequest(req, NULL, 0, NULL, 0, 0, 0);
    ok(ret, "failed to send request %lu\n", GetLastError());

    ret = WinHttpReceiveResponse(req, NULL);
    ok(ret, "failed to receive response %lu\n", GetLastError());

    status = 0xdeadbeef;
    size = sizeof(status);
    ret = WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &size, NULL);
    ok(ret, "failed to query status code %lu\n", GetLastError());
    ok(status == HTTP_STATUS_OK, "request failed unexpectedly %lu\n", status);

    size = 0;
    ret = WinHttpReadData(req, buffer, sizeof(buffer), &size);
    error = GetLastError();
    ok(ret || broken(error == ERROR_WINHTTP_SHUTDOWN || error == ERROR_WINHTTP_TIMEOUT) /* XP */, "failed to read data %lu\n", GetLastError());
    if (ret)
    {
        ok(size == 11, "expected 11, got %lu\n", size);
        ok(!memcmp(buffer, hello_world, 11), "got %s\n", buffer);
    }

    WinHttpCloseHandle(req);
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);

    /* credentials set with WinHttpSetCredentials take precedence over those set through options */

    ses = WinHttpOpen(L"winetest", WINHTTP_ACCESS_TYPE_NO_PROXY, NULL, NULL, 0);
    ok(ses != NULL, "failed to open session %lu\n", GetLastError());

    con = WinHttpConnect(ses, L"localhost", port, 0);
    ok(con != NULL, "failed to open a connection %lu\n", GetLastError());

    req = WinHttpOpenRequest(con, NULL, L"/auth", NULL, NULL, NULL, 0);
    ok(req != NULL, "failed to open a request %lu\n", GetLastError());

    ret = WinHttpSetCredentials(req, WINHTTP_AUTH_TARGET_SERVER, WINHTTP_AUTH_SCHEME_BASIC, L"user", L"pwd", NULL);
    ok(ret, "failed to set credentials %lu\n", GetLastError());

    ret = WinHttpSetOption(req, WINHTTP_OPTION_USERNAME, (void *)L"user", lstrlenW(L"user"));
    ok(ret, "failed to set username %lu\n", GetLastError());

    ret = WinHttpSetOption(req, WINHTTP_OPTION_PASSWORD, (void *)L"pwd2", lstrlenW(L"pwd2"));
    ok(ret, "failed to set password %lu\n", GetLastError());

    ret = WinHttpSendRequest(req, NULL, 0, NULL, 0, 0, 0);
    ok(ret, "failed to send request %lu\n", GetLastError());

    ret = WinHttpReceiveResponse(req, NULL);
    ok(ret, "failed to receive response %lu\n", GetLastError());

    status = 0xdeadbeef;
    size = sizeof(status);
    ret = WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &size, NULL);
    ok(ret, "failed to query status code %lu\n", GetLastError());
    ok(status == HTTP_STATUS_OK, "request failed unexpectedly %lu\n", status);

    WinHttpCloseHandle(req);
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);

    ses = WinHttpOpen(L"winetest", WINHTTP_ACCESS_TYPE_NO_PROXY, NULL, NULL, 0);
    ok(ses != NULL, "failed to open session %lu\n", GetLastError());

    con = WinHttpConnect(ses, L"localhost", port, 0);
    ok(con != NULL, "failed to open a connection %lu\n", GetLastError());

    req = WinHttpOpenRequest(con, NULL, L"/auth", NULL, NULL, NULL, 0);
    ok(req != NULL, "failed to open a request %lu\n", GetLastError());

    ret = WinHttpSetOption(req, WINHTTP_OPTION_USERNAME, (void *)L"user", lstrlenW(L"user"));
    ok(ret, "failed to set username %lu\n", GetLastError());

    ret = WinHttpSetOption(req, WINHTTP_OPTION_PASSWORD, (void *)L"pwd", lstrlenW(L"pwd"));
    ok(ret, "failed to set password %lu\n", GetLastError());

    ret = WinHttpSetCredentials(req, WINHTTP_AUTH_TARGET_SERVER, WINHTTP_AUTH_SCHEME_BASIC, L"user", L"pwd2", NULL);
    ok(ret, "failed to set credentials %lu\n", GetLastError());

    ret = WinHttpSendRequest(req, NULL, 0, NULL, 0, 0, 0);
    ok(ret, "failed to send request %lu\n", GetLastError());

    ret = WinHttpReceiveResponse(req, NULL);
    ok(ret, "failed to receive response %lu\n", GetLastError());

    status = 0xdeadbeef;
    size = sizeof(status);
    ret = WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &size, NULL);
    ok(ret, "failed to query status code %lu\n", GetLastError());
    ok(status == HTTP_STATUS_DENIED, "request failed unexpectedly %lu\n", status);

    WinHttpCloseHandle(req);
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
}

static void test_multi_authentication(int port)
{
    HINTERNET ses, con, req;
    DWORD supported, first, target, size, index;
    WCHAR buf[512];
    BOOL ret;

    ses = WinHttpOpen(L"winetest", WINHTTP_ACCESS_TYPE_NO_PROXY, NULL, NULL, 0);
    ok(ses != NULL, "failed to open session %lu\n", GetLastError());

    con = WinHttpConnect(ses, L"localhost", port, 0);
    ok(con != NULL, "failed to open a connection %lu\n", GetLastError());

    req = WinHttpOpenRequest(con, L"GET", L"/multiauth", NULL, NULL, NULL, 0);
    ok(req != NULL, "failed to open a request %lu\n", GetLastError());

    ret = WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                             WINHTTP_NO_REQUEST_DATA,0, 0, 0 );
    ok(ret, "expected success\n");

    ret = WinHttpReceiveResponse(req, NULL);
    ok(ret, "expected success\n");

    supported = first = target = 0xdeadbeef;
    ret = WinHttpQueryAuthSchemes(req, &supported, &first, &target);
    ok(ret, "expected success\n");
    ok(supported == (WINHTTP_AUTH_SCHEME_BASIC | WINHTTP_AUTH_SCHEME_NTLM), "got %#lx\n", supported);
    ok(target == WINHTTP_AUTH_TARGET_SERVER, "got %#lx\n", target);
    ok(first == WINHTTP_AUTH_SCHEME_BASIC, "got %#lx\n", first);

    index = 0;
    size = sizeof(buf);
    ret = WinHttpQueryHeaders(req, WINHTTP_QUERY_CUSTOM, L"WWW-Authenticate", buf, &size, &index);
    ok(ret, "expected success\n");
    ok(!lstrcmpW(buf, L"Bearer"), "buf = %s\n", wine_dbgstr_w(buf));
    ok(size == lstrlenW(buf) * sizeof(WCHAR), "size = %lu\n", size);
    ok(index == 1, "index = %lu\n", index);

    index = 0;
    size = 0xdeadbeef;
    ret = WinHttpQueryHeaders(req, WINHTTP_QUERY_CUSTOM, L"WWW-Authenticate", NULL, &size, &index);
    ok(!ret && GetLastError() == ERROR_INSUFFICIENT_BUFFER,
       "WinHttpQueryHeaders returned %d(%lu)\n", ret, GetLastError());
    ok(size == (lstrlenW(buf) + 1) * sizeof(WCHAR), "size = %lu\n", size);
    ok(index == 0, "index = %lu\n", index);

    WinHttpCloseHandle(req);
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
}

static void test_large_data_authentication(int port)
{
    HINTERNET ses, con, req;
    DWORD status, size;
    BOOL ret;

    ses = WinHttpOpen(L"winetest", WINHTTP_ACCESS_TYPE_NO_PROXY, NULL, NULL, 0);
    ok(ses != NULL, "failed to open session %lu\n", GetLastError());

    con = WinHttpConnect(ses, L"localhost", port, 0);
    ok(con != NULL, "failed to open a connection %lu\n", GetLastError());

    req = WinHttpOpenRequest(con, L"GET", L"/largeauth", NULL, NULL, NULL, 0);
    ok(req != NULL, "failed to open a request %lu\n", GetLastError());

    ret = WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    ok(ret, "expected success\n");

    ret = WinHttpReceiveResponse(req, NULL);
    ok(ret, "expected success\n");

    size = sizeof(status);
    ret = WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL,
                              &status, &size, NULL);
    ok(ret, "expected success\n");
    ok(status == HTTP_STATUS_DENIED, "got %lu\n", status);

    ret = WinHttpSetCredentials(req, WINHTTP_AUTH_TARGET_SERVER, WINHTTP_AUTH_SCHEME_NTLM, L"user", L"pwd", NULL);
    ok(ret, "expected success\n");

    ret = WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    ok(ret, "expected success %lu\n", GetLastError());

    ret = WinHttpReceiveResponse(req, NULL);
    ok(ret, "expected success\n");

    size = sizeof(status);
    ret = WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL,
                              &status, &size, NULL);
    ok(ret, "expected success\n");
    ok(status == HTTP_STATUS_OK, "got %lu\n", status);

    WinHttpCloseHandle(req);
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
}

static void test_no_headers(int port)
{
    HINTERNET ses, con, req;
    DWORD error, size;
    WCHAR buf[10];
    BOOL ret;

    ses = WinHttpOpen(L"winetest", WINHTTP_ACCESS_TYPE_NO_PROXY, NULL, NULL, 0);
    ok(ses != NULL, "failed to open session %lu\n", GetLastError());

    con = WinHttpConnect(ses, L"localhost", port, 0);
    ok(con != NULL, "failed to open a connection %lu\n", GetLastError());

    req = WinHttpOpenRequest(con, NULL, L"/no_headers", NULL, NULL, NULL, 0);
    ok(req != NULL, "failed to open a request %lu\n", GetLastError());

    ret = WinHttpSendRequest(req, NULL, 0, NULL, 0, 0, 0);
    if (!ret)
    {
        error = GetLastError();
        ok(error == ERROR_WINHTTP_INVALID_SERVER_RESPONSE, "got %lu\n", error);
    }
    else
    {
        SetLastError(0xdeadbeef);
        ret = WinHttpReceiveResponse(req, NULL);
        error = GetLastError();
        ok(!ret, "expected failure\n");
        ok(error == ERROR_WINHTTP_INVALID_SERVER_RESPONSE, "got %lu\n", error);
    }

    WinHttpCloseHandle(req);

    req = WinHttpOpenRequest(con, NULL, L"/nostatustext", NULL, NULL, NULL, 0);
    ok(req != NULL, "failed to open a request %lu\n", GetLastError());

    ret = WinHttpSendRequest(req, NULL, 0, NULL, 0, 0, 0);
    ok(ret, "got %lu\n", GetLastError());

    ret = WinHttpReceiveResponse(req, NULL);
    ok(ret, "got %lu\n", GetLastError());

    memset(buf, 0xcc, sizeof(buf));
    size = sizeof(buf);
    ret = WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_TEXT, NULL, buf, &size, NULL);
    ok(ret, "got %lu\n", GetLastError());
    ok(!buf[0], "got %x\n", buf[0]);

    WinHttpCloseHandle(req);
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
}

static void test_no_content(int port)
{
    HINTERNET ses, con, req;
    char buf[128];
    DWORD size, len = sizeof(buf), bytes_read, status;
    BOOL ret;

    ses = WinHttpOpen(L"winetest", WINHTTP_ACCESS_TYPE_NO_PROXY, NULL, NULL, 0);
    ok(ses != NULL, "failed to open session %lu\n", GetLastError());

    con = WinHttpConnect(ses, L"localhost", port, 0);
    ok(con != NULL, "failed to open a connection %lu\n", GetLastError());

    req = WinHttpOpenRequest(con, NULL, L"/no_content", NULL, NULL, NULL, 0);
    ok(req != NULL, "failed to open a request %lu\n", GetLastError());

    size = 12345;
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryDataAvailable(req, &size);
    todo_wine {
    ok(!ret, "expected error\n");
    ok(GetLastError() == ERROR_WINHTTP_INCORRECT_HANDLE_STATE,
       "expected ERROR_WINHTTP_INCORRECT_HANDLE_STATE, got %lu\n", GetLastError());
    ok(size == 12345 || broken(size == 0) /* Win <= 2003 */,
       "expected 12345, got %lu\n", size);
    }

    ret = WinHttpSendRequest(req, NULL, 0, NULL, 0, 0, 0);
    ok(ret, "expected success\n");

    ret = WinHttpReceiveResponse(req, NULL);
    ok(ret, "expected success\n");

    status = 0xdeadbeef;
    size = sizeof(status);
    ret = WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                              NULL, &status, &size, NULL);
    ok(ret, "expected success\n");
    ok(status == HTTP_STATUS_NO_CONTENT, "expected status 204, got %lu\n", status);

    SetLastError(0xdeadbeef);
    size = sizeof(status);
    status = 12345;
    ret = WinHttpQueryHeaders(req, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                              NULL, &status, &size, 0);
    ok(!ret, "expected no content-length header\n");
    ok(GetLastError() == ERROR_WINHTTP_HEADER_NOT_FOUND, "wrong error %lu\n", GetLastError());
    ok(status == 12345, "expected 0, got %lu\n", status);

    SetLastError(0xdeadbeef);
    size = 12345;
    ret = WinHttpQueryDataAvailable(req, &size);
    ok(ret, "expected success\n");
    ok(GetLastError() == ERROR_SUCCESS || broken(GetLastError() == 0xdeadbeef) /* < win7 */,
       "wrong error %lu\n", GetLastError());
    ok(!size, "expected 0, got %lu\n", size);

    SetLastError(0xdeadbeef);
    ret = WinHttpReadData(req, buf, len, &bytes_read);
    ok(ret, "expected success\n");
    ok(GetLastError() == ERROR_SUCCESS || broken(GetLastError() == 0xdeadbeef) /* < win7 */,
       "wrong error %lu\n", GetLastError());
    ok(!bytes_read, "expected 0, got %lu\n", bytes_read);

    size = 12345;
    ret = WinHttpQueryDataAvailable(req, &size);
    ok(ret, "expected success\n");
    ok(size == 0, "expected 0, got %lu\n", size);

    WinHttpCloseHandle(req);

    size = 12345;
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryDataAvailable(req, &size);
    ok(!ret, "expected error\n");
    ok(GetLastError() == ERROR_INVALID_HANDLE, "expected ERROR_INVALID_HANDLE, got %#lx\n", GetLastError());
    ok(size == 12345, "expected 12345, got %lu\n", size);

    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
}

static void test_head_request(int port)
{
    HINTERNET ses, con, req;
    char buf[128];
    DWORD size, len, count, status;
    BOOL ret;

    ses = WinHttpOpen(L"winetest", WINHTTP_ACCESS_TYPE_NO_PROXY, NULL, NULL, 0);
    ok(ses != NULL, "failed to open session %lu\n", GetLastError());

    con = WinHttpConnect(ses, L"localhost", port, 0);
    ok(con != NULL, "failed to open a connection %lu\n", GetLastError());

    req = WinHttpOpenRequest(con, L"HEAD", L"/head", NULL, NULL, NULL, 0);
    ok(req != NULL, "failed to open a request %lu\n", GetLastError());

    ret = WinHttpSendRequest(req, NULL, 0, NULL, 0, 0, 0);
    ok(ret, "failed to send request %lu\n", GetLastError());

    ret = WinHttpReceiveResponse(req, NULL);
    ok(ret, "failed to receive response %lu\n", GetLastError());

    status = 0xdeadbeef;
    size = sizeof(status);
    ret = WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                              NULL, &status, &size, NULL);
    ok(ret, "failed to get status code %lu\n", GetLastError());
    ok(status == HTTP_STATUS_OK, "got %lu\n", status);

    len = 0xdeadbeef;
    size = sizeof(len);
    ret = WinHttpQueryHeaders(req, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                              NULL, &len, &size, 0);
    ok(ret, "failed to get content-length header %lu\n", GetLastError());
    ok(len == HTTP_STATUS_CONTINUE, "got %lu\n", len);

    count = 0xdeadbeef;
    ret = WinHttpQueryDataAvailable(req, &count);
    ok(ret, "failed to query data available %lu\n", GetLastError());
    ok(!count, "got %lu\n", count);

    len = sizeof(buf);
    count = 0xdeadbeef;
    ret = WinHttpReadData(req, buf, len, &count);
    ok(ret, "failed to read data %lu\n", GetLastError());
    ok(!count, "got %lu\n", count);

    count = 0xdeadbeef;
    ret = WinHttpQueryDataAvailable(req, &count);
    ok(ret, "failed to query data available %lu\n", GetLastError());
    ok(!count, "got %lu\n", count);

    WinHttpCloseHandle(req);
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
}

static void test_redirect(int port, const WCHAR *path, const WCHAR *target)
{
    HINTERNET ses, con, req;
    char buf[128];
    DWORD size, len, count, status;
    WCHAR url[128], expected[128];
    BOOL ret;

    ses = WinHttpOpen(L"winetest", WINHTTP_ACCESS_TYPE_NO_PROXY, NULL, NULL, 0);
    ok(ses != NULL, "failed to open session %lu\n", GetLastError());

    con = WinHttpConnect(ses, L"localhost", port, 0);
    ok(con != NULL, "failed to open a connection %lu\n", GetLastError());

    req = WinHttpOpenRequest(con, L"POST", path, NULL, NULL, NULL, 0);
    ok(req != NULL, "failed to open a request %lu\n", GetLastError());

    url[0] = 0;
    size = sizeof(url);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_URL, url, &size);
    ok(ret, "got %lu\n", GetLastError());
    swprintf(expected, ARRAY_SIZE(expected), L"http://localhost:%u%s", port, path);
    ok(!wcscmp(url, expected), "expected %s got %s\n", wine_dbgstr_w(expected), wine_dbgstr_w(url));

    ret = WinHttpSendRequest(req, NULL, 0, (void *)"data", sizeof("data"), sizeof("data"), 0);
    ok(ret, "failed to send request %lu\n", GetLastError());

    url[0] = 0;
    size = sizeof(url);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_URL, url, &size);
    ok(ret, "got %lu\n", GetLastError());
    ok(!wcscmp(url, expected), "expected %s got %s\n", wine_dbgstr_w(expected), wine_dbgstr_w(url));

    /* Exact buffer size match. */
    url[0] = 0;
    size = (lstrlenW(expected) + 1) * sizeof(WCHAR);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_URL, url, &size);
    ok(ret, "got %lu\n", GetLastError());
    ok(!wcscmp(url, expected), "expected %s got %s\n", wine_dbgstr_w(expected), wine_dbgstr_w(url));

    ret = WinHttpReceiveResponse(req, NULL);
    ok(ret, "failed to receive response %lu\n", GetLastError());

    url[0] = 0;
    size = sizeof(url);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_URL, url, &size);
    ok(ret, "got %lu\n", GetLastError());
    swprintf(expected, ARRAY_SIZE(expected), L"http://localhost:%u/%s", port, target);
    ok(!wcscmp(url, expected) ||
       broken(!!wcsstr(url, L"redirect-perm")), /* < Win10 */
       "expected %s got %s\n", wine_dbgstr_w(expected), wine_dbgstr_w(url));
    if (wcsstr(url, L"redirect-perm"))
        goto cleanup;

    status = 0xdeadbeef;
    size = sizeof(status);
    ret = WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                              NULL, &status, &size, NULL);
    ok(ret, "failed to get status code %lu\n", GetLastError());
    ok(status == HTTP_STATUS_OK, "got %lu\n", status);

    count = 0;
    ret = WinHttpQueryDataAvailable(req, &count);
    ok(ret, "failed to query data available %lu\n", GetLastError());
    ok(count == 128, "got %lu\n", count);

    len = sizeof(buf);
    count = 0;
    ret = WinHttpReadData(req, buf, len, &count);
    ok(ret, "failed to read data %lu\n", GetLastError());
    ok(count == 128, "got %lu\n", count);

cleanup:
    WinHttpCloseHandle(req);
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
}

static void test_websocket(int port)
{
    HINTERNET session, connection, request, socket, socket2;
    DWORD size, len, count, status, index, error, value;
    DWORD_PTR ctx;
    WINHTTP_WEB_SOCKET_BUFFER_TYPE type;
    BOOL broken_buffer_sizes = FALSE;
    WCHAR header[32];
    char buf[128], *large_buf;
    USHORT close_status;
    BOOL ret;

    if (!pWinHttpWebSocketCompleteUpgrade)
    {
        win_skip("WinHttpWebSocketCompleteUpgrade not supported\n");
        return;
    }

    session = WinHttpOpen(L"winetest", 0, NULL, NULL, 0);
    ok(session != NULL, "got %lu\n", GetLastError());

    connection = WinHttpConnect(session, L"localhost", port, 0);
    ok(connection != NULL, "got %lu\n", GetLastError());

    request = WinHttpOpenRequest(connection, L"GET", L"/websocket", NULL, NULL, NULL, 0);
    ok(request != NULL, "got %lu\n", GetLastError());

    ret = WinHttpSetOption(request, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, NULL, 0);
    ok(ret, "got %lu\n", GetLastError());

    size = sizeof(header);
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_UPGRADE, NULL, &header, &size, NULL);
    error = GetLastError();
    ok(!ret, "success\n");
    ok(error == ERROR_WINHTTP_INCORRECT_HANDLE_STATE, "got %lu\n", error);

    size = sizeof(header);
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CONNECTION, NULL, &header, &size, NULL);
    error = GetLastError();
    ok(!ret, "success\n");
    ok(error == ERROR_WINHTTP_INCORRECT_HANDLE_STATE, "got %lu\n", error);

    index = 0;
    size = sizeof(buf);
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                              L"Sec-WebSocket-Key", buf, &size, &index);
    error = GetLastError();
    ok(!ret, "success\n");
    ok(error == ERROR_WINHTTP_HEADER_NOT_FOUND, "got %lu\n", error);

    index = 0;
    size = sizeof(buf);
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                              L"Sec-WebSocket-Version", buf, &size, &index);
    error = GetLastError();
    ok(!ret, "success\n");
    ok(error == ERROR_WINHTTP_HEADER_NOT_FOUND, "got %lu\n", error);

    ret = WinHttpSendRequest(request, NULL, 0, NULL, 0, 0, 0);
    ok(ret, "got %lu\n", GetLastError());

    size = sizeof(header);
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_UPGRADE, NULL, &header, &size, NULL);
    error = GetLastError();
    ok(!ret, "success\n");
    ok(error == ERROR_WINHTTP_INCORRECT_HANDLE_STATE, "got %lu\n", error);

    size = sizeof(header);
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CONNECTION, NULL, &header, &size, NULL);
    error = GetLastError();
    ok(!ret, "success\n");
    ok(error == ERROR_WINHTTP_INCORRECT_HANDLE_STATE, "got %lu\n", error);

    index = 0;
    buf[0] = 0;
    size = sizeof(buf);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                              L"Sec-WebSocket-Key", buf, &size, &index);
    ok(ret, "got %lu\n", GetLastError());

    index = 0;
    buf[0] = 0;
    size = sizeof(buf);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                              L"Sec-WebSocket-Version", buf, &size, &index);
    ok(ret, "got %lu\n", GetLastError());

    ret = WinHttpReceiveResponse(request, NULL);
    ok(ret, "got %lu\n", GetLastError());

    count = 0xdeadbeef;
    ret = WinHttpQueryDataAvailable(request, &count);
    ok(ret, "got %lu\n", GetLastError());
    ok(!count, "got %lu\n", count);

    header[0] = 0;
    size = sizeof(header);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_UPGRADE, NULL, &header, &size, NULL);
    ok(ret, "got %lu\n", GetLastError());
    ok(!wcscmp( header, L"websocket" ), "got %s\n", wine_dbgstr_w(header));

    header[0] = 0;
    size = sizeof(header);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CONNECTION, NULL, &header, &size, NULL);
    ok(ret, "got %lu\n", GetLastError());
    ok(!wcscmp( header, L"Upgrade" ), "got %s\n", wine_dbgstr_w(header));

    status = 0xdeadbeef;
    size = sizeof(status);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &status,
                              &size, NULL);
    ok(ret, "got %lu\n", GetLastError());
    ok(status == HTTP_STATUS_SWITCH_PROTOCOLS, "got %lu\n", status);

    len = 0xdeadbeef;
    size = sizeof(len);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, NULL, &len,
                              &size, NULL);
    ok(!ret, "success\n");

    index = 0;
    size = sizeof(buf);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM, L"Sec-WebSocket-Accept", buf, &size, &index);
    ok(ret, "got %lu\n", GetLastError());

    socket = pWinHttpWebSocketCompleteUpgrade(request, 0);
    ok(socket != NULL, "got %lu\n", GetLastError());

    size = sizeof(header);
    ret = WinHttpQueryHeaders(socket, WINHTTP_QUERY_UPGRADE, NULL, &header, &size, NULL);
    error = GetLastError();
    ok(!ret, "success\n");
    ok(error == ERROR_WINHTTP_INCORRECT_HANDLE_TYPE, "got %lu\n", error);

    header[0] = 0;
    size = sizeof(header);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_UPGRADE, NULL, &header, &size, NULL);
    ok(ret, "got %lu\n", GetLastError());
    ok(!wcscmp( header, L"websocket" ), "got %s\n", wine_dbgstr_w(header));

    header[0] = 0;
    size = sizeof(header);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CONNECTION, NULL, &header, &size, NULL);
    ok(ret, "got %lu\n", GetLastError());
    ok(!wcscmp( header, L"Upgrade" ), "got %s\n", wine_dbgstr_w(header));

    index = 0;
    size = sizeof(buf);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM, L"Sec-WebSocket-Accept", buf, &size, &index);
    ok(ret, "got %lu\n", GetLastError());

    /* sending request again generates new key */
    ret = WinHttpSendRequest(request, NULL, 0, NULL, 0, 0, 0);
    ok(ret, "got %lu\n", GetLastError());

    /* and creates a new websocket */
    socket2 = pWinHttpWebSocketCompleteUpgrade(request, 0);
    ok(socket2 != NULL, "got %lu\n", GetLastError());
    ok(socket2 != socket, "got same socket\n");

    WinHttpCloseHandle(connection);
    /* request handle is still valid */
    size = sizeof(ctx);
    ret = WinHttpQueryOption(request, WINHTTP_OPTION_CONTEXT_VALUE, &ctx, &size);
    ok(ret, "got %lu\n", GetLastError());

    ret = WinHttpCloseHandle(socket2);
    ok(ret, "got %lu\n", GetLastError());

    ret = WinHttpCloseHandle(socket);
    ok(ret, "got %lu\n", GetLastError());

    ret = WinHttpQueryOption(request, WINHTTP_OPTION_CONTEXT_VALUE, &ctx, &size);
    ok(ret, "got %lu\n", GetLastError());

    ret = WinHttpCloseHandle(session);
    ok(ret, "got %lu\n", GetLastError());

    ret = WinHttpQueryOption(request, WINHTTP_OPTION_CONTEXT_VALUE, &ctx, &size);
    ok(ret, "got %lu\n", GetLastError());

    ret = WinHttpCloseHandle(request);
    ok(ret, "got %lu\n", GetLastError());

    session = WinHttpOpen(L"winetest", 0, NULL, NULL, 0);
    ok(session != NULL, "got %lu\n", GetLastError());

    connection = WinHttpConnect(session, L"ws.ifelse.io", 0, 0);
    ok(connection != NULL, "got %lu\n", GetLastError());

    size = 0xdeadbeef;
    ret = WinHttpQueryOption(session, WINHTTP_OPTION_WEB_SOCKET_RECEIVE_BUFFER_SIZE, &value, &size);
    ok(ret, "got %lu\n", GetLastError());
    ok(size == sizeof(DWORD), "got %lu.\n", size);
    ok(value == 32768, "got %lu.\n", value);

    value = 65535;
    ret = WinHttpSetOption(session, WINHTTP_OPTION_WEB_SOCKET_RECEIVE_BUFFER_SIZE, &value, sizeof(DWORD));
    ok(ret, "got %lu\n", GetLastError());

    size = 0xdeadbeef;
    ret = WinHttpQueryOption(session, WINHTTP_OPTION_WEB_SOCKET_SEND_BUFFER_SIZE, &value, &size);
    ok(ret, "got %lu\n", GetLastError());
    ok(size == sizeof(DWORD), "got %lu.\n", size);
    ok(value == 32768, "got %lu.\n", value);

    value = 15;
    ret = WinHttpSetOption(session, WINHTTP_OPTION_WEB_SOCKET_SEND_BUFFER_SIZE, &value, sizeof(DWORD));
    ok(ret, "got %lu\n", GetLastError());

    request = WinHttpOpenRequest(connection, L"GET", L"/", NULL, NULL, NULL, 0);
    ok(request != NULL, "got %lu\n", GetLastError());

    size = 0xdeadbeef;
    ret = WinHttpQueryOption(session, WINHTTP_OPTION_WEB_SOCKET_RECEIVE_BUFFER_SIZE, &value, &size);
    ok(ret, "got %lu\n", GetLastError());
    ok(size == sizeof(DWORD), "got %lu.\n", size);
    ok(value == 65535 || broken( value == WINHTTP_OPTION_WEB_SOCKET_RECEIVE_BUFFER_SIZE ) /* Win8 */, "got %lu.\n", value);
    if (value == WINHTTP_OPTION_WEB_SOCKET_RECEIVE_BUFFER_SIZE)
        broken_buffer_sizes = TRUE;

    size = 0xdeadbeef;
    ret = WinHttpQueryOption(request, WINHTTP_OPTION_WEB_SOCKET_RECEIVE_BUFFER_SIZE, &value, &size);
    ok(ret, "got %lu\n", GetLastError());
    ok(size == sizeof(DWORD), "got %lu.\n", size);
    ok(value == 65535 || broken( value == WINHTTP_OPTION_WEB_SOCKET_RECEIVE_BUFFER_SIZE ), "got %lu.\n", value);

    value = 1048576;
    ret = WinHttpSetOption(request, WINHTTP_OPTION_WEB_SOCKET_RECEIVE_BUFFER_SIZE, &value, sizeof(DWORD));
    ok(ret, "got %lu\n", GetLastError());

    size = 0xdeadbeef;
    ret = WinHttpQueryOption(session, WINHTTP_OPTION_WEB_SOCKET_RECEIVE_BUFFER_SIZE, &value, &size);
    ok(ret, "got %lu\n", GetLastError());
    ok(size == sizeof(DWORD), "got %lu.\n", size);
    ok(value == 65535 || broken( value == WINHTTP_OPTION_WEB_SOCKET_RECEIVE_BUFFER_SIZE ) /* Win8 */, "got %lu.\n", value);

    size = 0xdeadbeef;
    ret = WinHttpQueryOption(request, WINHTTP_OPTION_WEB_SOCKET_RECEIVE_BUFFER_SIZE, &value, &size);
    ok(ret, "got %lu\n", GetLastError());
    ok(size == sizeof(DWORD), "got %lu.\n", size);
    ok(value == 1048576, "got %lu.\n", value);

    size = 0xdeadbeef;
    ret = WinHttpQueryOption(connection, WINHTTP_OPTION_WEB_SOCKET_RECEIVE_BUFFER_SIZE, &value, &size);
    ok(!ret, "got %d\n", ret);
    todo_wine ok(GetLastError() == ERROR_WINHTTP_INCORRECT_HANDLE_TYPE, "got %lu\n", GetLastError());

    size = 0xdeadbeef;
    ret = WinHttpQueryOption(request, WINHTTP_OPTION_WEB_SOCKET_SEND_BUFFER_SIZE, &value, &size);
    ok(ret, "got %lu\n", GetLastError());
    ok(size == sizeof(DWORD), "got %lu.\n", size);
    ok(value == 15 || broken( value == WINHTTP_OPTION_WEB_SOCKET_SEND_BUFFER_SIZE ) /* Win8 */, "got %lu.\n", value);

    size = sizeof(value);
    ret = WinHttpQueryOption(session, WINHTTP_OPTION_WEB_SOCKET_KEEPALIVE_INTERVAL, &value, &size);
    ok(!ret, "got %d\n", ret);
    ok(GetLastError() == ERROR_INVALID_PARAMETER, "got %lu\n", GetLastError());

    size = sizeof(value);
    ret = WinHttpQueryOption(request, WINHTTP_OPTION_WEB_SOCKET_KEEPALIVE_INTERVAL, &value, &size);
    ok(!ret, "got %d\n", ret);
    ok(GetLastError() == ERROR_INVALID_PARAMETER, "got %lu\n", GetLastError());

    value = 20000;
    ret = WinHttpSetOption(request, WINHTTP_OPTION_WEB_SOCKET_KEEPALIVE_INTERVAL, &value, sizeof(DWORD));
    ok(!ret, "got %d\n", ret);
    todo_wine ok(GetLastError() == ERROR_WINHTTP_INCORRECT_HANDLE_TYPE, "got %lu\n", GetLastError());

    ret = WinHttpSetOption(request, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, NULL, 0);
    ok(ret, "got %lu\n", GetLastError());

    if (!broken_buffer_sizes)
    {
        /* Fails because we have a too small send buffer size set, but is different on Win8. */
        ret = WinHttpSendRequest(request, NULL, 0, NULL, 0, 0, 0);
        ok(!ret, "got %d\n", ret);
        ok(GetLastError() == ERROR_NOT_ENOUGH_MEMORY, "got %lu\n", GetLastError());
    }

    value = 16;
    ret = WinHttpSetOption(request, WINHTTP_OPTION_WEB_SOCKET_SEND_BUFFER_SIZE, &value, sizeof(DWORD));
    ok(ret, "got %lu\n", GetLastError());

    ret = WinHttpSendRequest(request, NULL, 0, NULL, 0, 0, 0);
    ok(ret, "got %lu\n", GetLastError());

    value = 15;
    ret = WinHttpSetOption(request, WINHTTP_OPTION_WEB_SOCKET_SEND_BUFFER_SIZE, &value, sizeof(DWORD));
    ok(ret, "got %lu\n", GetLastError());

    ret = WinHttpReceiveResponse(request, NULL);
    ok(ret, "got %lu\n", GetLastError());

    status = 0xdeadbeef;
    size = sizeof(status);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &status,
                              &size, NULL);
    ok(ret, "got %lu\n", GetLastError());
    ok(status == HTTP_STATUS_SWITCH_PROTOCOLS, "got %lu\n", status);

    socket = pWinHttpWebSocketCompleteUpgrade(request, 0);
    ok(socket != NULL, "got %lu\n", GetLastError());

    size = sizeof(value);
    ret = WinHttpQueryOption(socket, WINHTTP_OPTION_WEB_SOCKET_RECEIVE_BUFFER_SIZE, &value, &size);
    ok(!ret, "got %d\n", ret);
    todo_wine ok(GetLastError() == ERROR_WINHTTP_INCORRECT_HANDLE_TYPE, "got %lu\n", GetLastError());

    value = 65535;
    ret = WinHttpSetOption(socket, WINHTTP_OPTION_WEB_SOCKET_RECEIVE_BUFFER_SIZE, &value, sizeof(DWORD));
    ok(!ret, "got %d\n", ret);
    todo_wine ok(GetLastError() == ERROR_WINHTTP_INCORRECT_HANDLE_TYPE, "got %lu\n", GetLastError());

    ret = WinHttpSetOption(socket, WINHTTP_OPTION_WEB_SOCKET_SEND_BUFFER_SIZE, &value, sizeof(DWORD));
    ok(!ret, "got %d\n", ret);
    todo_wine ok(GetLastError() == ERROR_WINHTTP_INCORRECT_HANDLE_TYPE, "got %lu\n", GetLastError());

    value = 20000;
    ret = WinHttpSetOption(socket, WINHTTP_OPTION_WEB_SOCKET_KEEPALIVE_INTERVAL, &value, 2);
    ok(!ret, "got %d\n", ret);
    ok(GetLastError() == ERROR_INVALID_PARAMETER, "got %lu\n", GetLastError());

    value = 20000;
    ret = WinHttpSetOption(socket, WINHTTP_OPTION_WEB_SOCKET_KEEPALIVE_INTERVAL, &value, sizeof(DWORD) * 2);
    ok(!ret, "got %d\n", ret);
    ok(GetLastError() == ERROR_INVALID_PARAMETER, "got %lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    value = 20000;
    ret = WinHttpSetOption(socket, WINHTTP_OPTION_WEB_SOCKET_KEEPALIVE_INTERVAL, &value, sizeof(DWORD));
    ok(ret, "got %lu\n", GetLastError());
    ok(!GetLastError(), "got %lu\n", GetLastError());

    size = sizeof(value);
    ret = WinHttpQueryOption(socket, WINHTTP_OPTION_WEB_SOCKET_KEEPALIVE_INTERVAL, &value, &size);
    ok(!ret, "got %d\n", ret);
    ok(GetLastError() == ERROR_INVALID_PARAMETER, "got %lu\n", GetLastError());

    size = 0;
    ret = WinHttpQueryOption(socket, WINHTTP_OPTION_WEB_SOCKET_KEEPALIVE_INTERVAL, NULL, &size);
    ok(!ret, "got %d\n", ret);
    ok(GetLastError() == ERROR_INVALID_PARAMETER, "got %lu\n", GetLastError());

    value = 10000;
    ret = WinHttpSetOption(socket, WINHTTP_OPTION_WEB_SOCKET_KEEPALIVE_INTERVAL, &value, sizeof(DWORD));
    ok(!ret, "got %d\n", ret);
    ok(GetLastError() == ERROR_INVALID_PARAMETER, "got %lu\n", GetLastError());

    value = 10000;
    ret = WinHttpSetOption(socket, WINHTTP_OPTION_WEB_SOCKET_KEEPALIVE_INTERVAL, &value, 2);
    ok(!ret, "got %d\n", ret);
    ok(GetLastError() == ERROR_INVALID_PARAMETER, "got %lu\n", GetLastError());

    buf[0] = 0;
    count = 0;
    type = 0xdeadbeef;
    error = pWinHttpWebSocketReceive(socket, buf, sizeof(buf), &count, &type);
    ok(!error, "got %lu\n", error);
    ok(buf[0] == 'R', "got %c\n", buf[0]);
    ok(count, "got zero count\n");
    ok(type == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE, "got %u\n", type);

    error = pWinHttpWebSocketSend(socket, WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE, NULL, 1);
    ok(error == ERROR_INVALID_PARAMETER, "got %lu\n", error);

    large_buf = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(buf) * 2);
    memcpy(large_buf, "hello", sizeof("hello"));
    memcpy(large_buf + sizeof(buf), "world", sizeof("world"));
    error = pWinHttpWebSocketSend(socket, WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE, large_buf, sizeof(buf) * 2);
    ok(!error, "got %lu\n", error);
    HeapFree(GetProcessHeap(), 0, large_buf);

    error = pWinHttpWebSocketReceive(socket, NULL, 0, NULL, NULL);
    ok(error == ERROR_INVALID_PARAMETER, "got %lu\n", error);

    error = pWinHttpWebSocketReceive(socket, buf, 0, NULL, NULL);
    ok(error == ERROR_INVALID_PARAMETER, "got %lu\n", error);

    error = pWinHttpWebSocketReceive(socket, NULL, 1, NULL, NULL);
    ok(error == ERROR_INVALID_PARAMETER, "got %lu\n", error);

    buf[0] = 0;
    count = 0;
    type = 0xdeadbeef;
    error = pWinHttpWebSocketReceive(socket, buf, sizeof(buf), &count, &type);
    ok(!error, "got %lu\n", error);
    ok(buf[0] == 'h', "got %c\n", buf[0]);
    ok(count == sizeof(buf), "got %lu\n", count);
    ok(type == WINHTTP_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE, "got %u\n", type);

    buf[0] = 0;
    count = 0;
    type = 0xdeadbeef;
    error = pWinHttpWebSocketReceive(socket, buf, sizeof(buf), &count, &type);
    ok(!error, "got %lu\n", error);
    ok(buf[0] == 'w', "got %c\n", buf[0]);
    ok(count == sizeof(buf), "got %lu\n", count);
    ok(type == WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE, "got %u\n", type);

    error = pWinHttpWebSocketShutdown(socket, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, NULL, 1);
    ok(error == ERROR_INVALID_PARAMETER, "got %lu\n", error);

    error = pWinHttpWebSocketShutdown(socket, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, buf, sizeof(buf));
    ok(error == ERROR_INVALID_PARAMETER, "got %lu\n", error);

    error = pWinHttpWebSocketShutdown(socket, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, (void *)"success",
                                      sizeof("success"));
    ok(!error, "got %lu\n", error);

    error = pWinHttpWebSocketClose(socket, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, NULL, 1);
    ok(error == ERROR_INVALID_PARAMETER, "got %lu\n", error);

    error = pWinHttpWebSocketClose(socket, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, buf, sizeof(buf));
    ok(error == ERROR_INVALID_PARAMETER, "got %lu\n", error);

    error = pWinHttpWebSocketClose(socket, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, (void *)"success2",
                                   sizeof("success2"));
    ok(!error, "got %lu\n", error);

    error = pWinHttpWebSocketQueryCloseStatus(socket, NULL, NULL, 0, NULL);
    ok(error == ERROR_INVALID_PARAMETER, "got %lu\n", error);

    error = pWinHttpWebSocketQueryCloseStatus(socket, &close_status, NULL, 0, NULL);
    ok(error == ERROR_INVALID_PARAMETER, "got %lu\n", error);

    error = pWinHttpWebSocketQueryCloseStatus(socket, &close_status, buf, 0, NULL);
    ok(error == ERROR_INVALID_PARAMETER, "got %lu\n", error);

    error = pWinHttpWebSocketQueryCloseStatus(socket, &close_status, buf, sizeof(buf), NULL);
    ok(error == ERROR_INVALID_PARAMETER, "got %lu\n", error);

    error = pWinHttpWebSocketQueryCloseStatus(socket, NULL, NULL, 0, &len);
    ok(error == ERROR_INVALID_PARAMETER, "got %lu\n", error);

    len = 0xdeadbeef;
    error = pWinHttpWebSocketQueryCloseStatus(socket, &close_status, NULL, 0, &len);
    ok(!error, "got %lu\n", error);
    ok(!len, "got %lu\n", len);

    error = pWinHttpWebSocketQueryCloseStatus(socket, &close_status, NULL, 1, &len);
    ok(error == ERROR_INVALID_PARAMETER, "got %lu\n", error);

    close_status = 0xdead;
    len = 0xdeadbeef;
    memset(buf, 0, sizeof(buf));
    error = pWinHttpWebSocketQueryCloseStatus(socket, &close_status, buf, sizeof(buf), &len);
    ok(!error, "got %lu\n", error);
    ok(close_status == 1000, "got %d\n", close_status);
    ok(!len, "got %lu\n", len);

    WinHttpCloseHandle(socket);
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
}

static void test_not_modified(int port)
{
    BOOL ret;
    HINTERNET session, request, connection;
    DWORD index, len, status, size, start = GetTickCount();
    SYSTEMTIME st;
    WCHAR today[(sizeof(L"If-Modified-Since: ") + WINHTTP_TIME_FORMAT_BUFSIZE)/sizeof(WCHAR) + 3], buffer[32];

    memcpy(today, L"If-Modified-Since: ", sizeof(L"If-Modified-Since: "));
    GetSystemTime(&st);
    WinHttpTimeFromSystemTime(&st, &today[ARRAY_SIZE(L"If-Modified-Since: ") - 1]);

    session = WinHttpOpen(L"winetest", WINHTTP_ACCESS_TYPE_NO_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    ok(session != NULL, "WinHttpOpen failed: %lu\n", GetLastError());

    connection = WinHttpConnect(session, L"localhost", port, 0);
    ok(connection != NULL, "WinHttpConnect failed: %lu\n", GetLastError());

    request = WinHttpOpenRequest(connection, NULL, L"/not_modified", NULL, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_BYPASS_PROXY_CACHE);
    ok(request != NULL, "WinHttpOpenrequest failed: %lu\n", GetLastError());

    ret = WinHttpSendRequest(request, today, 0, NULL, 0, 0, 0);
    ok(ret, "WinHttpSendRequest failed: %lu\n", GetLastError());

    ret = WinHttpReceiveResponse(request, NULL);
    ok(ret, "WinHttpReceiveResponse failed: %lu\n", GetLastError());

    index = 0;
    len = sizeof(buffer);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                              L"If-Modified-Since", buffer, &len, &index);
    ok(ret, "failed to get header %lu\n", GetLastError());

    status = 0xdeadbeef;
    size = sizeof(status);
    ret = WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
                              NULL, &status, &size, NULL);
    ok(ret, "WinHttpQueryHeaders failed: %lu\n", GetLastError());
    ok(status == HTTP_STATUS_NOT_MODIFIED, "got %lu\n", status);

    size = 0xdeadbeef;
    ret = WinHttpQueryDataAvailable(request, &size);
    ok(ret, "WinHttpQueryDataAvailable failed: %lu\n", GetLastError());
    ok(!size, "got %lu\n", size);

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    start = GetTickCount() - start;
    ok(start <= 2000, "Expected less than 2 seconds for the test, got %lu ms\n", start);
}

static void test_bad_header( int port )
{
    static const WCHAR expected_headers[] =
    {
        L"HTTP/1.1 200 OK\r\n"
        L"Server: winetest\r\n"
        L"SpaceAfterHdr: bad\r\n"
        L"OkHdr: ok\r\n"
        L"\r\n"
    };

    HINTERNET ses, con, req;
    WCHAR buffer[512];
    DWORD index, len;
    unsigned int i;
    BOOL ret;

    static const WCHAR bad_headers[] =
        L"Content-Type: text/html\n\r"
        L"Test1: Value1\n"
        L"Test2: Value2\n\n\n"
        L"Test3: Value3\r\r\r"
        L"Test4: Value4\r\n\r\n"
        L"Cookie: 111";

    static const struct
    {
        const WCHAR *header;
        const WCHAR *value;
    }
    header_tests[] =
    {
        {L"Content-Type", L"text/html"},
        {L"Test1", L"Value1"},
        {L"Test2", L"Value2"},
        {L"Test3", L"Value3"},
        {L"Test4", L"Value4"},
        {L"Cookie", L"111"},
    };

    ses = WinHttpOpen( L"winetest", WINHTTP_ACCESS_TYPE_NO_PROXY, NULL, NULL, 0 );
    ok( ses != NULL, "failed to open session %lu\n", GetLastError() );

    con = WinHttpConnect( ses, L"localhost", port, 0 );
    ok( con != NULL, "failed to open a connection %lu\n", GetLastError() );

    req = WinHttpOpenRequest( con, L"POST", L"/bad_headers", NULL, NULL, NULL, 0 );
    ok( req != NULL, "failed to open a request %lu\n", GetLastError() );

    ret = WinHttpAddRequestHeaders( req, bad_headers, ~0u, WINHTTP_ADDREQ_FLAG_ADD );
    ok( ret, "failed to add header %lu\n", GetLastError() );

    for (i = 0; i < ARRAY_SIZE(header_tests); ++i)
    {
        index = 0;
        buffer[0] = 0;
        len = sizeof(buffer);
        ret = WinHttpQueryHeaders( req, WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
                                   header_tests[i].header, buffer, &len, &index );
        ok( ret, "header %s: failed to query headers %lu\n", debugstr_w(header_tests[i].header), GetLastError() );
        ok( !wcscmp( buffer, header_tests[i].value ), "header %s: got %s\n",
            debugstr_w(header_tests[i].header), debugstr_w(buffer) );
        ok( index == 1, "header %s: index = %lu\n", debugstr_w(header_tests[i].header), index );
    }

    ret = WinHttpSendRequest( req, L"Test5: Value5\rTest6: Value6", ~0u, NULL, 0, 0, 0 );
    ok( ret, "failed to send request %lu\n", GetLastError() );

    ret = WinHttpReceiveResponse( req, NULL );
    ok( ret, "failed to receive response %lu\n", GetLastError() );

    len = sizeof(buffer);
    ret = WinHttpQueryHeaders( req, WINHTTP_QUERY_CUSTOM, L"OkHdr", buffer, &len, WINHTTP_NO_HEADER_INDEX );
    ok( ret, "got error %lu.\n", GetLastError() );

    len = sizeof(buffer);
    ret = WinHttpQueryHeaders( req, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, buffer, &len, WINHTTP_NO_HEADER_INDEX );
    ok( ret, "got error %lu.\n", GetLastError() );
    ok( !wcscmp( buffer, expected_headers ), "got %s.\n", debugstr_w(buffer) );

    len = sizeof(buffer);
    ret = WinHttpQueryHeaders( req, WINHTTP_QUERY_CUSTOM, L"SpaceAfterHdr", buffer, &len, WINHTTP_NO_HEADER_INDEX );
    ok( ret, "got error %lu.\n", GetLastError() );
    ok( !wcscmp( buffer, L"bad" ), "got %s.\n", debugstr_w(buffer) );

    WinHttpCloseHandle( req );
    WinHttpCloseHandle( con );
    WinHttpCloseHandle( ses );
}

static void test_multiple_reads(int port)
{
    HINTERNET ses, con, req;
    DWORD total_len = 0;
    BOOL ret;

    ses = WinHttpOpen(L"winetest", WINHTTP_ACCESS_TYPE_NO_PROXY, NULL, NULL, 0);
    ok(ses != NULL, "failed to open session %lu\n", GetLastError());

    con = WinHttpConnect(ses, L"localhost", port, 0);
    ok(con != NULL, "failed to open a connection %lu\n", GetLastError());

    req = WinHttpOpenRequest(con, NULL, L"big", NULL, NULL, NULL, 0);
    ok(req != NULL, "failed to open a request %lu\n", GetLastError());

    ret = WinHttpSendRequest(req, NULL, 0, NULL, 0, 0, 0);
    ok(ret, "failed to send request %lu\n", GetLastError());

    ret = WinHttpReceiveResponse(req, NULL);
    ok(ret == TRUE, "expected success\n");

    for (;;)
    {
        DWORD len = 0xdeadbeef;
        ret = WinHttpQueryDataAvailable( req, &len );
        ok( ret, "WinHttpQueryDataAvailable failed with error %lu\n", GetLastError() );
        if (ret) ok( len != 0xdeadbeef, "WinHttpQueryDataAvailable return wrong length\n" );
        if (len)
        {
            DWORD bytes_read;
            char *buf = HeapAlloc( GetProcessHeap(), 0, len + 1 );

            ret = WinHttpReadData( req, buf, len, &bytes_read );
            ok(ret, "WinHttpReadData failed: %lu\n", GetLastError());
            ok( len == bytes_read, "only got %lu of %lu available\n", bytes_read, len );

            HeapFree( GetProcessHeap(), 0, buf );
            if (!bytes_read) break;
            total_len += bytes_read;
        }
        if (!len) break;
    }
    ok(total_len == BIG_BUFFER_LEN, "got wrong length: %lu\n", total_len);

    WinHttpCloseHandle(req);
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
}

static void test_cookies( int port )
{
    HINTERNET ses, con, req;
    DWORD status, size;
    BOOL ret;

    ses = WinHttpOpen( L"winetest", WINHTTP_ACCESS_TYPE_NO_PROXY, NULL, NULL, 0 );
    ok( ses != NULL, "failed to open session %lu\n", GetLastError() );

    con = WinHttpConnect( ses, L"localhost", port, 0 );
    ok( con != NULL, "failed to open a connection %lu\n", GetLastError() );

    req = WinHttpOpenRequest( con, NULL, L"/cookie", NULL, NULL, NULL, 0 );
    ok( req != NULL, "failed to open a request %lu\n", GetLastError() );

    ret = WinHttpSendRequest( req, NULL, 0, NULL, 0, 0, 0 );
    ok( ret, "failed to send request %lu\n", GetLastError() );

    ret = WinHttpReceiveResponse( req, NULL );
    ok( ret, "failed to receive response %lu\n", GetLastError() );

    status = 0xdeadbeef;
    size = sizeof(status);
    ret = WinHttpQueryHeaders( req, WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &size, NULL );
    ok( ret, "failed to query status code %lu\n", GetLastError() );
    ok( status == HTTP_STATUS_OK, "request failed unexpectedly %lu\n", status );

    WinHttpCloseHandle( req );

    req = WinHttpOpenRequest( con, NULL, L"/cookie2", NULL, NULL, NULL, 0 );
    ok( req != NULL, "failed to open a request %lu\n", GetLastError() );

    ret = WinHttpSendRequest( req, NULL, 0, NULL, 0, 0, 0 );
    ok( ret, "failed to send request %lu\n", GetLastError() );

    ret = WinHttpReceiveResponse( req, NULL );
    ok( ret, "failed to receive response %lu\n", GetLastError() );

    status = 0xdeadbeef;
    size = sizeof(status);
    ret = WinHttpQueryHeaders( req, WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &size, NULL );
    ok( ret, "failed to query status code %lu\n", GetLastError() );
    ok( status == HTTP_STATUS_OK, "request failed unexpectedly %lu\n", status );

    WinHttpCloseHandle( req );
    WinHttpCloseHandle( con );

    con = WinHttpConnect( ses, L"localhost", port, 0 );
    ok( con != NULL, "failed to open a connection %lu\n", GetLastError() );

    req = WinHttpOpenRequest( con, NULL, L"/cookie2", NULL, NULL, NULL, 0 );
    ok( req != NULL, "failed to open a request %lu\n", GetLastError() );

    ret = WinHttpSendRequest( req, NULL, 0, NULL, 0, 0, 0 );
    ok( ret, "failed to send request %lu\n", GetLastError() );

    ret = WinHttpReceiveResponse( req, NULL );
    ok( ret, "failed to receive response %lu\n", GetLastError() );

    status = 0xdeadbeef;
    size = sizeof(status);
    ret = WinHttpQueryHeaders( req, WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &size, NULL );
    ok( ret, "failed to query status code %lu\n", GetLastError() );
    ok( status == HTTP_STATUS_OK, "request failed unexpectedly %lu\n", status );

    WinHttpCloseHandle( req );

    req = WinHttpOpenRequest( con, NULL, L"/cookie3", NULL, NULL, NULL, 0 );
    ok( req != NULL, "failed to open a request %lu\n", GetLastError() );
    ret = WinHttpSendRequest( req, L"Cookie: name=value2\r\n", ~0u, NULL, 0, 0, 0 );
    ok( ret, "failed to send request %lu\n", GetLastError() );

    ret = WinHttpReceiveResponse( req, NULL );
    ok( ret, "failed to receive response %lu\n", GetLastError() );

    status = 0xdeadbeef;
    size = sizeof(status);
    ret = WinHttpQueryHeaders( req, WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &size, NULL );
    ok( ret, "failed to query status code %lu\n", GetLastError() );
    ok( status == HTTP_STATUS_OK || broken(status == HTTP_STATUS_BAD_REQUEST), "request failed unexpectedly %lu\n", status );

    WinHttpCloseHandle( req );
    WinHttpCloseHandle( con );
    WinHttpCloseHandle( ses );

    ses = WinHttpOpen( L"winetest", WINHTTP_ACCESS_TYPE_NO_PROXY, NULL, NULL, 0 );
    ok( ses != NULL, "failed to open session %lu\n", GetLastError() );

    con = WinHttpConnect( ses, L"localhost", port, 0 );
    ok( con != NULL, "failed to open a connection %lu\n", GetLastError() );

    req = WinHttpOpenRequest( con, NULL, L"/cookie2", NULL, NULL, NULL, 0 );
    ok( req != NULL, "failed to open a request %lu\n", GetLastError() );

    ret = WinHttpSendRequest( req, NULL, 0, NULL, 0, 0, 0 );
    ok( ret, "failed to send request %lu\n", GetLastError() );

    ret = WinHttpReceiveResponse( req, NULL );
    ok( ret, "failed to receive response %lu\n", GetLastError() );

    status = 0xdeadbeef;
    size = sizeof(status);
    ret = WinHttpQueryHeaders( req, WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &size, NULL );
    ok( ret, "failed to query status code %lu\n", GetLastError() );
    ok( status == HTTP_STATUS_BAD_REQUEST, "request failed unexpectedly %lu\n", status );

    WinHttpCloseHandle( req );
    WinHttpCloseHandle( con );
    WinHttpCloseHandle( ses );

    ses = WinHttpOpen( L"winetest", WINHTTP_ACCESS_TYPE_NO_PROXY, NULL, NULL, 0 );
    ok( ses != NULL, "failed to open session %lu\n", GetLastError() );

    con = WinHttpConnect( ses, L"localhost", port, 0 );
    ok( con != NULL, "failed to open a connection %lu\n", GetLastError() );

    req = WinHttpOpenRequest( con, NULL, L"/cookie4", NULL, NULL, NULL, 0 );
    ok( req != NULL, "failed to open a request %lu\n", GetLastError() );

    ret = WinHttpSendRequest( req, NULL, 0, NULL, 0, 0, 0 );
    ok( ret, "failed to send request %lu\n", GetLastError() );

    ret = WinHttpReceiveResponse( req, NULL );
    ok( ret, "failed to receive response %lu\n", GetLastError() );

    status = 0xdeadbeef;
    size = sizeof(status);
    ret = WinHttpQueryHeaders( req, WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &size, NULL );
    ok( ret, "failed to query status code %lu\n", GetLastError() );
    ok( status == HTTP_STATUS_OK, "request failed unexpectedly %lu\n", status );
    WinHttpCloseHandle( req );

    req = WinHttpOpenRequest( con, NULL, L"/cookie5", NULL, NULL, NULL, 0 );
    ok( req != NULL, "failed to open a request %lu\n", GetLastError() );

    ret = WinHttpSendRequest( req, NULL, 0, NULL, 0, 0, 0 );
    ok( ret, "failed to send request %lu\n", GetLastError() );

    ret = WinHttpReceiveResponse( req, NULL );
    ok( ret, "failed to receive response %lu\n", GetLastError() );

    status = 0xdeadbeef;
    size = sizeof(status);
    ret = WinHttpQueryHeaders( req, WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &size, NULL );
    ok( ret, "failed to query status code %lu\n", GetLastError() );
    ok( status == HTTP_STATUS_OK || broken(status == HTTP_STATUS_BAD_REQUEST) /* < win7 */,
        "request failed unexpectedly %lu\n", status );

    WinHttpCloseHandle( req );
    WinHttpCloseHandle( con );
    WinHttpCloseHandle( ses );
}

static void do_request( HINTERNET con, const WCHAR *obj, DWORD flags )
{
    HINTERNET req;
    DWORD status, size;
    BOOL ret;

    req = WinHttpOpenRequest( con, NULL, obj, NULL, NULL, NULL, flags );
    ok( req != NULL, "failed to open a request %lu\n", GetLastError() );

    ret = WinHttpSendRequest( req, NULL, 0, NULL, 0, 0, 0 );
    ok( ret, "failed to send request %lu\n", GetLastError() );

    ret = WinHttpReceiveResponse( req, NULL );
    ok( ret, "failed to receive response %lu\n", GetLastError() );

    status = 0xdeadbeef;
    size = sizeof(status);
    ret = WinHttpQueryHeaders( req, WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &size, NULL );
    ok( ret, "failed to query status code %lu\n", GetLastError() );
    ok( status == HTTP_STATUS_OK || broken(status == HTTP_STATUS_BAD_REQUEST) /* < win7 */,
        "request %s with flags %#lx failed %lu\n", wine_dbgstr_w(obj), flags, status );
    WinHttpCloseHandle( req );
}

static void test_request_path_escapes( int port )
{
    static const WCHAR objW[] =
        {'/','e','s','c','a','p','e','\r','\n',0x1f,0x7f,'<',' ','%','o','n','e','?',0x1f,0x7f,' ','!','"','#',
         '$','%','&','\'','(',')','*','+',',','-','.','/',':',';','<','=','>','?','@','[','\\',']','^','_','`',
         '{','|','}','~','\r','\n',0};
    static const WCHAR obj2W[] =
        {'/','e','s','c','a','p','e','\r','\n',0x1f,0x7f,'<',' ','%','t','w','o','?',0x1f,0x7f,' ','!','"','#',
         '$','%','&','\'','(',')','*','+',',','-','.','/',':',';','<','=','>','?','@','[','\\',']','^','_','`',
         '{','|','}','~','\r','\n',0};
    static const WCHAR obj3W[] =
        {'/','e','s','c','a','p','e','\r','\n',0x1f,0x7f,'<',' ','%','t','h','r','e','e','?',0x1f,0x7f,' ','!',
         '"','#','$','%','&','\'','(',')','*','+',',','-','.','/',':',';','<','=','>','?','@','[','\\',']','^',
         '_','`','{','|','}','~','\r','\n',0};
    static const WCHAR obj4W[] =
        {'/','e','s','c','a','p','e','\r','\n',0x1f,0x7f,'<',' ','%','f','o','u','r','?',0x1f,0x7f,' ','!','"',
         '#','$','%','&','\'','(',')','*','+',',','-','.','/',':',';','<','=','>','?','@','[','\\',']','^','_',
         '`','{','|','}','~','\r','\n',0};
    static const WCHAR obj5W[] =
        {'/','e','s','c','a','p','e','&','t','e','x','t','=','o','n','e',0x80,0x7f,0x7e,0};
    static const WCHAR obj6W[] =
        {'/','e','s','c','a','p','e','&','t','e','x','t','=','t','w','o',0x80,0x7f,0x7e,0};
    static const WCHAR obj7W[] =
        {'/','e','s','c','a','p','e','&','t','e','x','t','=',0x541b,0x306e,0x540d,0x306f,0};
    HINTERNET ses, con;

    ses = WinHttpOpen( L"winetest", WINHTTP_ACCESS_TYPE_NO_PROXY, NULL, NULL, 0 );
    ok( ses != NULL, "failed to open session %lu\n", GetLastError() );

    con = WinHttpConnect( ses, L"localhost", port, 0 );
    ok( con != NULL, "failed to open a connection %lu\n", GetLastError() );

    do_request( con, objW, 0 );
    do_request( con, obj2W, WINHTTP_FLAG_ESCAPE_PERCENT );
    do_request( con, obj3W, WINHTTP_FLAG_ESCAPE_DISABLE );
    do_request( con, obj4W, WINHTTP_FLAG_ESCAPE_DISABLE_QUERY );
    do_request( con, obj5W, 0 );
    do_request( con, obj6W, WINHTTP_FLAG_ESCAPE_DISABLE );
    do_request( con, obj7W, WINHTTP_FLAG_ESCAPE_DISABLE );

    WinHttpCloseHandle( con );
    WinHttpCloseHandle( ses );
}

static void test_connection_info( int port )
{
    HINTERNET ses, con, req;
    WINHTTP_CONNECTION_INFO info;
    DWORD size, error;
    BOOL ret;

    ses = WinHttpOpen( L"winetest", WINHTTP_ACCESS_TYPE_NO_PROXY, NULL, NULL, 0 );
    ok( ses != NULL, "failed to open session %lu\n", GetLastError() );

    con = WinHttpConnect( ses, L"localhost", port, 0 );
    ok( con != NULL, "failed to open a connection %lu\n", GetLastError() );

    req = WinHttpOpenRequest( con, NULL, L"/basic", NULL, NULL, NULL, 0 );
    ok( req != NULL, "failed to open a request %lu\n", GetLastError() );

    size = sizeof(info);
    SetLastError( 0xdeadbeef );
    ret = WinHttpQueryOption( req, WINHTTP_OPTION_CONNECTION_INFO, &info, &size );
    error = GetLastError();
    if (!ret && error == ERROR_INVALID_PARAMETER)
    {
        win_skip( "WINHTTP_OPTION_CONNECTION_INFO not supported\n" );
        return;
    }
    ok( !ret, "unexpected success\n" );
    ok( error == ERROR_WINHTTP_INCORRECT_HANDLE_STATE, "got %lu\n", error );

    ret = WinHttpSendRequest( req, NULL, 0, NULL, 0, 0, 0 );
    ok( ret, "failed to send request %lu\n", GetLastError() );

    size = 0;
    SetLastError( 0xdeadbeef );
    ret = WinHttpQueryOption( req, WINHTTP_OPTION_CONNECTION_INFO, &info, &size );
    error = GetLastError();
    ok( !ret, "unexpected success\n" );
    ok( error == ERROR_INSUFFICIENT_BUFFER, "got %lu\n", error );

    size = sizeof(info);
    memset( &info, 0, sizeof(info) );
    ret = WinHttpQueryOption( req, WINHTTP_OPTION_CONNECTION_INFO, &info, &size );
    ok( ret, "failed to retrieve connection info %lu\n", GetLastError() );
    ok( info.cbSize == sizeof(info) || info.cbSize == sizeof(info) - sizeof(info.cbSize) /* Win7 */, "wrong size %lu\n", info.cbSize );

    ret = WinHttpReceiveResponse( req, NULL );
    ok( ret, "failed to receive response %lu\n", GetLastError() );

    size = sizeof(info);
    memset( &info, 0, sizeof(info) );
    ret = WinHttpQueryOption( req, WINHTTP_OPTION_CONNECTION_INFO, &info, &size );
    ok( ret, "failed to retrieve connection info %lu\n", GetLastError() );
    ok( info.cbSize == sizeof(info) || info.cbSize == sizeof(info) - sizeof(info.cbSize) /* Win7 */, "wrong size %lu\n", info.cbSize );

    WinHttpCloseHandle( req );
    WinHttpCloseHandle( con );
    WinHttpCloseHandle( ses );
}

static void test_passport_auth( int port )
{
    static const WCHAR headersW[] =
        L"HTTP/1.1 401 Found\r\nContent-Length: 0\r\nLocation: /\r\nWWW-Authenticate: Passport1.4\r\n\r\n";
    HINTERNET ses, con, req;
    DWORD status, size, option, err;
    WCHAR buf[128];
    BOOL ret;

    ses = WinHttpOpen( L"winetest", 0, NULL, NULL, 0 );
    ok( ses != NULL, "got %lu\n", GetLastError() );

    option = WINHTTP_ENABLE_PASSPORT_AUTH;
    ret = WinHttpSetOption( ses, WINHTTP_OPTION_CONFIGURE_PASSPORT_AUTH, &option, sizeof(option) );
    ok( ret, "got %lu\n", GetLastError() );

    con = WinHttpConnect( ses, L"localhost", port, 0 );
    ok( con != NULL, "got %lu\n", GetLastError() );

    req = WinHttpOpenRequest( con, NULL, L"/passport", NULL, NULL, NULL, 0 );
    ok( req != NULL, "got %lu\n", GetLastError() );

    ret = WinHttpSendRequest( req, NULL, 0, NULL, 0, 0, 0 );
    ok( ret, "got %lu\n", GetLastError() );

    ret = WinHttpReceiveResponse( req, NULL );
    err = GetLastError();
    ok( ret || broken(!ret && err == ERROR_WINHTTP_LOGIN_FAILURE) /* winxp */
            || broken(!ret && err == ERROR_WINHTTP_INVALID_SERVER_RESPONSE ), "got %lu\n", err );
    if (!ret)
    {
        win_skip("no support for Passport redirects\n");
        goto cleanup;
    }

    status = 0xdeadbeef;
    size = sizeof(status);
    ret = WinHttpQueryHeaders( req, WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &size, NULL );
    ok( ret, "got %lu\n", GetLastError() );
    ok( status == HTTP_STATUS_DENIED, "got %lu\n", status );

    buf[0] = 0;
    size = sizeof(buf);
    ret = WinHttpQueryHeaders( req, WINHTTP_QUERY_STATUS_TEXT, NULL, buf, &size, NULL );
    ok( ret, "got %lu\n", GetLastError() );
    ok( !lstrcmpW(L"Found", buf) || broken(!lstrcmpW(L"Unauthorized", buf)) /* < win7 */, "got %s\n",
        wine_dbgstr_w(buf) );

    buf[0] = 0;
    size = sizeof(buf);
    ret = WinHttpQueryHeaders( req, WINHTTP_QUERY_RAW_HEADERS_CRLF, NULL, buf, &size, NULL );
    ok( ret || broken(!ret && GetLastError() == ERROR_INSUFFICIENT_BUFFER) /* < win7 */, "got %lu\n", GetLastError() );
    if (ret)
    {
        ok( size == lstrlenW(headersW) * sizeof(WCHAR), "got %lu\n", size );
        ok( !lstrcmpW(headersW, buf), "got %s\n", wine_dbgstr_w(buf) );
    }

cleanup:
    WinHttpCloseHandle( req );
    WinHttpCloseHandle( con );
    WinHttpCloseHandle( ses );
}

static void test_credentials(void)
{
    static WCHAR userW[] = {'u','s','e','r',0};
    static WCHAR passW[] = {'p','a','s','s',0};
    static WCHAR proxy_userW[] = {'p','r','o','x','y','u','s','e','r',0};
    static WCHAR proxy_passW[] = {'p','r','o','x','y','p','a','s','s',0};
    HINTERNET ses, con, req;
    DWORD size, error;
    WCHAR buffer[32];
    BOOL ret;

    ses = WinHttpOpen(L"winetest", 0, proxy_userW, proxy_passW, 0);
    ok(ses != NULL, "failed to open session %lu\n", GetLastError());

    con = WinHttpConnect(ses, L"localhost", 0, 0);
    ok(con != NULL, "failed to open a connection %lu\n", GetLastError());

    req = WinHttpOpenRequest(con, NULL, NULL, NULL, NULL, NULL, 0);
    ok(req != NULL, "failed to open a request %lu\n", GetLastError());

    size = ARRAY_SIZE(buffer);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_PROXY_USERNAME, &buffer, &size);
    ok(ret, "failed to query proxy username %lu\n", GetLastError());
    ok(!buffer[0], "unexpected result %s\n", wine_dbgstr_w(buffer));
    ok(!size, "expected 0, got %lu\n", size);

    size = 4;
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_PROXY_USERNAME, NULL, &size);
    ok(!ret && GetLastError() == ERROR_INSUFFICIENT_BUFFER, "Unexpected error %lu\n", GetLastError());
    ok(size == 2, "Unexpected size %lu\n", size);

    size = ARRAY_SIZE(buffer);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_PROXY_PASSWORD, &buffer, &size);
    ok(ret, "failed to query proxy password %lu\n", GetLastError());
    ok(!buffer[0], "unexpected result %s\n", wine_dbgstr_w(buffer));
    ok(!size, "expected 0, got %lu\n", size);

    size = 4;
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_PROXY_PASSWORD, NULL, &size);
    ok(!ret && GetLastError() == ERROR_INSUFFICIENT_BUFFER, "Unexpected error %lu\n", GetLastError());
    ok(size == 2, "Unexpected size %lu\n", size);

    ret = WinHttpSetOption(req, WINHTTP_OPTION_PROXY_USERNAME, proxy_userW, lstrlenW(proxy_userW));
    ok(ret, "failed to set username %lu\n", GetLastError());

    size = ARRAY_SIZE(buffer);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_PROXY_USERNAME, &buffer, &size);
    ok(ret, "failed to query proxy username %lu\n", GetLastError());
    ok(!wcscmp(buffer, proxy_userW), "unexpected result %s\n", wine_dbgstr_w(buffer));
    ok(size == lstrlenW(proxy_userW) * sizeof(WCHAR), "unexpected result %lu\n", size);

    /* Exact buffer size match. */
    size = (lstrlenW(proxy_userW) + 1) * sizeof(WCHAR);
    buffer[0] = 0;
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_PROXY_USERNAME, &buffer, &size);
    ok(ret, "failed to query proxy username %lu\n", GetLastError());
    ok(!wcscmp(buffer, proxy_userW), "unexpected result %s\n", wine_dbgstr_w(buffer));
    ok(size == lstrlenW(proxy_userW) * sizeof(WCHAR), "unexpected result %lu\n", size);

    buffer[0] = 0x1;
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_PROXY_USERNAME, &buffer, &size);
    ok(!ret && GetLastError() == ERROR_INSUFFICIENT_BUFFER, "Unexpected error %lu\n", GetLastError());
    ok(*buffer == 0x1, "unexpected result %s\n", wine_dbgstr_w(buffer));
    ok(size == (lstrlenW(proxy_userW) + 1) * sizeof(WCHAR), "unexpected result %lu\n", size);

    size = 0;
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_PROXY_USERNAME, NULL, &size);
    ok(!ret && GetLastError() == ERROR_INSUFFICIENT_BUFFER, "Unexpected error %lu\n", GetLastError());
    ok(size == (lstrlenW(proxy_userW) + 1) * sizeof(WCHAR), "Unexpected size %lu\n", size);

    size = ARRAY_SIZE(buffer);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_USERNAME, &buffer, &size);
    ok(ret, "failed to query username %lu\n", GetLastError());
    ok(!buffer[0], "unexpected result %s\n", wine_dbgstr_w(buffer));
    ok(!size, "expected 0, got %lu\n", size);

    size = 4;
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_USERNAME, NULL, &size);
    ok(!ret && GetLastError() == ERROR_INSUFFICIENT_BUFFER, "Unexpected error %lu\n", GetLastError());
    ok(size == 2, "Unexpected size %lu\n", size);

    size = ARRAY_SIZE(buffer);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_PASSWORD, &buffer, &size);
    ok(ret, "failed to query password %lu\n", GetLastError());
    ok(!buffer[0], "unexpected result %s\n", wine_dbgstr_w(buffer));
    ok(!size, "expected 0, got %lu\n", size);

    size = 4;
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_PASSWORD, NULL, &size);
    ok(!ret && GetLastError() == ERROR_INSUFFICIENT_BUFFER, "Unexpected error %lu\n", GetLastError());
    ok(size == 2, "Unexpected size %lu\n", size);

    ret = WinHttpSetOption(req, WINHTTP_OPTION_PROXY_PASSWORD, proxy_passW, lstrlenW(proxy_passW));
    ok(ret, "failed to set proxy password %lu\n", GetLastError());

    size = ARRAY_SIZE(buffer);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_PROXY_PASSWORD, &buffer, &size);
    ok(ret, "failed to query proxy password %lu\n", GetLastError());
    ok(!wcscmp(buffer, proxy_passW), "unexpected result %s\n", wine_dbgstr_w(buffer));
    ok(size == lstrlenW(proxy_passW) * sizeof(WCHAR), "unexpected result %lu\n", size);

    /* Exact buffer size match. */
    size = (lstrlenW(proxy_passW) + 1) * sizeof(WCHAR);
    buffer[0] = 0;
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_PROXY_PASSWORD, &buffer, &size);
    ok(ret, "failed to query proxy password %lu\n", GetLastError());
    ok(!wcscmp(buffer, proxy_passW), "unexpected result %s\n", wine_dbgstr_w(buffer));
    ok(size == lstrlenW(proxy_passW) * sizeof(WCHAR), "unexpected result %lu\n", size);

    buffer[0] = 0x1;
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_PROXY_PASSWORD, &buffer, &size);
    ok(!ret && GetLastError() == ERROR_INSUFFICIENT_BUFFER, "Unexpected error %lu\n", GetLastError());
    ok(*buffer == 0x1, "unexpected result %s\n", wine_dbgstr_w(buffer));
    ok(size == (lstrlenW(proxy_passW) + 1) * sizeof(WCHAR), "unexpected result %lu\n", size);

    size = 0;
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_PROXY_PASSWORD, NULL, &size);
    ok(!ret && GetLastError() == ERROR_INSUFFICIENT_BUFFER, "Unexpected error %lu\n", GetLastError());
    ok(size == (lstrlenW(proxy_passW) + 1) * sizeof(WCHAR), "Unexpected size %lu\n", size);

    ret = WinHttpSetOption(req, WINHTTP_OPTION_USERNAME, userW, lstrlenW(userW));
    ok(ret, "failed to set username %lu\n", GetLastError());

    size = ARRAY_SIZE(buffer);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_USERNAME, &buffer, &size);
    ok(ret, "failed to query username %lu\n", GetLastError());
    ok(!wcscmp(buffer, userW), "unexpected result %s\n", wine_dbgstr_w(buffer));
    ok(size == lstrlenW(userW) * sizeof(WCHAR), "unexpected result %lu\n", size);

    /* Exact buffer size match. */
    size = (lstrlenW(userW) + 1) * sizeof(WCHAR);
    buffer[0] = 0;
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_USERNAME, &buffer, &size);
    ok(ret, "failed to query username %lu\n", GetLastError());
    ok(!wcscmp(buffer, userW), "unexpected result %s\n", wine_dbgstr_w(buffer));
    ok(size == lstrlenW(userW) * sizeof(WCHAR), "unexpected result %lu\n", size);

    buffer[0] = 0x1;
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_USERNAME, &buffer, &size);
    ok(!ret && GetLastError() == ERROR_INSUFFICIENT_BUFFER, "Unexpected error %lu\n", GetLastError());
    ok(*buffer == 0x1, "unexpected result %s\n", wine_dbgstr_w(buffer));
    ok(size == (lstrlenW(userW) + 1) * sizeof(WCHAR), "unexpected result %lu\n", size);

    size = 0;
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_USERNAME, NULL, &size);
    ok(!ret && GetLastError() == ERROR_INSUFFICIENT_BUFFER, "Unexpected error %lu\n", GetLastError());
    ok(size == (lstrlenW(userW) + 1) * sizeof(WCHAR), "Unexpected size %lu\n", size);

    ret = WinHttpSetOption(req, WINHTTP_OPTION_PASSWORD, passW, lstrlenW(passW));
    ok(ret, "failed to set password %lu\n", GetLastError());

    size = ARRAY_SIZE(buffer);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_PASSWORD, &buffer, &size);
    ok(ret, "failed to query password %lu\n", GetLastError());
    ok(!wcscmp(buffer, passW), "unexpected result %s\n", wine_dbgstr_w(buffer));
    ok(size == lstrlenW(passW) * sizeof(WCHAR), "unexpected result %lu\n", size);

    /* Exact buffer size match. */
    buffer[0] = 0;
    size = (lstrlenW(passW) + 1) * sizeof(WCHAR);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_PASSWORD, &buffer, &size);
    ok(ret, "failed to query password %lu\n", GetLastError());
    ok(!wcscmp(buffer, passW), "unexpected result %s\n", wine_dbgstr_w(buffer));
    ok(size == lstrlenW(passW) * sizeof(WCHAR), "unexpected result %lu\n", size);

    buffer[0] = 0x1;
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_PASSWORD, &buffer, &size);
    ok(!ret && GetLastError() == ERROR_INSUFFICIENT_BUFFER, "Unexpected error %lu\n", GetLastError());
    ok(*buffer == 0x1, "unexpected result %s\n", wine_dbgstr_w(buffer));
    ok(size == (lstrlenW(passW) + 1) * sizeof(WCHAR), "unexpected result %lu\n", size);

    size = 0;
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_PASSWORD, NULL, &size);
    ok(!ret && GetLastError() == ERROR_INSUFFICIENT_BUFFER, "Unexpected error %lu\n", GetLastError());
    ok(size == (lstrlenW(passW) + 1) * sizeof(WCHAR), "Unexpected size %lu\n", size);

    WinHttpCloseHandle(req);

    req = WinHttpOpenRequest(con, NULL, NULL, NULL, NULL, NULL, 0);
    ok(req != NULL, "failed to open a request %lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = WinHttpSetCredentials(req, WINHTTP_AUTH_TARGET_SERVER, WINHTTP_AUTH_SCHEME_BASIC, userW, NULL, NULL);
    error = GetLastError();
    ok(!ret, "expected failure\n");
    ok(error == ERROR_INVALID_PARAMETER, "expected ERROR_INVALID_PARAMETER, got %lu\n", error);

    SetLastError(0xdeadbeef);
    ret = WinHttpSetCredentials(req, WINHTTP_AUTH_TARGET_SERVER, WINHTTP_AUTH_SCHEME_BASIC, NULL, passW, NULL);
    error = GetLastError();
    ok(!ret, "expected failure\n");
    ok(error == ERROR_INVALID_PARAMETER, "expected ERROR_INVALID_PARAMETER, got %lu\n", error);

    ret = WinHttpSetCredentials(req, WINHTTP_AUTH_TARGET_SERVER, WINHTTP_AUTH_SCHEME_BASIC, userW, passW, NULL);
    ok(ret, "failed to set credentials %lu\n", GetLastError());

    size = ARRAY_SIZE(buffer);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_USERNAME, &buffer, &size);
    ok(ret, "failed to query username %lu\n", GetLastError());
    todo_wine {
    ok(!buffer[0], "unexpected result %s\n", wine_dbgstr_w(buffer));
    ok(!size, "expected 0, got %lu\n", size);
    }

    size = ARRAY_SIZE(buffer);
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_PASSWORD, &buffer, &size);
    ok(ret, "failed to query password %lu\n", GetLastError());
    todo_wine {
    ok(!buffer[0], "unexpected result %s\n", wine_dbgstr_w(buffer));
    ok(!size, "expected 0, got %lu\n", size);
    }

    WinHttpCloseHandle(req);
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
}

static void test_IWinHttpRequest(int port)
{
    static const WCHAR data_start[] = {'<','!','D','O','C','T','Y','P','E',' ','h','t','m','l',' ','P','U','B','L','I','C'};
    HRESULT hr;
    IWinHttpRequest *req;
    BSTR method, url, username, password, response = NULL, status_text = NULL, headers = NULL;
    BSTR date, today, connection, value = NULL;
    VARIANT async, empty, timeout, body, body2, proxy_server, bypass_list, data, cp, flags;
    VARIANT_BOOL succeeded;
    LONG status;
    WCHAR todayW[WINHTTP_TIME_FORMAT_BUFSIZE];
    SYSTEMTIME st;
    IStream *stream, *stream2;
    LARGE_INTEGER pos;
    char buf[128];
    WCHAR bufW[128];
    DWORD count;

    GetSystemTime( &st );
    WinHttpTimeFromSystemTime( &st, todayW );

    CoInitialize( NULL );
    hr = CoCreateInstance( &CLSID_WinHttpRequest, NULL, CLSCTX_INPROC_SERVER, &IID_IWinHttpRequest, (void **)&req );
    ok( hr == S_OK, "got %#lx\n", hr );

    V_VT( &empty ) = VT_ERROR;
    V_ERROR( &empty ) = 0xdeadbeef;

    V_VT( &async ) = VT_BOOL;
    V_BOOL( &async ) = VARIANT_FALSE;

    method = SysAllocString( L"POST" );
    url = SysAllocString( L"http://test.winehq.org/tests/post.php" );
    hr = IWinHttpRequest_Open( req, method, url, async );
    ok( hr == S_OK, "got %#lx\n", hr );
    SysFreeString( method );
    SysFreeString( url );

    V_VT( &data ) = VT_BSTR;
    V_BSTR( &data ) = SysAllocString( L"testdata\x80" );
    hr = IWinHttpRequest_Send( req, data );
    ok( hr == S_OK || hr == HRESULT_FROM_WIN32( ERROR_WINHTTP_INVALID_SERVER_RESPONSE ), "got %#lx\n", hr );
    SysFreeString( V_BSTR( &data ) );
    if (hr != S_OK) goto done;

    hr = IWinHttpRequest_Open( req, NULL, NULL, empty );
    ok( hr == E_INVALIDARG, "got %#lx\n", hr );

    method = SysAllocString( L"GET" );
    hr = IWinHttpRequest_Open( req, method, NULL, empty );
    ok( hr == E_INVALIDARG, "got %#lx\n", hr );

    hr = IWinHttpRequest_Open( req, method, NULL, async );
    ok( hr == E_INVALIDARG, "got %#lx\n", hr );

    url = SysAllocString( L"http://test.winehq.org" );
    hr = IWinHttpRequest_Open( req, NULL, url, empty );
    ok( hr == E_INVALIDARG, "got %#lx\n", hr );

    hr = IWinHttpRequest_Abort( req );
    ok( hr == S_OK, "got %#lx\n", hr );

    hr = IWinHttpRequest_Open( req, method, url, empty );
    ok( hr == S_OK, "got %#lx\n", hr );

    hr = IWinHttpRequest_Abort( req );
    ok( hr == S_OK, "got %#lx\n", hr );

    IWinHttpRequest_Release( req );

    hr = CoCreateInstance( &CLSID_WinHttpRequest, NULL, CLSCTX_INPROC_SERVER, &IID_IWinHttpRequest, (void **)&req );
    ok( hr == S_OK, "got %#lx\n", hr );

    SysFreeString( url );
    url = SysAllocString( L"test.winehq.org" );
    hr = IWinHttpRequest_Open( req, method, url, async );
    ok( hr == HRESULT_FROM_WIN32( ERROR_WINHTTP_UNRECOGNIZED_SCHEME ), "got %#lx\n", hr );

    SysFreeString( method );
    method = SysAllocString( L"INVALID" );
    hr = IWinHttpRequest_Open( req, method, url, async );
    ok( hr == HRESULT_FROM_WIN32( ERROR_WINHTTP_UNRECOGNIZED_SCHEME ), "got %#lx\n", hr );

    V_VT( &flags ) = VT_ERROR;
    V_ERROR( &flags ) = 0xdeadbeef;
    hr = IWinHttpRequest_get_Option( req, WinHttpRequestOption_SslErrorIgnoreFlags, &flags );
    ok( hr == S_OK, "got %#lx\n", hr );
    ok( V_VT( &flags ) == VT_I4, "got %#x\n", V_VT( &flags ) );
    ok( V_I4( &flags ) == 0, "got %lx\n", V_I4( &flags ) );

    V_VT( &flags ) = VT_I4;
    V_I4( &flags ) = SECURITY_FLAG_IGNORE_UNKNOWN_CA;
    hr = IWinHttpRequest_put_Option( req, WinHttpRequestOption_SslErrorIgnoreFlags, flags );
    ok( hr == S_OK, "got %#lx\n", hr );

    V_VT( &flags ) = VT_ERROR;
    V_ERROR( &flags ) = 0xdeadbeef;
    hr = IWinHttpRequest_get_Option( req, WinHttpRequestOption_SslErrorIgnoreFlags, &flags );
    ok( hr == S_OK, "got %#lx\n", hr );
    ok( V_I4( &flags ) == SECURITY_FLAG_IGNORE_UNKNOWN_CA, "got %lx\n", V_I4( &flags ) );

    SysFreeString( method );
    method = SysAllocString( L"GET" );
    SysFreeString( url );
    url = SysAllocString( L"http://test.winehq.org" );
    V_VT( &async ) = VT_ERROR;
    V_ERROR( &async ) = DISP_E_PARAMNOTFOUND;
    hr = IWinHttpRequest_Open( req, method, url, async );
    ok( hr == S_OK, "got %#lx\n", hr );

    V_VT( &cp ) = VT_ERROR;
    V_ERROR( &cp ) = 0xdeadbeef;
    hr = IWinHttpRequest_get_Option( req, WinHttpRequestOption_URLCodePage, &cp );
    ok( hr == S_OK, "got %#lx\n", hr );
    ok( V_VT( &cp ) == VT_I4, "got %#x\n", V_VT( &cp ) );
    ok( V_I4( &cp ) == CP_UTF8, "got %ld\n", V_I4( &cp ) );

    V_VT( &cp ) = VT_UI4;
    V_UI4( &cp ) = CP_ACP;
    hr = IWinHttpRequest_put_Option( req, WinHttpRequestOption_URLCodePage, cp );
    ok( hr == S_OK, "got %#lx\n", hr );

    V_VT( &cp ) = VT_ERROR;
    V_ERROR( &cp ) = 0xdeadbeef;
    hr = IWinHttpRequest_get_Option( req, WinHttpRequestOption_URLCodePage, &cp );
    ok( hr == S_OK, "got %#lx\n", hr );
    ok( V_VT( &cp ) == VT_I4, "got %#x\n", V_VT( &cp ) );
    ok( V_I4( &cp ) == CP_ACP, "got %ld\n", V_I4( &cp ) );

    value = SysAllocString( L"utf-8" );
    V_VT( &cp ) = VT_BSTR;
    V_BSTR( &cp ) = value;
    hr = IWinHttpRequest_put_Option( req, WinHttpRequestOption_URLCodePage, cp );
    ok( hr == S_OK, "got %#lx\n", hr );
    SysFreeString( value );

    V_VT( &cp ) = VT_ERROR;
    V_ERROR( &cp ) = 0xdeadbeef;
    hr = IWinHttpRequest_get_Option( req, WinHttpRequestOption_URLCodePage, &cp );
    ok( hr == S_OK, "got %#lx\n", hr );
    ok( V_VT( &cp ) == VT_I4, "got %#x\n", V_VT( &cp ) );
    ok( V_I4( &cp ) == CP_UTF8, "got %ld\n", V_I4( &cp ) );

    V_VT( &flags ) = VT_ERROR;
    V_ERROR( &flags ) = 0xdeadbeef;
    hr = IWinHttpRequest_get_Option( req, WinHttpRequestOption_SslErrorIgnoreFlags, &flags );
    ok( hr == S_OK, "got %#lx\n", hr );
    ok( V_I4( &flags ) == SECURITY_FLAG_IGNORE_UNKNOWN_CA, "got %lx\n", V_I4( &flags ) );

    V_VT( &flags ) = VT_I4;
    V_I4( &flags ) = 0x321;
    hr = IWinHttpRequest_put_Option( req, WinHttpRequestOption_SslErrorIgnoreFlags, flags );
    ok( hr == E_INVALIDARG, "got %#lx\n", hr );

    V_VT( &flags ) = VT_UI4;
    V_UI4( &flags ) = 0x123;
    hr = IWinHttpRequest_put_Option( req, WinHttpRequestOption_SslErrorIgnoreFlags, flags );
    ok( hr == E_INVALIDARG, "got %#lx\n", hr );

    V_VT( &flags ) = VT_UI4;
    V_UI4( &flags ) = SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
    hr = IWinHttpRequest_put_Option( req, WinHttpRequestOption_SslErrorIgnoreFlags, flags);
    ok( hr == S_OK, "got %#lx\n", hr );

    V_VT( &flags ) = VT_ERROR;
    V_ERROR( &flags ) = 0xdeadbeef;
    hr = IWinHttpRequest_get_Option( req, WinHttpRequestOption_SslErrorIgnoreFlags, &flags );
    ok( hr == S_OK, "got %#lx\n", hr );
    ok( V_I4( &flags ) == SECURITY_FLAG_IGNORE_CERT_DATE_INVALID, "got %lx\n", V_I4( &flags ) );

    V_VT( &flags ) = VT_I4;
    V_I4( &flags ) = SECURITY_FLAG_IGNORE_UNKNOWN_CA|SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
    hr = IWinHttpRequest_put_Option( req, WinHttpRequestOption_SslErrorIgnoreFlags, flags );
    ok( hr == S_OK, "got %#lx\n", hr );

    V_VT( &flags ) = VT_ERROR;
    V_ERROR( &flags ) = 0xdeadbeef;
    hr = IWinHttpRequest_get_Option( req, WinHttpRequestOption_SslErrorIgnoreFlags, &flags );
    ok( hr == S_OK, "got %#lx\n", hr );
    ok( V_I4( &flags ) == (SECURITY_FLAG_IGNORE_UNKNOWN_CA|SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE), "got %lx\n", V_I4( &flags ) );

    hr = IWinHttpRequest_Abort( req );
    ok( hr == S_OK, "got %#lx\n", hr );

    hr = IWinHttpRequest_Send( req, empty );
    ok( hr == HRESULT_FROM_WIN32( ERROR_WINHTTP_CANNOT_CALL_BEFORE_OPEN ), "got %#lx\n", hr );

    hr = IWinHttpRequest_Abort( req );
    ok( hr == S_OK, "got %#lx\n", hr );

    IWinHttpRequest_Release( req );

    hr = CoCreateInstance( &CLSID_WinHttpRequest, NULL, CLSCTX_INPROC_SERVER, &IID_IWinHttpRequest, (void **)&req );
    ok( hr == S_OK, "got %#lx\n", hr );

    hr = IWinHttpRequest_get_ResponseText( req, NULL );
    ok( hr == E_INVALIDARG, "got %#lx\n", hr );

    hr = IWinHttpRequest_get_ResponseText( req, &response );
    ok( hr == HRESULT_FROM_WIN32( ERROR_WINHTTP_CANNOT_CALL_BEFORE_SEND ), "got %#lx\n", hr );

    hr = IWinHttpRequest_get_Status( req, NULL );
    ok( hr == E_INVALIDARG, "got %#lx\n", hr );

    hr = IWinHttpRequest_get_Status( req, &status );
    ok( hr == HRESULT_FROM_WIN32( ERROR_WINHTTP_CANNOT_CALL_BEFORE_SEND ), "got %#lx\n", hr );

    hr = IWinHttpRequest_get_StatusText( req, NULL );
    ok( hr == E_INVALIDARG, "got %#lx\n", hr );

    hr = IWinHttpRequest_get_StatusText( req, &status_text );
    ok( hr == HRESULT_FROM_WIN32( ERROR_WINHTTP_CANNOT_CALL_BEFORE_SEND ), "got %#lx\n", hr );

    hr = IWinHttpRequest_get_ResponseBody( req, NULL );
    ok( hr == E_INVALIDARG, "got %#lx\n", hr );

    hr = IWinHttpRequest_SetTimeouts( req, 10000, 10000, 10000, 10000 );
    ok( hr == S_OK, "got %#lx\n", hr );

    hr = IWinHttpRequest_SetCredentials( req, NULL, NULL, 0xdeadbeef );
    ok( hr == HRESULT_FROM_WIN32( ERROR_WINHTTP_CANNOT_CALL_BEFORE_OPEN ), "got %#lx\n", hr );

    VariantInit( &proxy_server );
    V_VT( &proxy_server ) = VT_ERROR;
    VariantInit( &bypass_list );
    V_VT( &bypass_list ) = VT_ERROR;
    hr = IWinHttpRequest_SetProxy( req, HTTPREQUEST_PROXYSETTING_DIRECT, proxy_server, bypass_list );
    ok( hr == S_OK, "got %#lx\n", hr );

    hr = IWinHttpRequest_SetProxy( req, HTTPREQUEST_PROXYSETTING_PROXY, proxy_server, bypass_list );
    ok( hr == S_OK, "got %#lx\n", hr );

    hr = IWinHttpRequest_SetProxy( req, HTTPREQUEST_PROXYSETTING_DIRECT, proxy_server, bypass_list );
    ok( hr == S_OK, "got %#lx\n", hr );

    hr = IWinHttpRequest_GetAllResponseHeaders( req, NULL );
    ok( hr == E_INVALIDARG, "got %#lx\n", hr );

    hr = IWinHttpRequest_GetAllResponseHeaders( req, &headers );
    ok( hr == HRESULT_FROM_WIN32( ERROR_WINHTTP_CANNOT_CALL_BEFORE_SEND ), "got %#lx\n", hr );

    hr = IWinHttpRequest_GetResponseHeader( req, NULL, NULL );
    ok( hr == HRESULT_FROM_WIN32( ERROR_WINHTTP_CANNOT_CALL_BEFORE_SEND ), "got %#lx\n", hr );

    connection = SysAllocString( L"Connection" );
    hr = IWinHttpRequest_GetResponseHeader( req, connection, NULL );
    ok( hr == HRESULT_FROM_WIN32( ERROR_WINHTTP_CANNOT_CALL_BEFORE_SEND ), "got %#lx\n", hr );

    hr = IWinHttpRequest_GetResponseHeader( req, connection, &value );
    ok( hr == HRESULT_FROM_WIN32( ERROR_WINHTTP_CANNOT_CALL_BEFORE_SEND ), "got %#lx\n", hr );

    hr = IWinHttpRequest_SetRequestHeader( req, NULL, NULL );
    ok( hr == E_INVALIDARG, "got %#lx\n", hr );

    date = SysAllocString( L"Date" );
    hr = IWinHttpRequest_SetRequestHeader( req, date, NULL );
    ok( hr == HRESULT_FROM_WIN32( ERROR_WINHTTP_CANNOT_CALL_BEFORE_OPEN ), "got %#lx\n", hr );

    today = SysAllocString( todayW );
    hr = IWinHttpRequest_SetRequestHeader( req, date, today );
    ok( hr == HRESULT_FROM_WIN32( ERROR_WINHTTP_CANNOT_CALL_BEFORE_OPEN ), "got %#lx\n", hr );

    hr = IWinHttpRequest_SetAutoLogonPolicy( req, 0xdeadbeef );
    ok( hr == E_INVALIDARG, "got %#lx\n", hr );

    hr = IWinHttpRequest_SetAutoLogonPolicy( req, AutoLogonPolicy_OnlyIfBypassProxy );
    ok( hr == S_OK, "got %#lx\n", hr );

    SysFreeString( method );
    method = SysAllocString( L"GET" );
    SysFreeString( url );
    url = SysAllocString( L"http://test.winehq.org" );
    hr = IWinHttpRequest_Open( req, method, url, async );
    ok( hr == S_OK, "got %#lx\n", hr );

    hr = IWinHttpRequest_get_ResponseText( req, NULL );
    ok( hr == E_INVALIDARG, "got %#lx\n", hr );

    hr = IWinHttpRequest_get_ResponseText( req, &response );
    ok( hr == HRESULT_FROM_WIN32( ERROR_WINHTTP_CANNOT_CALL_BEFORE_SEND ), "got %#lx\n", hr );

    hr = IWinHttpRequest_get_Status( req, &status );
    ok( hr == HRESULT_FROM_WIN32( ERROR_WINHTTP_CANNOT_CALL_BEFORE_SEND ), "got %#lx\n", hr );

    hr = IWinHttpRequest_get_StatusText( req, &status_text );
    ok( hr == HRESULT_FROM_WIN32( ERROR_WINHTTP_CANNOT_CALL_BEFORE_SEND ), "got %#lx\n", hr );

    hr = IWinHttpRequest_get_ResponseBody( req, NULL );
    ok( hr == E_INVALIDARG, "got %#lx\n", hr );

    hr = IWinHttpRequest_SetTimeouts( req, 10000, 10000, 10000, 10000 );
    ok( hr == S_OK, "got %#lx\n", hr );

    hr = IWinHttpRequest_SetCredentials( req, NULL, NULL, 0xdeadbeef );
    ok( hr == E_INVALIDARG, "got %#lx\n", hr );

    username = SysAllocString( L"username" );
    hr = IWinHttpRequest_SetCredentials( req, username, NULL, 0xdeadbeef );
    ok( hr == E_INVALIDARG, "got %#lx\n", hr );

    password = SysAllocString( L"password" );
    hr = IWinHttpRequest_SetCredentials( req, NULL, password, 0xdeadbeef );
    ok( hr == E_INVALIDARG, "got %#lx\n", hr );

    hr = IWinHttpRequest_SetCredentials( req, username, password, 0xdeadbeef );
    ok( hr == E_INVALIDARG, "got %#lx\n", hr );

    hr = IWinHttpRequest_SetCredentials( req, NULL, password, HTTPREQUEST_SETCREDENTIALS_FOR_SERVER );
    ok( hr == E_INVALIDARG, "got %#lx\n", hr );

    hr = IWinHttpRequest_SetCredentials( req, username, password, HTTPREQUEST_SETCREDENTIALS_FOR_SERVER );
    ok( hr == S_OK, "got %#lx\n", hr );

    V_VT( &flags ) = VT_I4;
    V_I4( &flags ) = SECURITY_FLAG_IGNORE_UNKNOWN_CA|SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
    hr = IWinHttpRequest_put_Option( req, WinHttpRequestOption_SslErrorIgnoreFlags, flags );
    ok( hr == S_OK, "got %#lx\n", hr );

    V_VT( &proxy_server ) = VT_BSTR;
    V_BSTR( &proxy_server ) = SysAllocString( L"proxyserver" );
    V_VT( &bypass_list ) = VT_BSTR;
    V_BSTR( &bypass_list ) = SysAllocString( L"bypasslist" );
    hr = IWinHttpRequest_SetProxy( req, HTTPREQUEST_PROXYSETTING_PROXY, proxy_server, bypass_list );
    ok( hr == S_OK, "got %#lx\n", hr );

    hr = IWinHttpRequest_SetProxy( req, 0xdeadbeef, proxy_server, bypass_list );
    ok( hr == E_INVALIDARG, "got %#lx\n", hr );

    hr = IWinHttpRequest_SetProxy( req, HTTPREQUEST_PROXYSETTING_DIRECT, proxy_server, bypass_list );
    ok( hr == S_OK, "got %#lx\n", hr );

    hr = IWinHttpRequest_GetAllResponseHeaders( req, &headers );
    ok( hr == HRESULT_FROM_WIN32( ERROR_WINHTTP_CANNOT_CALL_BEFORE_SEND ), "got %#lx\n", hr );

    hr = IWinHttpRequest_GetResponseHeader( req, connection, &value );
    ok( hr == HRESULT_FROM_WIN32( ERROR_WINHTTP_CANNOT_CALL_BEFORE_SEND ), "got %#lx\n", hr );

    hr = IWinHttpRequest_SetRequestHeader( req, date, today );
    ok( hr == S_OK, "got %#lx\n", hr );

    hr = IWinHttpRequest_SetRequestHeader( req, date, NULL );
    ok( hr == S_OK, "got %#lx\n", hr );

    hr = IWinHttpRequest_SetAutoLogonPolicy( req, AutoLogonPolicy_OnlyIfBypassProxy );
    ok( hr == S_OK, "got %#lx\n", hr );

    hr = IWinHttpRequest_Send( req, empty );
    ok( hr == S_OK, "got %#lx\n", hr );

    hr = IWinHttpRequest_Send( req, empty );
    ok( hr == S_OK, "got %#lx\n", hr );

    V_VT( &flags ) = VT_ERROR;
    V_ERROR( &flags ) = 0xdeadbeef;
    hr = IWinHttpRequest_get_Option( req, WinHttpRequestOption_SslErrorIgnoreFlags, &flags );
    ok( hr == S_OK, "got %#lx\n", hr );
    ok( V_I4( &flags ) == (SECURITY_FLAG_IGNORE_UNKNOWN_CA|SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE), "got %lx\n", V_I4( &flags ) );

    hr = IWinHttpRequest_get_ResponseText( req, NULL );
    ok( hr == E_INVALIDARG, "got %#lx\n", hr );

    hr = IWinHttpRequest_get_ResponseText( req, &response );
    ok( hr == S_OK, "got %#lx\n", hr );
    ok( !memcmp(response, data_start, sizeof(data_start)), "got %s\n", wine_dbgstr_wn(response, 32) );
    SysFreeString( response );

    hr = IWinHttpRequest_get_Status( req, NULL );
    ok( hr == E_INVALIDARG, "got %#lx\n", hr );

    status = 0;
    hr = IWinHttpRequest_get_Status( req, &status );
    ok( hr == S_OK, "got %#lx\n", hr );
    trace("Status = %lu\n", status);

    hr = IWinHttpRequest_get_StatusText( req, NULL );
    ok( hr == E_INVALIDARG, "got %#lx\n", hr );

    hr = IWinHttpRequest_get_StatusText( req, &status_text );
    ok( hr == S_OK, "got %#lx\n", hr );
    trace("StatusText=%s\n", wine_dbgstr_w(status_text));
    SysFreeString( status_text );

    hr = IWinHttpRequest_get_ResponseBody( req, NULL );
    ok( hr == E_INVALIDARG, "got %#lx\n", hr );

    hr = IWinHttpRequest_SetCredentials( req, username, password, HTTPREQUEST_SETCREDENTIALS_FOR_SERVER );
    ok( hr == S_OK, "got %#lx\n", hr );

    hr = IWinHttpRequest_SetProxy( req, HTTPREQUEST_PROXYSETTING_PROXY, proxy_server, bypass_list );
    ok( hr == S_OK, "got %#lx\n", hr );

    hr = IWinHttpRequest_SetProxy( req, HTTPREQUEST_PROXYSETTING_DIRECT, proxy_server, bypass_list );
    ok( hr == S_OK, "got %#lx\n", hr );

    hr = IWinHttpRequest_GetAllResponseHeaders( req, NULL );
    ok( hr == E_INVALIDARG, "got %#lx\n", hr );

    hr = IWinHttpRequest_GetAllResponseHeaders( req, &headers );
    ok( hr == S_OK, "got %#lx\n", hr );
    SysFreeString( headers );

    hr = IWinHttpRequest_GetResponseHeader( req, NULL, NULL );
    ok( hr == E_INVALIDARG, "got %#lx\n", hr );

    hr = IWinHttpRequest_GetResponseHeader( req, connection, NULL );
    ok( hr == E_INVALIDARG, "got %#lx\n", hr );

    hr = IWinHttpRequest_GetResponseHeader( req, connection, &value );
    ok( hr == S_OK, "got %#lx\n", hr );
    SysFreeString( value );

    hr = IWinHttpRequest_SetRequestHeader( req, date, today );
    ok( hr == HRESULT_FROM_WIN32( ERROR_WINHTTP_CANNOT_CALL_AFTER_SEND ), "got %#lx\n", hr );

    hr = IWinHttpRequest_SetAutoLogonPolicy( req, AutoLogonPolicy_OnlyIfBypassProxy );
    ok( hr == S_OK, "got %#lx\n", hr );

    VariantInit( &timeout );
    V_VT( &timeout ) = VT_I4;
    V_I4( &timeout ) = 10;
    hr = IWinHttpRequest_WaitForResponse( req, timeout, &succeeded );
    ok( hr == S_OK, "got %#lx\n", hr );

    hr = IWinHttpRequest_get_Status( req, &status );
    ok( hr == S_OK, "got %#lx\n", hr );

    hr = IWinHttpRequest_get_StatusText( req, &status_text );
    ok( hr == S_OK, "got %#lx\n", hr );
    SysFreeString( status_text );

    hr = IWinHttpRequest_SetCredentials( req, username, password, HTTPREQUEST_SETCREDENTIALS_FOR_SERVER );
    ok( hr == S_OK, "got %#lx\n", hr );

    hr = IWinHttpRequest_SetProxy( req, HTTPREQUEST_PROXYSETTING_PROXY, proxy_server, bypass_list );
    ok( hr == S_OK, "got %#lx\n", hr );

    hr = IWinHttpRequest_SetProxy( req, HTTPREQUEST_PROXYSETTING_DIRECT, proxy_server, bypass_list );
    ok( hr == S_OK, "got %#lx\n", hr );

    hr = IWinHttpRequest_Send( req, empty );
    ok( hr == S_OK, "got %#lx\n", hr );

    hr = IWinHttpRequest_get_ResponseText( req, NULL );
    ok( hr == E_INVALIDARG, "got %#lx\n", hr );

    hr = IWinHttpRequest_get_ResponseText( req, &response );
    ok( hr == S_OK, "got %#lx\n", hr );
    SysFreeString( response );

    hr = IWinHttpRequest_get_ResponseBody( req, NULL );
    ok( hr == E_INVALIDARG, "got %#lx\n", hr );

    VariantInit( &body );
    V_VT( &body ) = VT_ERROR;
    hr = IWinHttpRequest_get_ResponseBody( req, &body );
    ok( hr == S_OK, "got %#lx\n", hr );
    ok( V_VT( &body ) == (VT_ARRAY|VT_UI1), "got %#x\n", V_VT( &body ) );

    hr = VariantClear( &body );
    ok( hr == S_OK, "got %#lx\n", hr );

    VariantInit( &body );
    V_VT( &body ) = VT_ERROR;
    hr = IWinHttpRequest_get_ResponseStream( req, &body );
    ok( hr == S_OK, "got %#lx\n", hr );
    ok( V_VT( &body ) == VT_UNKNOWN, "got %#x\n", V_VT( &body ) );

    hr = IUnknown_QueryInterface( V_UNKNOWN( &body ), &IID_IStream, (void **)&stream );
    ok( hr == S_OK, "got %#lx\n", hr );
    ok( V_UNKNOWN( &body ) == (IUnknown *)stream, "got different interface pointer\n" );

    buf[0] = 0;
    count = 0xdeadbeef;
    hr = IStream_Read( stream, buf, 128, &count );
    ok( hr == S_OK, "got %#lx\n", hr );
    ok( count != 0xdeadbeef, "count not set\n" );
    ok( buf[0], "no data\n" );

    VariantInit( &body2 );
    V_VT( &body2 ) = VT_ERROR;
    hr = IWinHttpRequest_get_ResponseStream( req, &body2 );
    ok( hr == S_OK, "got %#lx\n", hr );
    ok( V_VT( &body2 ) == VT_UNKNOWN, "got %#x\n", V_VT( &body2 ) );
    ok( V_UNKNOWN( &body ) != V_UNKNOWN( &body2 ), "got same interface pointer\n" );

    hr = IUnknown_QueryInterface( V_UNKNOWN( &body2 ), &IID_IStream, (void **)&stream2 );
    ok( hr == S_OK, "got %#lx\n", hr );
    ok( V_UNKNOWN( &body2 ) == (IUnknown *)stream2, "got different interface pointer\n" );
    IStream_Release( stream2 );

    hr = VariantClear( &body );
    ok( hr == S_OK, "got %#lx\n", hr );

    hr = VariantClear( &body2 );
    ok( hr == S_OK, "got %#lx\n", hr );

    hr = IWinHttpRequest_SetProxy( req, HTTPREQUEST_PROXYSETTING_PROXY, proxy_server, bypass_list );
    ok( hr == S_OK, "got %#lx\n", hr );

    hr = IWinHttpRequest_SetProxy( req, HTTPREQUEST_PROXYSETTING_DIRECT, proxy_server, bypass_list );
    ok( hr == S_OK, "got %#lx\n", hr );

    hr = IWinHttpRequest_GetAllResponseHeaders( req, &headers );
    ok( hr == S_OK, "got %#lx\n", hr );
    SysFreeString( headers );

    hr = IWinHttpRequest_GetResponseHeader( req, connection, &value );
    ok( hr == S_OK, "got %#lx\n", hr );
    SysFreeString( value );

    hr = IWinHttpRequest_SetRequestHeader( req, date, today );
    ok( hr == HRESULT_FROM_WIN32( ERROR_WINHTTP_CANNOT_CALL_AFTER_SEND ), "got %#lx\n", hr );

    hr = IWinHttpRequest_SetAutoLogonPolicy( req, AutoLogonPolicy_OnlyIfBypassProxy );
    ok( hr == S_OK, "got %#lx\n", hr );

    hr = IWinHttpRequest_Send( req, empty );
    ok( hr == S_OK, "got %#lx\n", hr );

    hr = IWinHttpRequest_Abort( req );
    ok( hr == S_OK, "got %#lx\n", hr );

    hr = IWinHttpRequest_Abort( req );
    ok( hr == S_OK, "got %#lx\n", hr );

    IWinHttpRequest_Release( req );

    pos.QuadPart = 0;
    hr = IStream_Seek( stream, pos, STREAM_SEEK_SET, NULL );
    ok( hr == S_OK, "got %#lx\n", hr );

    buf[0] = 0;
    count = 0xdeadbeef;
    hr = IStream_Read( stream, buf, 128, &count );
    ok( hr == S_OK, "got %#lx\n", hr );
    ok( count != 0xdeadbeef, "count not set\n" );
    ok( buf[0], "no data\n" );
    IStream_Release( stream );

    hr = CoCreateInstance( &CLSID_WinHttpRequest, NULL, CLSCTX_INPROC_SERVER, &IID_IWinHttpRequest, (void **)&req );
    ok( hr == S_OK, "got %#lx\n", hr );

    V_VT( &async ) = VT_I4;
    V_I4( &async ) = 1;
    hr = IWinHttpRequest_Open( req, method, url, async );
    ok( hr == S_OK, "got %#lx\n", hr );

    hr = IWinHttpRequest_Send( req, empty );
    ok( hr == S_OK, "got %#lx\n", hr );

    hr = IWinHttpRequest_WaitForResponse( req, timeout, &succeeded );
    ok( hr == S_OK, "got %#lx\n", hr );

    IWinHttpRequest_Release( req );

    SysFreeString( method );
    SysFreeString( url );
    SysFreeString( username );
    SysFreeString( password );
    SysFreeString( connection );
    SysFreeString( date );
    SysFreeString( today );
    VariantClear( &proxy_server );
    VariantClear( &bypass_list );

    hr = CoCreateInstance( &CLSID_WinHttpRequest, NULL, CLSCTX_INPROC_SERVER, &IID_IWinHttpRequest, (void **)&req );
    ok( hr == S_OK, "got %#lx\n", hr );

    url = SysAllocString( L"https://test.winehq.org:443" );
    method = SysAllocString( L"POST" );
    V_VT( &async ) = VT_BOOL;
    V_BOOL( &async ) = VARIANT_FALSE;
    hr = IWinHttpRequest_Open( req, method, url, async );
    ok( hr == S_OK, "got %#lx\n", hr );
    SysFreeString( method );
    SysFreeString( url );

    hr = IWinHttpRequest_Send( req, empty );
    ok( hr == S_OK ||
        hr == HRESULT_FROM_WIN32( ERROR_WINHTTP_INVALID_SERVER_RESPONSE ) ||
        hr == HRESULT_FROM_WIN32( ERROR_WINHTTP_SECURE_CHANNEL_ERROR ) /* win7 */, "got %#lx\n", hr );
    if (hr != S_OK) goto done;

    hr = IWinHttpRequest_get_ResponseText( req, &response );
    ok( hr == S_OK, "got %#lx\n", hr );
    ok( !memcmp(response, data_start, sizeof(data_start)), "got %s\n", wine_dbgstr_wn(response, 32) );
    SysFreeString( response );

    IWinHttpRequest_Release( req );

    hr = CoCreateInstance( &CLSID_WinHttpRequest, NULL, CLSCTX_INPROC_SERVER, &IID_IWinHttpRequest, (void **)&req );
    ok( hr == S_OK, "got %#lx\n", hr );

    sprintf( buf, "http://localhost:%d/auth", port );
    MultiByteToWideChar( CP_ACP, 0, buf, -1, bufW, ARRAY_SIZE( bufW ));
    url = SysAllocString( bufW );
    method = SysAllocString( L"POST" );
    V_VT( &async ) = VT_BOOL;
    V_BOOL( &async ) = VARIANT_FALSE;
    hr = IWinHttpRequest_Open( req, method, url, async );
    ok( hr == S_OK, "got %#lx\n", hr );
    SysFreeString( method );
    SysFreeString( url );

    hr = IWinHttpRequest_get_Status( req, &status );
    ok( hr == HRESULT_FROM_WIN32( ERROR_WINHTTP_CANNOT_CALL_BEFORE_SEND ), "got %#lx\n", hr );

    V_VT( &data ) = VT_BSTR;
    V_BSTR( &data ) = SysAllocString( L"testdata\x80" );
    hr = IWinHttpRequest_Send( req, data );
    ok( hr == S_OK, "got %#lx\n", hr );
    SysFreeString( V_BSTR( &data ) );

    hr = IWinHttpRequest_get_ResponseText( req, &response );
    ok( hr == S_OK, "got %#lx\n", hr );
    ok( !memcmp( response, L"Unauthorized", sizeof(L"Unauthorized") ), "got %s\n", wine_dbgstr_w(response) );
    SysFreeString( response );

    status = 0xdeadbeef;
    hr = IWinHttpRequest_get_Status( req, &status );
    ok( hr == S_OK, "got %#lx\n", hr );
    ok( status == HTTP_STATUS_DENIED, "got %lu\n", status );

done:
    IWinHttpRequest_Release( req );
    CoUninitialize();
}

static void request_get_property(IWinHttpRequest *request, int property, VARIANT *ret)
{
    DISPPARAMS params;
    VARIANT arg;
    HRESULT hr;

    memset(&params, 0, sizeof(params));
    params.cNamedArgs = 0;
    params.rgdispidNamedArgs = NULL;
    params.cArgs = 1;
    params.rgvarg = &arg;
    VariantInit(&arg);
    V_VT(&arg) = VT_I4;
    V_I4(&arg) = property;
    VariantInit(ret);
    hr = IWinHttpRequest_Invoke(request, DISPID_HTTPREQUEST_OPTION, &IID_NULL, 0,
                                DISPATCH_PROPERTYGET, &params, ret, NULL, NULL);
    ok(hr == S_OK, "error %#lx\n", hr);
}

static void test_IWinHttpRequest_Invoke(void)
{
    WCHAR openW[] = {'O','p','e','n',0};
    WCHAR optionW[] = {'O','p','t','i','o','n',0};
    OLECHAR *open = openW, *option = optionW;
    BSTR utf8;
    CLSID clsid;
    IWinHttpRequest *request;
    IDispatch *dispatch;
    DISPID id;
    DISPPARAMS params;
    VARIANT arg[3], ret;
    UINT err;
    BOOL bret;
    HRESULT hr;

    CoInitialize(NULL);

    hr = CLSIDFromProgID(L"WinHttp.WinHttpRequest.5.1", &clsid);
    ok(hr == S_OK, "CLSIDFromProgID error %#lx\n", hr);
    bret = IsEqualIID(&clsid, &CLSID_WinHttpRequest);
    ok(bret || broken(!bret) /* win2003 */, "not expected %s\n", wine_dbgstr_guid(&clsid));

    hr = CoCreateInstance(&CLSID_WinHttpRequest, 0, CLSCTX_INPROC_SERVER, &IID_IUnknown, (void **)&request);
    ok(hr == S_OK, "error %#lx\n", hr);

    hr = IWinHttpRequest_QueryInterface(request, &IID_IDispatch, (void **)&dispatch);
    ok(hr == S_OK, "error %#lx\n", hr);
    IDispatch_Release(dispatch);

    hr = IWinHttpRequest_GetIDsOfNames(request, &IID_NULL, &open, 1, 0x0409, &id);
    ok(hr == S_OK, "error %#lx\n", hr);
    ok(id == DISPID_HTTPREQUEST_OPEN, "expected DISPID_HTTPREQUEST_OPEN, got %lu\n", id);

    hr = IWinHttpRequest_GetIDsOfNames(request, &IID_NULL, &option, 1, 0x0409, &id);
    ok(hr == S_OK, "error %#lx\n", hr);
    ok(id == DISPID_HTTPREQUEST_OPTION, "expected DISPID_HTTPREQUEST_OPTION, got %lu\n", id);

    request_get_property(request, WinHttpRequestOption_URLCodePage, &ret);
    ok(V_VT(&ret) == VT_I4, "expected VT_I4, got %#x\n", V_VT(&ret));
    ok(V_I4(&ret) == CP_UTF8, "expected CP_UTF8, got %ld\n", V_I4(&ret));

    memset(&params, 0, sizeof(params));
    params.cArgs = 2;
    params.cNamedArgs = 0;
    params.rgvarg = arg;
    V_VT(&arg[0]) = VT_I4;
    V_I4(&arg[0]) = 1252;
    V_VT(&arg[1]) = VT_R8;
    V_R8(&arg[1]) = 2.0; /* WinHttpRequestOption_URLCodePage */
    VariantInit(&ret);
    hr = IWinHttpRequest_Invoke(request, DISPID_HTTPREQUEST_OPTION, &IID_NULL, 0,
                                DISPATCH_METHOD, &params, NULL, NULL, &err);
    ok(hr == S_OK, "error %#lx\n", hr);

    request_get_property(request, WinHttpRequestOption_URLCodePage, &ret);
    ok(V_VT(&ret) == VT_I4, "expected VT_I4, got %#x\n", V_VT(&ret));
    ok(V_I4(&ret) == CP_UTF8, "expected CP_UTF8, got %ld\n", V_I4(&ret));

    memset(&params, 0, sizeof(params));
    params.cArgs = 2;
    params.cNamedArgs = 0;
    params.rgvarg = arg;
    V_VT(&arg[0]) = VT_I4;
    V_I4(&arg[0]) = 1252;
    V_VT(&arg[1]) = VT_R8;
    V_R8(&arg[1]) = 2.0; /* WinHttpRequestOption_URLCodePage */
    VariantInit(&ret);
    hr = IWinHttpRequest_Invoke(request, DISPID_HTTPREQUEST_OPTION, &IID_NULL, 0,
                                DISPATCH_METHOD | DISPATCH_PROPERTYPUT, &params, NULL, NULL, &err);
    ok(hr == S_OK, "error %#lx\n", hr);

    request_get_property(request, WinHttpRequestOption_URLCodePage, &ret);
    ok(V_VT(&ret) == VT_I4, "expected VT_I4, got %#x\n", V_VT(&ret));
    ok(V_I4(&ret) == CP_UTF8, "expected CP_UTF8, got %ld\n", V_I4(&ret));

    memset(&params, 0, sizeof(params));
    params.cArgs = 2;
    params.cNamedArgs = 0;
    params.rgvarg = arg;
    V_VT(&arg[0]) = VT_I4;
    V_I4(&arg[0]) = 1252;
    V_VT(&arg[1]) = VT_R8;
    V_R8(&arg[1]) = 2.0; /* WinHttpRequestOption_URLCodePage */
    VariantInit(&ret);
    hr = IWinHttpRequest_Invoke(request, DISPID_HTTPREQUEST_OPTION, &IID_NULL, 0,
                                DISPATCH_PROPERTYPUT, &params, NULL, NULL, &err);
    ok(hr == S_OK, "error %#lx\n", hr);

    request_get_property(request, WinHttpRequestOption_URLCodePage, &ret);
    ok(V_VT(&ret) == VT_I4, "expected VT_I4, got %#x\n", V_VT(&ret));
    ok(V_I4(&ret) == 1252, "expected 1252, got %ld\n", V_I4(&ret));

    memset(&params, 0, sizeof(params));
    params.cArgs = 2;
    params.cNamedArgs = 0;
    params.rgvarg = arg;
    V_VT(&arg[0]) = VT_BSTR;
    utf8 = SysAllocString(L"UTF-8");
    V_BSTR(&arg[0]) = utf8;
    V_VT(&arg[1]) = VT_R8;
    V_R8(&arg[1]) = 2.0; /* WinHttpRequestOption_URLCodePage */
    hr = IWinHttpRequest_Invoke(request, id, &IID_NULL, 0, DISPATCH_METHOD, &params, NULL, NULL, &err);
    ok(hr == S_OK, "error %#lx\n", hr);

    request_get_property(request, WinHttpRequestOption_URLCodePage, &ret);
    ok(V_VT(&ret) == VT_I4, "expected VT_I4, got %#x\n", V_VT(&ret));
    ok(V_I4(&ret) == 1252, "expected 1252, got %ld\n", V_I4(&ret));

    VariantInit(&ret);
    hr = IWinHttpRequest_Invoke(request, id, &IID_NULL, 0, DISPATCH_METHOD, &params, &ret, NULL, &err);
    ok(hr == S_OK, "error %#lx\n", hr);

    request_get_property(request, WinHttpRequestOption_URLCodePage, &ret);
    ok(V_VT(&ret) == VT_I4, "expected VT_I4, got %#x\n", V_VT(&ret));
    ok(V_I4(&ret) == 1252, "expected 1252, got %ld\n", V_I4(&ret));

    VariantInit(&ret);
    hr = IWinHttpRequest_Invoke(request, id, &IID_NULL, 0, DISPATCH_PROPERTYPUT, &params, &ret, NULL, &err);
    ok(hr == S_OK, "error %#lx\n", hr);

    request_get_property(request, WinHttpRequestOption_URLCodePage, &ret);
    ok(V_VT(&ret) == VT_I4, "expected VT_I4, got %#x\n", V_VT(&ret));
    ok(V_I4(&ret) == CP_UTF8, "expected CP_UTF8, got %ld\n", V_I4(&ret));

    hr = IWinHttpRequest_Invoke(request, DISPID_HTTPREQUEST_OPTION, &IID_NULL, 0, DISPATCH_PROPERTYPUT, &params, NULL, NULL, NULL);
    ok(hr == S_OK, "error %#lx\n", hr);

    hr = IWinHttpRequest_Invoke(request, 255, &IID_NULL, 0, DISPATCH_PROPERTYPUT, &params, NULL, NULL, NULL);
    ok(hr == DISP_E_MEMBERNOTFOUND, "error %#lx\n", hr);

    VariantInit(&ret);
    hr = IWinHttpRequest_Invoke(request, DISPID_HTTPREQUEST_OPTION, &IID_IUnknown, 0, DISPATCH_PROPERTYPUT, &params, &ret, NULL, &err);
    ok(hr == DISP_E_UNKNOWNINTERFACE, "error %#lx\n", hr);

    VariantInit(&ret);
    if (0) /* crashes */
        hr = IWinHttpRequest_Invoke(request, DISPID_HTTPREQUEST_OPTION, &IID_NULL, 0, DISPATCH_PROPERTYPUT, NULL, &ret, NULL, &err);

    params.cArgs = 1;
    hr = IWinHttpRequest_Invoke(request, DISPID_HTTPREQUEST_OPTION, &IID_NULL, 0, DISPATCH_PROPERTYPUT, &params, &ret, NULL, &err);
    ok(hr == DISP_E_TYPEMISMATCH, "error %#lx\n", hr);

    VariantInit(&arg[2]);
    params.cArgs = 3;
    hr = IWinHttpRequest_Invoke(request, DISPID_HTTPREQUEST_OPTION, &IID_NULL, 0, DISPATCH_PROPERTYPUT, &params, &ret, NULL, &err);
    todo_wine
    ok(hr == S_OK, "error %#lx\n", hr);

    VariantInit(&arg[0]);
    VariantInit(&arg[1]);
    VariantInit(&arg[2]);

    params.cArgs = 1;
    V_VT(&arg[0]) = VT_I4;
    V_I4(&arg[0]) = WinHttpRequestOption_URLCodePage;
    hr = IWinHttpRequest_Invoke(request, DISPID_HTTPREQUEST_OPTION, &IID_NULL, 0, DISPATCH_PROPERTYGET, &params, NULL, NULL, NULL);
    ok(hr == S_OK, "error %#lx\n", hr);

    V_VT(&ret) = 0xdead;
    V_I4(&ret) = 0xbeef;
    hr = IWinHttpRequest_Invoke(request, DISPID_HTTPREQUEST_OPTION, &IID_NULL, 0, DISPATCH_METHOD|DISPATCH_PROPERTYGET, &params, &ret, NULL, NULL);
    ok(hr == S_OK, "error %#lx\n", hr);
    ok(V_VT(&ret) == VT_I4, "expected VT_I4, got %#x\n", V_VT(&ret));
    ok(V_I4(&ret) == CP_UTF8, "expected CP_UTF8, got %ld\n", V_I4(&ret));

    V_VT(&ret) = 0xdead;
    V_I4(&ret) = 0xbeef;
    hr = IWinHttpRequest_Invoke(request, DISPID_HTTPREQUEST_OPTION, &IID_NULL, 0, DISPATCH_METHOD, &params, &ret, NULL, NULL);
    ok(hr == S_OK, "error %#lx\n", hr);
    ok(V_VT(&ret) == VT_I4, "expected VT_I4, got %#x\n", V_VT(&ret));
    ok(V_I4(&ret) == CP_UTF8, "expected CP_UTF8, got %ld\n", V_I4(&ret));

    hr = IWinHttpRequest_Invoke(request, DISPID_HTTPREQUEST_OPTION, &IID_NULL, 0, DISPATCH_METHOD|DISPATCH_PROPERTYGET, &params, NULL, NULL, NULL);
    ok(hr == S_OK, "error %#lx\n", hr);

    V_VT(&ret) = 0xdead;
    V_I4(&ret) = 0xbeef;
    hr = IWinHttpRequest_Invoke(request, DISPID_HTTPREQUEST_OPTION, &IID_NULL, 0, 0, &params, &ret, NULL, NULL);
    ok(hr == S_OK, "error %#lx\n", hr);
    ok(V_VT(&ret) == VT_EMPTY, "expected VT_EMPTY, got %#x\n", V_VT(&ret));
    ok(V_I4(&ret) == 0xbeef || V_I4(&ret) == 0 /* Win8 */, "expected 0xdead, got %ld\n", V_I4(&ret));

    hr = IWinHttpRequest_Invoke(request, DISPID_HTTPREQUEST_OPTION, &IID_NULL, 0, 0, &params, NULL, NULL, NULL);
    ok(hr == S_OK, "error %#lx\n", hr);

    hr = IWinHttpRequest_Invoke(request, DISPID_HTTPREQUEST_OPTION, &IID_IUnknown, 0, DISPATCH_PROPERTYGET, &params, NULL, NULL, NULL);
    ok(hr == DISP_E_UNKNOWNINTERFACE, "error %#lx\n", hr);

    params.cArgs = 2;
    hr = IWinHttpRequest_Invoke(request, DISPID_HTTPREQUEST_OPTION, &IID_NULL, 0, DISPATCH_PROPERTYGET, &params, NULL, NULL, NULL);
    todo_wine
    ok(hr == S_OK, "error %#lx\n", hr);

    params.cArgs = 0;
    hr = IWinHttpRequest_Invoke(request, DISPID_HTTPREQUEST_OPTION, &IID_NULL, 0, DISPATCH_PROPERTYGET, &params, NULL, NULL, NULL);
    ok(hr == DISP_E_PARAMNOTFOUND, "error %#lx\n", hr);

    SysFreeString(utf8);

    params.cArgs = 1;
    V_VT(&arg[0]) = VT_I4;
    V_I4(&arg[0]) = AutoLogonPolicy_Never;
    VariantInit(&ret);
    hr = IWinHttpRequest_Invoke(request, DISPID_HTTPREQUEST_SETAUTOLOGONPOLICY, &IID_NULL, 0,
                                DISPATCH_METHOD, &params, &ret, NULL, NULL);
    ok(hr == S_OK, "error %#lx\n", hr);

    IWinHttpRequest_Release(request);

    CoUninitialize();
}

static void test_WinHttpDetectAutoProxyConfigUrl(void)
{
    BOOL ret;
    WCHAR *url;
    DWORD error;

    SetLastError(0xdeadbeef);
    ret = WinHttpDetectAutoProxyConfigUrl( 0, NULL );
    error = GetLastError();
    ok( !ret, "expected failure\n" );
    ok( error == ERROR_INVALID_PARAMETER, "got %lu\n", error );

    url = NULL;
    SetLastError(0xdeadbeef);
    ret = WinHttpDetectAutoProxyConfigUrl( 0, &url );
    error = GetLastError();
    ok( !ret, "expected failure\n" );
    ok( error == ERROR_INVALID_PARAMETER, "got %lu\n", error );

    SetLastError(0xdeadbeef);
    ret = WinHttpDetectAutoProxyConfigUrl( WINHTTP_AUTO_DETECT_TYPE_DNS_A, NULL );
    error = GetLastError();
    ok( !ret, "expected failure\n" );
    ok( error == ERROR_INVALID_PARAMETER, "got %lu\n", error );

    url = (WCHAR *)0xdeadbeef;
    SetLastError(0xdeadbeef);
    ret = WinHttpDetectAutoProxyConfigUrl( WINHTTP_AUTO_DETECT_TYPE_DNS_A, &url );
    error = GetLastError();
    if (!ret)
    {
        ok( error == ERROR_WINHTTP_AUTODETECTION_FAILED, "got %lu\n", error );
        ok( !url || broken(url == (WCHAR *)0xdeadbeef), "got %p\n", url );
    }
    else
    {
        trace("%s\n", wine_dbgstr_w(url));
        GlobalFree( url );
    }

    url = (WCHAR *)0xdeadbeef;
    SetLastError(0xdeadbeef);
    ret = WinHttpDetectAutoProxyConfigUrl( WINHTTP_AUTO_DETECT_TYPE_DHCP, &url );
    error = GetLastError();
    if (!ret)
    {
        ok( error == ERROR_WINHTTP_AUTODETECTION_FAILED, "got %lu\n", error );
        ok( !url || broken(url == (WCHAR *)0xdeadbeef), "got %p\n", url );
    }
    else
    {
        ok( error == ERROR_SUCCESS, "got %lu\n", error );
        trace("%s\n", wine_dbgstr_w(url));
        GlobalFree( url );
    }
}

static void test_WinHttpGetIEProxyConfigForCurrentUser(void)
{
    BOOL ret;
    DWORD error;
    WINHTTP_CURRENT_USER_IE_PROXY_CONFIG cfg;

    memset( &cfg, 0, sizeof(cfg) );

    SetLastError(0xdeadbeef);
    ret = WinHttpGetIEProxyConfigForCurrentUser( NULL );
    error = GetLastError();
    ok( !ret, "expected failure\n" );
    ok( error == ERROR_INVALID_PARAMETER, "got %lu\n", error );

    SetLastError(0xdeadbeef);
    ret = WinHttpGetIEProxyConfigForCurrentUser( &cfg );
    error = GetLastError();
    ok( ret, "expected success\n" );
    ok( error == ERROR_SUCCESS || broken(error == ERROR_NO_TOKEN) /* < win7 */, "got %lu\n", error );

    trace("IEProxy.AutoDetect=%d\n", cfg.fAutoDetect);
    trace("IEProxy.AutoConfigUrl=%s\n", wine_dbgstr_w(cfg.lpszAutoConfigUrl));
    trace("IEProxy.Proxy=%s\n", wine_dbgstr_w(cfg.lpszProxy));
    trace("IEProxy.ProxyBypass=%s\n", wine_dbgstr_w(cfg.lpszProxyBypass));
    GlobalFree( cfg.lpszAutoConfigUrl );
    GlobalFree( cfg.lpszProxy );
    GlobalFree( cfg.lpszProxyBypass );
}

static void test_WinHttpGetProxyForUrl(int port)
{
    WCHAR pac_url[64];
    BOOL ret, old_winhttp = FALSE;
    DWORD error;
    HINTERNET session;
    WINHTTP_AUTOPROXY_OPTIONS options;
    WINHTTP_PROXY_INFO info;

    memset( &options, 0, sizeof(options) );

    SetLastError(0xdeadbeef);
    ret = WinHttpGetProxyForUrl( NULL, NULL, NULL, NULL );
    error = GetLastError();
    ok( !ret, "expected failure\n" );
    ok( error == ERROR_INVALID_HANDLE, "got %lu\n", error );

    session = WinHttpOpen( L"winetest", 0, NULL, NULL, 0 );
    ok( session != NULL, "failed to open session %lu\n", GetLastError() );

    SetLastError(0xdeadbeef);
    ret = WinHttpGetProxyForUrl( session, NULL, NULL, NULL );
    error = GetLastError();
    ok( !ret, "expected failure\n" );
    ok( error == ERROR_INVALID_PARAMETER, "got %lu\n", error );

    SetLastError(0xdeadbeef);
    ret = WinHttpGetProxyForUrl( session, L"", NULL, NULL );
    error = GetLastError();
    ok( !ret, "expected failure\n" );
    ok( error == ERROR_INVALID_PARAMETER, "got %lu\n", error );

    SetLastError(0xdeadbeef);
    ret = WinHttpGetProxyForUrl( session, L"http://winehq.org", NULL, NULL );
    error = GetLastError();
    ok( !ret, "expected failure\n" );
    ok( error == ERROR_INVALID_PARAMETER, "got %lu\n", error );

    SetLastError(0xdeadbeef);
    ret = WinHttpGetProxyForUrl( session, L"http://winehq.org", &options, &info );
    error = GetLastError();
    ok( !ret, "expected failure\n" );
    ok( error == ERROR_INVALID_PARAMETER, "got %lu\n", error );

    options.dwFlags = WINHTTP_AUTOPROXY_AUTO_DETECT;
    options.dwAutoDetectFlags = WINHTTP_AUTO_DETECT_TYPE_DNS_A;

    SetLastError(0xdeadbeef);
    ret = WinHttpGetProxyForUrl( session, L"http://winehq.org", &options, NULL );
    error = GetLastError();
    ok( !ret, "expected failure\n" );
    ok( error == ERROR_INVALID_PARAMETER, "got %lu\n", error );

    options.dwFlags = WINHTTP_AUTOPROXY_AUTO_DETECT;
    options.dwAutoDetectFlags = 0;

    SetLastError(0xdeadbeef);
    ret = WinHttpGetProxyForUrl( session, L"http://winehq.org", &options, &info );
    error = GetLastError();
    ok( !ret, "expected failure\n" );
    ok( error == ERROR_INVALID_PARAMETER, "got %lu\n", error );

    options.dwFlags = WINHTTP_AUTOPROXY_AUTO_DETECT | WINHTTP_AUTOPROXY_CONFIG_URL;
    options.dwAutoDetectFlags = WINHTTP_AUTO_DETECT_TYPE_DNS_A;

    SetLastError(0xdeadbeef);
    ret = WinHttpGetProxyForUrl( session, L"http://winehq.org", &options, &info );
    error = GetLastError();
    ok( !ret, "expected failure\n" );
    ok( error == ERROR_INVALID_PARAMETER, "got %lu\n", error );

    options.dwFlags = WINHTTP_AUTOPROXY_AUTO_DETECT;
    options.dwAutoDetectFlags = WINHTTP_AUTO_DETECT_TYPE_DNS_A;

    memset( &info, 0, sizeof(info) );
    SetLastError(0xdeadbeef);
    ret = WinHttpGetProxyForUrl( session, L"http://winehq.org", &options, &info );
    error = GetLastError();
    if (ret)
    {
        ok( error == ERROR_SUCCESS, "got %lu\n", error );
        trace("Proxy.AccessType=%lu\n", info.dwAccessType);
        trace("Proxy.Proxy=%s\n", wine_dbgstr_w(info.lpszProxy));
        trace("Proxy.ProxyBypass=%s\n", wine_dbgstr_w(info.lpszProxyBypass));
        GlobalFree( info.lpszProxy );
        GlobalFree( info.lpszProxyBypass );

        ret = WinHttpGetProxyForUrl( session, L"http:", &options, &info );
        ok( !ret, "expected failure\n" );
    }

    options.dwFlags = WINHTTP_AUTOPROXY_CONFIG_URL;
    options.dwAutoDetectFlags = 0;
    options.lpszAutoConfigUrl = L"http://wpad/wpad.dat";

    memset( &info, 0, sizeof(info) );
    ret = WinHttpGetProxyForUrl( session, L"http://winehq.org", &options, &info );
    if (ret)
    {
        trace("Proxy.AccessType=%lu\n", info.dwAccessType);
        trace("Proxy.Proxy=%s\n", wine_dbgstr_w(info.lpszProxy));
        trace("Proxy.ProxyBypass=%s\n", wine_dbgstr_w(info.lpszProxyBypass));
        GlobalFree( info.lpszProxy );
        GlobalFree( info.lpszProxyBypass );
    }

    options.dwFlags = WINHTTP_AUTOPROXY_AUTO_DETECT|WINHTTP_AUTOPROXY_CONFIG_URL;
    options.dwAutoDetectFlags = WINHTTP_AUTO_DETECT_TYPE_DHCP|WINHTTP_AUTO_DETECT_TYPE_DNS_A;
    options.lpszAutoConfigUrl = L"http://wpad/wpad.dat";

    SetLastError(0xdeadbeef);
    memset( &info, 0, sizeof(info) );
    ret = WinHttpGetProxyForUrl( session, L"http://winehq.org", &options, &info );
    error = GetLastError();
    ok( error != ERROR_INVALID_PARAMETER, "got ERROR_INVALID_PARAMETER\n" );
    if (ret)
    {
        GlobalFree( info.lpszProxy );
        GlobalFree( info.lpszProxyBypass );
    }

    options.dwFlags = WINHTTP_AUTOPROXY_AUTO_DETECT;
    options.dwAutoDetectFlags = WINHTTP_AUTO_DETECT_TYPE_DHCP|WINHTTP_AUTO_DETECT_TYPE_DNS_A;

    ret = WinHttpGetProxyForUrl( session, L"http:", &options, &info );
    ok( !ret, "expected failure\n" );

    swprintf(pac_url, ARRAY_SIZE(pac_url), L"http://localhost:%d/proxy.pac?ver=1", port);
    options.dwFlags = WINHTTP_AUTOPROXY_CONFIG_URL | WINHTTP_AUTOPROXY_NO_CACHE_SVC;
    options.dwAutoDetectFlags = 0;
    options.lpszAutoConfigUrl = pac_url;

    ret = WinHttpGetProxyForUrl( session, L"HTTP://WINEHQ.ORG/Test.html", &options, &info);
    if (!ret)
    {
        old_winhttp = TRUE;
        options.dwFlags &= ~WINHTTP_AUTOPROXY_NO_CACHE_SVC;
        ret = WinHttpGetProxyForUrl( session, L"HTTP://WINEHQ.ORG/Test.html", &options, &info);
    }
    ok(ret, "expected success\n" );
    ok(info.dwAccessType == WINHTTP_ACCESS_TYPE_NAMED_PROXY,
            "info.dwAccessType = %lu\n", info.dwAccessType);
    ok(!wcscmp(info.lpszProxy, L"http___WINEHQ.ORG_Test.html_WINEHQ.ORG:8080") ||
            broken(old_winhttp && !wcscmp(info.lpszProxy, L"HTTP___WINEHQ.ORG_Test.html_WINEHQ.ORG:8080")),
            "info.Proxy = %s\n", wine_dbgstr_w(info.lpszProxy));
    ok(!info.lpszProxyBypass, "info.ProxyBypass = %s\n",
            wine_dbgstr_w(info.lpszProxyBypass));
    GlobalFree( info.lpszProxy );

    options.dwFlags |= WINHTTP_AUTOPROXY_HOST_LOWERCASE;

    ret = WinHttpGetProxyForUrl( session, L"HTTP://WINEHQ.ORG/Test.html", &options, &info);
    ok(ret, "expected success\n" );
    ok(info.dwAccessType == WINHTTP_ACCESS_TYPE_NAMED_PROXY,
            "info.dwAccessType = %lu\n", info.dwAccessType);
    ok(!wcscmp(info.lpszProxy, L"http___winehq.org_Test.html_winehq.org:8080") ||
            broken(old_winhttp && !wcscmp(info.lpszProxy, L"HTTP___winehq.org_Test.html_winehq.org:8080")),
            "info.Proxy = %s\n", wine_dbgstr_w(info.lpszProxy));
    ok(!info.lpszProxyBypass, "info.ProxyBypass = %s\n",
            wine_dbgstr_w(info.lpszProxyBypass));
    GlobalFree( info.lpszProxy );

    if (!old_winhttp)
    {
        options.dwFlags |= WINHTTP_AUTOPROXY_HOST_KEEPCASE;

        ret = WinHttpGetProxyForUrl( session, L"HTTP://WINEHQ.ORG/Test.html", &options, &info);
        ok(ret, "expected success\n" );
        ok(info.dwAccessType == WINHTTP_ACCESS_TYPE_NAMED_PROXY,
                "info.dwAccessType = %lu\n", info.dwAccessType);
        ok(!wcscmp(info.lpszProxy, L"http___WINEHQ.ORG_Test.html_WINEHQ.ORG:8080"),
                "info.Proxy = %s\n", wine_dbgstr_w(info.lpszProxy));
        ok(!info.lpszProxyBypass, "info.ProxyBypass = %s\n",
                wine_dbgstr_w(info.lpszProxyBypass));
        GlobalFree( info.lpszProxy );
    }

    WinHttpCloseHandle( session );
}

static void test_chunked_read(void)
{
    WCHAR header[32];
    DWORD len, err;
    HINTERNET ses, con = NULL, req = NULL;
    BOOL ret;

    trace( "starting chunked read test\n" );

    ses = WinHttpOpen( L"winetest", 0, NULL, NULL, 0 );
    ok( ses != NULL, "WinHttpOpen failed with error %lu\n", GetLastError() );
    if (!ses) goto done;

    con = WinHttpConnect( ses, L"test.winehq.org", 0, 0 );
    ok( con != NULL, "WinHttpConnect failed with error %lu\n", GetLastError() );
    if (!con) goto done;

    req = WinHttpOpenRequest( con, NULL, L"/tests/chunked", NULL, NULL, NULL, 0 );
    ok( req != NULL, "WinHttpOpenRequest failed with error %lu\n", GetLastError() );
    if (!req) goto done;

    ret = WinHttpSendRequest( req, NULL, 0, NULL, 0, 0, 0 );
    err = GetLastError();
    if (!ret && (err == ERROR_WINHTTP_CANNOT_CONNECT || err == ERROR_WINHTTP_TIMEOUT))
    {
        skip("connection failed, skipping\n");
        goto done;
    }
    ok( ret, "WinHttpSendRequest failed with error %lu\n", GetLastError() );
    if (!ret) goto done;

    ret = WinHttpReceiveResponse( req, NULL );
    ok( ret, "WinHttpReceiveResponse failed with error %lu\n", GetLastError() );
    if (!ret) goto done;

    header[0] = 0;
    len = sizeof(header);
    ret = WinHttpQueryHeaders( req, WINHTTP_QUERY_TRANSFER_ENCODING, NULL, header, &len, 0 );
    ok( ret, "failed to get TRANSFER_ENCODING header with error %lu\n", GetLastError() );
    ok( !lstrcmpW( header, L"chunked" ), "wrong transfer encoding %s\n", wine_dbgstr_w(header) );
    trace( "transfer encoding: %s\n", wine_dbgstr_w(header) );

    header[0] = 0;
    len = sizeof(header);
    SetLastError( 0xdeadbeef );
    ret = WinHttpQueryHeaders( req, WINHTTP_QUERY_CONTENT_LENGTH, NULL, &header, &len, 0 );
    ok( !ret, "unexpected CONTENT_LENGTH header %s\n", wine_dbgstr_w(header) );
    ok( GetLastError() == ERROR_WINHTTP_HEADER_NOT_FOUND, "wrong error %lu\n", GetLastError() );

    trace( "entering query loop\n" );
    for (;;)
    {
        len = 0xdeadbeef;
        ret = WinHttpQueryDataAvailable( req, &len );
        ok( ret, "WinHttpQueryDataAvailable failed with error %lu\n", GetLastError() );
        if (ret) ok( len != 0xdeadbeef, "WinHttpQueryDataAvailable return wrong length\n" );
        trace( "got %lu available\n", len );
        if (len)
        {
            DWORD bytes_read;
            char *buf = HeapAlloc( GetProcessHeap(), 0, len + 1 );

            ret = WinHttpReadData( req, buf, len, &bytes_read );
            ok(ret, "WinHttpReadData failed: %lu\n", GetLastError());

            buf[bytes_read] = 0;
            trace( "WinHttpReadData -> %d %lu\n", ret, bytes_read );
            ok( len == bytes_read, "only got %lu of %lu available\n", bytes_read, len );
            ok( buf[bytes_read - 1] == '\n', "received partial line '%s'\n", buf );

            HeapFree( GetProcessHeap(), 0, buf );
            if (!bytes_read) break;
        }
        if (!len) break;
    }
    trace( "done\n" );

done:
    if (req) WinHttpCloseHandle( req );
    if (con) WinHttpCloseHandle( con );
    if (ses) WinHttpCloseHandle( ses );
}

static void test_max_http_automatic_redirects (void)
{
    HINTERNET session, request, connection;
    DWORD max_redirects, err, size;
    WCHAR url[128];
    BOOL ret;

    session = WinHttpOpen(L"winetest", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    ok(session != NULL, "WinHttpOpen failed to open session.\n");

    connection = WinHttpConnect (session, L"test.winehq.org", INTERNET_DEFAULT_HTTP_PORT, 0);
    ok(connection != NULL, "WinHttpConnect failed to open a connection, error: %lu\n", GetLastError());

    /* Test with 2 redirects (page will try to redirect 3 times) */
    request = WinHttpOpenRequest(connection, L"GET", L"tests/redirecttest.php?max=3", NULL, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_BYPASS_PROXY_CACHE);
    if (request == NULL && GetLastError() == ERROR_WINHTTP_NAME_NOT_RESOLVED)
    {
        skip("Network unreachable, skipping.\n");
        goto done;
    }
    ok(request != NULL, "WinHttpOpenRequest failed to open a request, error: %lu\n", GetLastError());
    if (!request) goto done;

    max_redirects = 2;
    ret = WinHttpSetOption(request, WINHTTP_OPTION_MAX_HTTP_AUTOMATIC_REDIRECTS, &max_redirects, sizeof(max_redirects));
    ok(ret, "WinHttpSetOption failed: %lu\n", GetLastError());

    ret = WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    err = GetLastError();
    if (!ret && (err == ERROR_WINHTTP_CANNOT_CONNECT || err == ERROR_WINHTTP_TIMEOUT))
    {
        skip("connection failed, skipping\n");
        goto done;
    }
    ok(ret == TRUE, "WinHttpSendRequest failed: %lu\n", GetLastError());

    url[0] = 0;
    size = sizeof(url);
    ret = WinHttpQueryOption(request, WINHTTP_OPTION_URL, url, &size);
    ok(ret, "got %lu\n", GetLastError());
    ok(!wcscmp(url, L"http://test.winehq.org/tests/redirecttest.php?max=3"), "got %s\n", wine_dbgstr_w(url));

    SetLastError(0xdeadbeef);
    ret = WinHttpReceiveResponse(request, NULL);
    ok(!ret, "WinHttpReceiveResponse succeeded, expected failure\n");
    ok(GetLastError() == ERROR_WINHTTP_REDIRECT_FAILED, "Expected ERROR_WINHTTP_REDIRECT_FAILED, got %lu\n", GetLastError());

    url[0] = 0;
    size = sizeof(url);
    ret = WinHttpQueryOption(request, WINHTTP_OPTION_URL, url, &size);
    ok(ret, "got %lu\n", GetLastError());
    ok(!wcscmp(url, L"http://test.winehq.org/tests/redirecttest.php?id=2&max=3") ||
       broken(!wcscmp(url, L"http://test.winehq.org/tests/redirecttest.php?id=1&max=3")) /* < Win10 1809 */,
       "got %s\n", wine_dbgstr_w(url));

 done:
    ret = WinHttpCloseHandle(request);
    ok(ret == TRUE, "WinHttpCloseHandle failed on closing request, got %d.\n", ret);
    ret = WinHttpCloseHandle(connection);
    ok(ret == TRUE, "WinHttpCloseHandle failed on closing connection, got %d.\n", ret);
    ret = WinHttpCloseHandle(session);
    ok(ret == TRUE, "WinHttpCloseHandle failed on closing session, got %d.\n", ret);
}

static const BYTE pfxdata[] =
{
    0x30, 0x82, 0x0b, 0x1d, 0x02, 0x01, 0x03, 0x30, 0x82, 0x0a, 0xe3, 0x06,
    0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x01, 0xa0, 0x82,
    0x0a, 0xd4, 0x04, 0x82, 0x0a, 0xd0, 0x30, 0x82, 0x0a, 0xcc, 0x30, 0x82,
    0x05, 0x07, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07,
    0x06, 0xa0, 0x82, 0x04, 0xf8, 0x30, 0x82, 0x04, 0xf4, 0x02, 0x01, 0x00,
    0x30, 0x82, 0x04, 0xed, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d,
    0x01, 0x07, 0x01, 0x30, 0x1c, 0x06, 0x0a, 0x2a, 0x86, 0x48, 0x86, 0xf7,
    0x0d, 0x01, 0x0c, 0x01, 0x06, 0x30, 0x0e, 0x04, 0x08, 0xac, 0x3e, 0x35,
    0xa8, 0xed, 0x0d, 0x50, 0x07, 0x02, 0x02, 0x08, 0x00, 0x80, 0x82, 0x04,
    0xc0, 0x5a, 0x62, 0x55, 0x25, 0xf6, 0x2c, 0xf1, 0x78, 0x6c, 0x63, 0x96,
    0x8a, 0xea, 0x04, 0x64, 0xb3, 0x99, 0x3b, 0x80, 0x50, 0x05, 0x37, 0x55,
    0xa3, 0x5e, 0x9f, 0x35, 0xc3, 0x3c, 0xdc, 0xf6, 0xc4, 0xc1, 0x39, 0xa2,
    0xd7, 0x50, 0xad, 0xf9, 0x29, 0x3c, 0x51, 0xea, 0x15, 0x20, 0x25, 0xd3,
    0x4d, 0x69, 0xdf, 0x10, 0xd8, 0x9d, 0x60, 0x78, 0x8a, 0x70, 0x44, 0x7f,
    0x01, 0x4f, 0x4a, 0xfa, 0xab, 0xfd, 0x46, 0x48, 0x96, 0x2b, 0x69, 0xfc,
    0x11, 0xf8, 0x3f, 0xd3, 0x79, 0x09, 0x75, 0x81, 0x47, 0xdf, 0xce, 0xfe,
    0x07, 0x2f, 0x0a, 0xd8, 0xac, 0x87, 0x14, 0x1f, 0x7b, 0x95, 0x70, 0xee,
    0x7e, 0x52, 0x90, 0x11, 0xd6, 0x69, 0xf4, 0xd5, 0x38, 0x85, 0xc9, 0xc1,
    0x07, 0x01, 0xe8, 0xbb, 0xfb, 0xe2, 0x08, 0xa8, 0xfa, 0xbf, 0xf0, 0x92,
    0x63, 0x1d, 0xbb, 0x2b, 0x45, 0x6f, 0xce, 0x97, 0x01, 0xd7, 0x95, 0xf0,
    0x9c, 0x9a, 0x6b, 0x73, 0x01, 0xbf, 0xf9, 0x3d, 0xc8, 0x2b, 0x86, 0x7a,
    0xd5, 0x65, 0x84, 0xd7, 0xff, 0xb2, 0xf9, 0x20, 0x52, 0x35, 0xc5, 0x60,
    0x33, 0x70, 0x1d, 0x2f, 0x26, 0x09, 0x1c, 0x22, 0x17, 0xd8, 0x08, 0x4e,
    0x69, 0x20, 0xe2, 0x71, 0xe4, 0x07, 0xb1, 0x48, 0x5f, 0x20, 0x08, 0x7a,
    0xbf, 0x65, 0x53, 0x23, 0x07, 0xf9, 0x6c, 0xde, 0x3e, 0x29, 0xbf, 0x6b,
    0xef, 0xbb, 0x6a, 0x5f, 0x79, 0xa1, 0x72, 0xa1, 0x10, 0x24, 0x80, 0xb4,
    0x44, 0xb8, 0xc9, 0xfc, 0xa3, 0x36, 0x7e, 0x23, 0x37, 0x58, 0xc6, 0x1e,
    0xe8, 0x42, 0x4d, 0xb5, 0xf5, 0x58, 0x93, 0x21, 0x38, 0xa2, 0xc4, 0xa9,
    0x01, 0x96, 0xf9, 0x61, 0xac, 0x55, 0xb3, 0x3d, 0xe4, 0x54, 0x8b, 0x6c,
    0xc3, 0x83, 0xff, 0x50, 0x87, 0x94, 0xe8, 0x35, 0x3c, 0x26, 0x0d, 0x20,
    0x8a, 0x25, 0x0e, 0xb6, 0x67, 0x78, 0x29, 0xc7, 0xbf, 0x76, 0x8e, 0x62,
    0x62, 0xc4, 0x50, 0xd6, 0xc5, 0x3c, 0xb4, 0x7a, 0x35, 0xbe, 0x53, 0x52,
    0xc4, 0xe4, 0x10, 0xb3, 0xe0, 0x73, 0xb0, 0xd1, 0xc1, 0x5a, 0x4f, 0x4e,
    0x64, 0x0d, 0x92, 0x51, 0x2d, 0x4d, 0xec, 0xb0, 0xc6, 0x40, 0x1b, 0x03,
    0x89, 0x7f, 0xc2, 0x2c, 0xe3, 0x2c, 0xbd, 0x8c, 0x9c, 0xd9, 0xe0, 0x08,
    0x59, 0xd3, 0xaf, 0x48, 0x56, 0x89, 0x60, 0x85, 0x76, 0xe0, 0xd8, 0x7c,
    0xcf, 0x02, 0x8f, 0xfd, 0xb2, 0x8f, 0x2b, 0x61, 0xcf, 0x28, 0x56, 0x8b,
    0x6b, 0x03, 0x2b, 0x2f, 0x83, 0x31, 0xa0, 0x1c, 0xd1, 0x6c, 0x87, 0x49,
    0xc4, 0x77, 0x55, 0x1f, 0x61, 0x45, 0x58, 0x88, 0x9f, 0x01, 0xc3, 0x63,
    0x62, 0x30, 0x35, 0xdf, 0x61, 0x74, 0x55, 0x63, 0x3f, 0xae, 0x41, 0xc1,
    0xb8, 0xf0, 0x9f, 0xab, 0x25, 0xad, 0x41, 0x5c, 0x1f, 0x00, 0x0d, 0xef,
    0xf0, 0xcf, 0xaf, 0x41, 0x23, 0xca, 0x8c, 0x38, 0xea, 0x5a, 0xe4, 0x8b,
    0xb4, 0x89, 0xd0, 0x76, 0x7f, 0x2b, 0x77, 0x8f, 0xe4, 0x44, 0xd5, 0x37,
    0xac, 0xc2, 0x09, 0x7e, 0x7e, 0x7e, 0x02, 0x5c, 0x27, 0x01, 0xcb, 0x4d,
    0xea, 0xb3, 0x97, 0x36, 0x35, 0xd2, 0x05, 0x3c, 0x4e, 0xb8, 0x04, 0x5c,
    0xb8, 0x95, 0x3f, 0xc6, 0xbf, 0xd4, 0x20, 0x01, 0xfb, 0xed, 0x37, 0x5a,
    0xad, 0x4c, 0x61, 0x93, 0xfe, 0x95, 0x7c, 0x34, 0x11, 0x15, 0x9d, 0x00,
    0x0b, 0x99, 0x69, 0xcb, 0x7e, 0xb9, 0x53, 0x46, 0x57, 0x39, 0x3f, 0x59,
    0x4b, 0x30, 0x8d, 0xfb, 0x84, 0x66, 0x2d, 0x06, 0xc9, 0x88, 0xa6, 0x18,
    0xd7, 0x36, 0xc6, 0xf6, 0xf7, 0x47, 0x85, 0x38, 0xc8, 0x3d, 0x37, 0xea,
    0x57, 0x4c, 0xb0, 0x7c, 0x95, 0x29, 0x84, 0xab, 0xbb, 0x19, 0x86, 0xc2,
    0xc5, 0x99, 0x01, 0x38, 0x6b, 0xf1, 0xd3, 0x1d, 0xa8, 0x02, 0xf9, 0x6f,
    0xaa, 0xf1, 0x57, 0xd0, 0x88, 0x68, 0x62, 0x5f, 0x9f, 0x7a, 0x63, 0xba,
    0x3a, 0xc9, 0x95, 0x11, 0x3c, 0xf9, 0xa1, 0xc1, 0x35, 0xfe, 0xd5, 0x12,
    0x49, 0x88, 0x0d, 0x5c, 0xe2, 0xd1, 0x15, 0x18, 0xfb, 0xd5, 0x7f, 0x19,
    0x3f, 0xaf, 0xa0, 0xcb, 0x31, 0x20, 0x9e, 0x03, 0x93, 0xa4, 0x66, 0xbd,
    0x83, 0xe8, 0x60, 0x34, 0x55, 0x0d, 0x97, 0x10, 0x23, 0x24, 0x7a, 0x45,
    0x36, 0xb4, 0xc4, 0xee, 0x60, 0x6f, 0xd8, 0x46, 0xc5, 0xac, 0x2b, 0xa9,
    0x18, 0x74, 0x83, 0x1e, 0xdf, 0x7c, 0x1a, 0x5a, 0xe8, 0x5f, 0x8b, 0x4f,
    0x9f, 0x40, 0x3e, 0x5e, 0xfb, 0xd3, 0x68, 0xac, 0x34, 0x62, 0x30, 0x23,
    0xb6, 0xbc, 0xdf, 0xbc, 0xc7, 0x25, 0xd2, 0x1b, 0x57, 0x33, 0xfb, 0x78,
    0x22, 0x21, 0x1e, 0x3a, 0xf6, 0x44, 0x18, 0x7e, 0x12, 0x36, 0x47, 0x58,
    0xd0, 0x59, 0x26, 0x98, 0x98, 0x95, 0xf4, 0xd1, 0xaa, 0x45, 0xaa, 0xe7,
    0xd1, 0xe6, 0x2d, 0x78, 0xf0, 0x8b, 0x1c, 0xfd, 0xf8, 0x50, 0x60, 0xa2,
    0x1e, 0x7f, 0xe3, 0x31, 0x77, 0x31, 0x58, 0x99, 0x0f, 0xda, 0x0e, 0xa3,
    0xc6, 0x7a, 0x30, 0x45, 0x55, 0x11, 0x91, 0x77, 0x41, 0x79, 0xd3, 0x56,
    0xb2, 0x07, 0x00, 0x61, 0xab, 0xec, 0x27, 0xc7, 0x9f, 0xfa, 0x89, 0x08,
    0xc2, 0x87, 0xcf, 0xe9, 0xdc, 0x9e, 0x29, 0x22, 0xfb, 0x23, 0x7f, 0x9d,
    0x89, 0xd5, 0x6e, 0x75, 0x20, 0xd8, 0x00, 0x5b, 0xc4, 0x94, 0xbb, 0xc5,
    0xb2, 0xba, 0x77, 0x2b, 0xf6, 0x3c, 0x88, 0xb0, 0x4c, 0x38, 0x46, 0x55,
    0xee, 0x8b, 0x03, 0x15, 0xbc, 0x0a, 0x1d, 0x47, 0x87, 0x44, 0xaf, 0xb1,
    0x2a, 0xa7, 0x4d, 0x08, 0xdf, 0x3b, 0x2d, 0x70, 0xa1, 0x67, 0x31, 0x76,
    0x6e, 0x6f, 0x40, 0x3b, 0x3b, 0xe8, 0xf9, 0xdf, 0x90, 0xa4, 0xce, 0x7f,
    0xb8, 0x2d, 0x69, 0xcb, 0x1c, 0x1e, 0x94, 0xcd, 0xb1, 0xd8, 0x43, 0x22,
    0xb8, 0x4f, 0x98, 0x92, 0x74, 0xb3, 0xde, 0xeb, 0x7a, 0xcb, 0xfa, 0xd0,
    0x36, 0xe4, 0x5d, 0xfa, 0xd3, 0xce, 0xf9, 0xba, 0x3e, 0x0f, 0x6c, 0xc3,
    0x5b, 0xb3, 0x81, 0x84, 0x6e, 0x5d, 0xc1, 0x21, 0x89, 0xec, 0x67, 0x9a,
    0xfd, 0x55, 0x20, 0xb0, 0x71, 0x53, 0xae, 0xf8, 0xa4, 0x8d, 0xd5, 0xe5,
    0x2d, 0x3a, 0xce, 0x89, 0x55, 0x8c, 0x4f, 0x3b, 0x37, 0x95, 0x4e, 0x15,
    0xbe, 0xe7, 0xd1, 0x7a, 0x36, 0x82, 0x45, 0x69, 0x7c, 0x27, 0x4f, 0xb9,
    0x4b, 0x7d, 0xcd, 0x59, 0xc8, 0xf4, 0x8b, 0x0f, 0x4f, 0x75, 0x23, 0xd3,
    0xd0, 0xc7, 0x10, 0x79, 0xc0, 0xf1, 0xac, 0x14, 0xf7, 0x0d, 0xc8, 0x5e,
    0xfc, 0xff, 0x1a, 0x2b, 0x10, 0x88, 0x7e, 0x7e, 0x2f, 0xfa, 0x7b, 0x9f,
    0x47, 0x23, 0x34, 0xfc, 0xf5, 0xde, 0xd9, 0xa3, 0x05, 0x99, 0x2a, 0x96,
    0x83, 0x3d, 0xa4, 0x7f, 0x6a, 0x66, 0x9b, 0xe7, 0xf1, 0x00, 0x4e, 0x9a,
    0xfc, 0x68, 0xd2, 0x74, 0x17, 0xba, 0xc9, 0xc8, 0x20, 0x39, 0xa1, 0xa8,
    0x85, 0xc6, 0x10, 0x2b, 0xab, 0x97, 0x34, 0x2d, 0x49, 0x68, 0x57, 0xb0,
    0x43, 0xee, 0x25, 0xbb, 0x35, 0x1b, 0x03, 0x99, 0xa3, 0x21, 0x68, 0x66,
    0x86, 0x3f, 0xc6, 0xfc, 0x49, 0xf0, 0xba, 0x5f, 0x00, 0xc6, 0xe3, 0x1c,
    0xb2, 0x9f, 0x16, 0x7f, 0xc7, 0x40, 0x4a, 0x9a, 0x39, 0xc1, 0x95, 0x69,
    0xa2, 0x87, 0xba, 0x58, 0xc6, 0xf2, 0xd6, 0x66, 0xa6, 0x4c, 0x6d, 0x29,
    0x9c, 0xa8, 0x6e, 0xa9, 0xd2, 0xe4, 0x54, 0x17, 0x89, 0xe2, 0x43, 0xf0,
    0xe1, 0x8b, 0x57, 0x84, 0x6c, 0x87, 0x63, 0x17, 0xbb, 0xf6, 0x33, 0x1b,
    0xe4, 0x34, 0x6a, 0x80, 0x70, 0x7b, 0x1b, 0xfd, 0xf8, 0x79, 0x28, 0xc8,
    0x3c, 0x8e, 0xa4, 0xd5, 0xb8, 0x96, 0x54, 0xd4, 0xec, 0x72, 0xe5, 0x40,
    0x8f, 0x56, 0xde, 0x82, 0x15, 0x72, 0x4d, 0xd8, 0x0c, 0x07, 0xea, 0xe6,
    0x44, 0xcd, 0x94, 0x73, 0x5c, 0x04, 0xe8, 0x8e, 0xb7, 0xc7, 0xc9, 0x29,
    0xdc, 0x04, 0xef, 0x7c, 0x31, 0x9b, 0x50, 0xbc, 0xea, 0x71, 0x1f, 0x28,
    0x22, 0xb6, 0x04, 0x53, 0x2e, 0x71, 0xc4, 0xf6, 0xbb, 0x88, 0x51, 0xee,
    0x3e, 0x76, 0x65, 0xb4, 0x4b, 0x1b, 0xa3, 0xec, 0x7b, 0xa7, 0x9d, 0x31,
    0x5d, 0xb8, 0x9f, 0xab, 0x6b, 0x54, 0x7d, 0xbd, 0xc1, 0x2c, 0x55, 0xb0,
    0x23, 0x8c, 0x06, 0x60, 0x01, 0x4f, 0x60, 0x85, 0x56, 0x7f, 0xfb, 0x99,
    0x0c, 0xdc, 0x8c, 0x09, 0x37, 0x46, 0x5b, 0x97, 0x5d, 0xe8, 0x31, 0x00,
    0x1b, 0x30, 0x9b, 0x02, 0x92, 0x29, 0xb5, 0x20, 0xce, 0x4b, 0x90, 0xfb,
    0x91, 0x07, 0x5a, 0xd3, 0xf5, 0xa0, 0xe6, 0x8f, 0xf8, 0x73, 0xc5, 0x4b,
    0xbb, 0xad, 0x2a, 0xeb, 0xa8, 0xb7, 0x68, 0x34, 0x36, 0x47, 0xd5, 0x4b,
    0x61, 0x89, 0x53, 0xe6, 0xb6, 0xb1, 0x07, 0xe4, 0x08, 0x2e, 0xed, 0x50,
    0xd4, 0x1e, 0xed, 0x7f, 0xbf, 0x35, 0x68, 0x04, 0x45, 0x72, 0x86, 0x71,
    0x15, 0x55, 0xdf, 0xe6, 0x30, 0xc0, 0x8b, 0x8a, 0xb0, 0x6c, 0xd0, 0x35,
    0x57, 0x8f, 0x04, 0x37, 0xbc, 0xe1, 0xb8, 0xbf, 0x27, 0x37, 0x3d, 0xd0,
    0xc8, 0x46, 0x67, 0x42, 0x51, 0x30, 0x82, 0x05, 0xbd, 0x06, 0x09, 0x2a,
    0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x01, 0xa0, 0x82, 0x05, 0xae,
    0x04, 0x82, 0x05, 0xaa, 0x30, 0x82, 0x05, 0xa6, 0x30, 0x82, 0x05, 0xa2,
    0x06, 0x0b, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x0c, 0x0a, 0x01,
    0x02, 0xa0, 0x82, 0x04, 0xee, 0x30, 0x82, 0x04, 0xea, 0x30, 0x1c, 0x06,
    0x0a, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x0c, 0x01, 0x03, 0x30,
    0x0e, 0x04, 0x08, 0x9f, 0xa4, 0x72, 0x2b, 0x6b, 0x0e, 0xcb, 0x9f, 0x02,
    0x02, 0x08, 0x00, 0x04, 0x82, 0x04, 0xc8, 0xe5, 0x35, 0xb9, 0x72, 0x28,
    0x20, 0x28, 0xad, 0xe3, 0x01, 0xd7, 0x0b, 0xe0, 0x4e, 0x36, 0xc3, 0x73,
    0x06, 0xd5, 0xf6, 0x75, 0x1a, 0x78, 0xb2, 0xd8, 0xf6, 0x5a, 0x85, 0x8e,
    0x50, 0xa3, 0x05, 0x49, 0x02, 0x2d, 0xf8, 0xa3, 0x2f, 0xe6, 0x02, 0x7a,
    0xd5, 0x0b, 0x1d, 0xf1, 0xd1, 0xe4, 0x16, 0xaa, 0x70, 0x2e, 0x34, 0xdb,
    0x56, 0xd9, 0x33, 0x94, 0x11, 0xaa, 0x60, 0xd4, 0xfa, 0x5b, 0xd1, 0xb3,
    0x2e, 0x86, 0x6a, 0x5a, 0x69, 0xdf, 0x11, 0x91, 0xb0, 0xca, 0x82, 0xff,
    0x63, 0xad, 0x6a, 0x0b, 0x90, 0xa6, 0xc7, 0x9b, 0xef, 0x9a, 0xf8, 0x96,
    0xec, 0xe4, 0xc4, 0xdf, 0x55, 0x4c, 0x12, 0x07, 0xab, 0x7c, 0x5c, 0x68,
    0x47, 0xf2, 0x92, 0xfb, 0x94, 0xab, 0xc3, 0x64, 0xd3, 0xfe, 0xb2, 0x16,
    0xb4, 0x78, 0x80, 0x52, 0xe9, 0x32, 0x39, 0x3b, 0x8d, 0x12, 0x91, 0x36,
    0xfd, 0xa1, 0x97, 0xc2, 0x0a, 0x4a, 0xf1, 0xb3, 0x8a, 0xe4, 0x01, 0xed,
    0x0a, 0xda, 0x2e, 0xa0, 0x38, 0xa9, 0x47, 0x3d, 0x3a, 0x64, 0x87, 0x06,
    0xc3, 0x83, 0x60, 0xaf, 0x84, 0xdb, 0x87, 0xff, 0x70, 0x61, 0x43, 0x7d,
    0x2d, 0x61, 0x9a, 0xf7, 0x0d, 0xca, 0x0c, 0x0f, 0xbe, 0x43, 0x5b, 0x99,
    0xe1, 0x90, 0x64, 0x1f, 0xa7, 0x1b, 0xa6, 0xa6, 0x5c, 0x13, 0x70, 0xa3,
    0xdb, 0xd7, 0xf0, 0xe8, 0x7a, 0xb0, 0xd1, 0x9b, 0x52, 0xa6, 0x4f, 0xd6,
    0xff, 0x54, 0x4d, 0xa6, 0x15, 0x05, 0x5c, 0xe9, 0x04, 0x6a, 0xc3, 0x49,
    0x12, 0x2f, 0x24, 0x03, 0xc3, 0x80, 0x06, 0xa6, 0x07, 0x8b, 0x96, 0xe7,
    0x39, 0x31, 0x6d, 0xd3, 0x1b, 0xa5, 0x45, 0x58, 0x04, 0xe7, 0x87, 0xdf,
    0x26, 0xfb, 0x1b, 0x9f, 0x92, 0x93, 0x32, 0x12, 0x9a, 0xc9, 0xe6, 0xcb,
    0x88, 0x14, 0x9f, 0x23, 0x0b, 0x52, 0xa2, 0xb8, 0x32, 0x6c, 0xa9, 0x33,
    0xa1, 0x17, 0xe8, 0x4a, 0xd4, 0x5c, 0x7d, 0xb3, 0xa3, 0x64, 0x86, 0x03,
    0x7c, 0x7c, 0x3f, 0x99, 0xdc, 0x21, 0x9f, 0x93, 0xc6, 0xb9, 0x1d, 0xe0,
    0x21, 0x79, 0x78, 0x35, 0xdc, 0x1e, 0x27, 0x3c, 0x73, 0x7f, 0x0f, 0xd6,
    0x4f, 0xde, 0xe9, 0xb4, 0xb7, 0xe3, 0xf5, 0x72, 0xce, 0x42, 0xf3, 0x91,
    0x5b, 0x84, 0xba, 0xbb, 0xae, 0xf0, 0x87, 0x0f, 0x50, 0xa4, 0x5e, 0x80,
    0x23, 0x57, 0x2b, 0xa0, 0xa3, 0xc3, 0x8a, 0x2f, 0xa8, 0x7a, 0x1a, 0x65,
    0x8f, 0x62, 0xf8, 0x3e, 0xe2, 0xcd, 0xbc, 0x63, 0x56, 0x8e, 0x77, 0xf3,
    0xf9, 0x69, 0x10, 0x57, 0xa8, 0xaf, 0x67, 0x2a, 0x9f, 0x7f, 0x7e, 0xeb,
    0x1d, 0x99, 0xa6, 0x67, 0xcd, 0x9e, 0x42, 0x2e, 0x5e, 0x4e, 0x61, 0x24,
    0xfa, 0xca, 0x2a, 0xeb, 0x62, 0x1f, 0xa3, 0x14, 0x0a, 0x06, 0x4b, 0x77,
    0x78, 0x77, 0x9b, 0xf1, 0x03, 0xcc, 0xb5, 0xfe, 0xfb, 0x7a, 0x77, 0xa6,
    0x82, 0x9f, 0xe5, 0xde, 0x9d, 0x0d, 0x4d, 0x37, 0xc6, 0x12, 0x73, 0x6d,
    0xea, 0xbb, 0x48, 0xf0, 0xd2, 0x81, 0xcc, 0x1a, 0x47, 0xfa, 0xa4, 0xd2,
    0xb2, 0x27, 0xa0, 0xfc, 0x30, 0x04, 0xdb, 0x05, 0xd3, 0x0b, 0xbc, 0x4d,
    0x7a, 0x99, 0xef, 0x7f, 0x26, 0x01, 0xd4, 0x07, 0x0b, 0x1e, 0x99, 0x06,
    0x3c, 0xde, 0x3d, 0x1c, 0x21, 0x82, 0x68, 0x46, 0x35, 0x38, 0x61, 0xea,
    0xd4, 0xc2, 0x65, 0x09, 0x39, 0x87, 0xb4, 0xd3, 0x5d, 0x3c, 0xa3, 0x79,
    0xe4, 0x01, 0x4e, 0xbf, 0x18, 0xba, 0x57, 0x3f, 0xdd, 0xea, 0x0a, 0x6b,
    0x99, 0xfb, 0x93, 0xfa, 0xab, 0xee, 0x08, 0xdf, 0x38, 0x23, 0xae, 0x8d,
    0xa8, 0x03, 0x13, 0xfe, 0x83, 0x88, 0xb0, 0xc2, 0xf9, 0x90, 0xa5, 0x1c,
    0x01, 0x6f, 0x71, 0x91, 0x42, 0x35, 0x81, 0x74, 0x71, 0x6c, 0xba, 0x86,
    0x48, 0xfe, 0x96, 0xd2, 0x88, 0x12, 0x36, 0x4e, 0xa6, 0x2f, 0xd1, 0xdb,
    0xfa, 0xbf, 0xdb, 0x84, 0x01, 0xfc, 0x7d, 0x7a, 0xac, 0x20, 0xae, 0xf5,
    0x95, 0xc9, 0xdc, 0x10, 0x5f, 0x4c, 0xae, 0x85, 0x01, 0x8b, 0xfe, 0x77,
    0x13, 0x01, 0xae, 0x39, 0x59, 0x7e, 0xbc, 0xfd, 0xc9, 0x42, 0xe4, 0x13,
    0x07, 0x3f, 0xa9, 0x74, 0xd9, 0xd5, 0xfc, 0xb9, 0x78, 0xbe, 0x97, 0xf5,
    0xe7, 0x36, 0x7f, 0xfa, 0x23, 0x30, 0xeb, 0xab, 0x92, 0xd3, 0xdc, 0x3f,
    0x7f, 0xc0, 0x77, 0x93, 0xf9, 0x88, 0xe3, 0x4e, 0x13, 0x53, 0x6d, 0x71,
    0x87, 0xe9, 0x24, 0x2b, 0xae, 0x26, 0xbf, 0x62, 0x51, 0x04, 0x42, 0xe1,
    0x13, 0x9d, 0xd8, 0x9f, 0x59, 0x87, 0x3f, 0xfc, 0x94, 0xff, 0xcf, 0x88,
    0x88, 0xe6, 0xeb, 0x6e, 0xc1, 0x96, 0x04, 0x27, 0xc8, 0xda, 0xfa, 0xe8,
    0x2e, 0xbb, 0x2c, 0x6e, 0xf4, 0xb4, 0x00, 0x7d, 0x8d, 0x3b, 0xef, 0x8b,
    0x18, 0xa9, 0x5f, 0x32, 0xa9, 0xf2, 0x3a, 0x7e, 0x65, 0x2d, 0x6e, 0x8d,
    0x75, 0x77, 0xf6, 0xa6, 0xd8, 0xf9, 0x6b, 0x51, 0xe6, 0x66, 0x52, 0x59,
    0x39, 0x97, 0x22, 0xda, 0xb2, 0xd6, 0x82, 0x5a, 0x6e, 0x61, 0x60, 0x16,
    0x48, 0x7b, 0xf1, 0xc3, 0x4d, 0x7f, 0x50, 0xfa, 0x4d, 0x58, 0x27, 0x30,
    0xc8, 0x96, 0xe0, 0x41, 0x4f, 0x6b, 0xeb, 0x88, 0xa2, 0x7a, 0xef, 0x8a,
    0x88, 0xc8, 0x50, 0x4b, 0x55, 0x66, 0xee, 0xbf, 0xc4, 0x01, 0x82, 0x4c,
    0xec, 0xde, 0x37, 0x64, 0xd6, 0x1e, 0xcf, 0x3e, 0x2e, 0xfe, 0x84, 0x68,
    0xbf, 0xa3, 0x68, 0x77, 0xa9, 0x03, 0xe4, 0xf8, 0xd7, 0xb2, 0x6e, 0xa3,
    0xc4, 0xc3, 0x36, 0x53, 0xf3, 0xdd, 0x7e, 0x4c, 0xf0, 0xe9, 0xb2, 0x44,
    0xe6, 0x60, 0x3d, 0x00, 0x9a, 0x08, 0xc3, 0x21, 0x17, 0x49, 0xda, 0x49,
    0xfb, 0x4c, 0x8b, 0xe9, 0x10, 0x66, 0xfe, 0xb7, 0xe0, 0xf9, 0xdd, 0xbf,
    0x41, 0xfe, 0x04, 0x9b, 0x7f, 0xe8, 0xd6, 0x2e, 0x4d, 0x0f, 0x7b, 0x10,
    0x73, 0x4c, 0xa1, 0x3e, 0x43, 0xb7, 0xcf, 0x94, 0x97, 0x7e, 0x24, 0xbb,
    0x87, 0xbf, 0x22, 0xb8, 0x3e, 0xeb, 0x9a, 0x3f, 0xe3, 0x86, 0xee, 0x21,
    0xbc, 0xf5, 0x44, 0xeb, 0x60, 0x2e, 0xe7, 0x8f, 0x89, 0xa4, 0x91, 0x61,
    0x28, 0x90, 0x85, 0x68, 0xe0, 0xa9, 0x62, 0x93, 0x86, 0x5a, 0x15, 0xbe,
    0xb2, 0x76, 0x83, 0xf2, 0x0f, 0x00, 0xc7, 0xb6, 0x57, 0xe9, 0x1f, 0x92,
    0x49, 0xfe, 0x50, 0x85, 0xbf, 0x39, 0x3d, 0xe4, 0x8b, 0x72, 0x2d, 0x49,
    0xbe, 0x05, 0x0a, 0x34, 0x56, 0x80, 0xc6, 0x1f, 0x46, 0x59, 0xc9, 0xfe,
    0x40, 0xfb, 0x78, 0x6d, 0x7a, 0xe5, 0x30, 0xe9, 0x81, 0x55, 0x75, 0x05,
    0x63, 0xd2, 0x22, 0xee, 0x2e, 0x6e, 0xb9, 0x18, 0xe5, 0x8a, 0x5a, 0x66,
    0xbd, 0x74, 0x30, 0xe3, 0x8b, 0x76, 0x22, 0x18, 0x1e, 0xef, 0x69, 0xe8,
    0x9d, 0x07, 0xa7, 0x9a, 0x87, 0x6c, 0x04, 0x4b, 0x74, 0x2b, 0xbe, 0x37,
    0x2f, 0x29, 0x9b, 0x60, 0x9d, 0x8b, 0x57, 0x55, 0x34, 0xca, 0x41, 0x25,
    0xae, 0x56, 0x92, 0x34, 0x1b, 0x9e, 0xbd, 0xfe, 0x74, 0xbd, 0x4e, 0x29,
    0xf0, 0x5e, 0x27, 0x94, 0xb0, 0x9e, 0x23, 0x9f, 0x4a, 0x0f, 0xa1, 0xdf,
    0xe7, 0xc4, 0xdb, 0xbe, 0x0f, 0x1a, 0x0b, 0x6c, 0xb0, 0xe1, 0x06, 0x7c,
    0x5a, 0x5b, 0x81, 0x1c, 0xb6, 0x12, 0xec, 0x6f, 0x3b, 0xbb, 0x84, 0x36,
    0xd5, 0x28, 0x16, 0xea, 0x51, 0xa8, 0x99, 0x24, 0x8f, 0xe7, 0xf8, 0xe9,
    0xce, 0xa1, 0x65, 0x96, 0x6f, 0x4e, 0x2f, 0xb7, 0x6f, 0x65, 0x39, 0xad,
    0xfd, 0x2e, 0xa0, 0x37, 0x32, 0x2f, 0xf3, 0x95, 0xa1, 0x3a, 0xa1, 0x9d,
    0x2c, 0x9e, 0xa1, 0x4b, 0x7e, 0xc9, 0x7e, 0x86, 0xaa, 0x16, 0x00, 0x82,
    0x1d, 0x36, 0xbf, 0x98, 0x0a, 0x82, 0x5b, 0xcc, 0xc4, 0x6a, 0xad, 0xa0,
    0x1f, 0x47, 0x98, 0xde, 0x8d, 0x68, 0x38, 0x3f, 0x33, 0xe2, 0x08, 0x3b,
    0x2a, 0x65, 0xd9, 0x2f, 0x53, 0x68, 0xb8, 0x78, 0xd0, 0x1d, 0xbb, 0x2a,
    0x73, 0x19, 0xba, 0x58, 0xea, 0xf1, 0x0a, 0xaa, 0xa6, 0xbe, 0x27, 0xd6,
    0x00, 0x6b, 0x4e, 0x43, 0x8e, 0x5b, 0x19, 0xc1, 0x37, 0x0f, 0xfb, 0x81,
    0x72, 0x10, 0xb6, 0x20, 0x32, 0xcd, 0xa2, 0x7c, 0x90, 0xd4, 0xf5, 0xcf,
    0x1c, 0xcb, 0x14, 0x24, 0x7a, 0x4d, 0xf5, 0xd5, 0xd9, 0xce, 0x6a, 0x64,
    0xc9, 0xd3, 0xa7, 0x36, 0x6f, 0x1d, 0xf1, 0xe9, 0x71, 0x6c, 0x3d, 0x02,
    0xa4, 0x62, 0xb1, 0x82, 0x5c, 0x13, 0x4b, 0x6b, 0x68, 0xe2, 0x31, 0xef,
    0xe4, 0x46, 0xfd, 0xe5, 0xa8, 0x29, 0xe9, 0x1e, 0xad, 0xff, 0x33, 0xdb,
    0x0b, 0xc0, 0x92, 0xb1, 0xef, 0xeb, 0xb3, 0x6f, 0x96, 0x7b, 0xdf, 0xcd,
    0x07, 0x19, 0x86, 0x60, 0x98, 0xcf, 0x95, 0xfe, 0x98, 0xdd, 0x29, 0xa6,
    0x35, 0x7b, 0x46, 0x13, 0x03, 0xa8, 0xd9, 0x7c, 0xb3, 0xdf, 0x9f, 0x14,
    0xb7, 0x34, 0x5a, 0xc4, 0x12, 0x81, 0xc5, 0x98, 0x25, 0x8d, 0x3e, 0xe3,
    0xd8, 0x2d, 0xe4, 0x54, 0xab, 0xb0, 0x13, 0xfd, 0xd1, 0x3f, 0x3b, 0xbf,
    0xa9, 0x45, 0x28, 0x8a, 0x2f, 0x9c, 0x1e, 0x2d, 0xe5, 0xab, 0x13, 0x95,
    0x97, 0xc3, 0x34, 0x37, 0x8d, 0x93, 0x66, 0x31, 0x81, 0xa0, 0x30, 0x23,
    0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x15, 0x31,
    0x16, 0x04, 0x14, 0xa5, 0x23, 0x9b, 0x7e, 0xe6, 0x45, 0x71, 0xbf, 0x48,
    0xc6, 0x27, 0x3c, 0x96, 0x87, 0x63, 0xbd, 0x1f, 0xde, 0x72, 0x12, 0x30,
    0x79, 0x06, 0x09, 0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0x37, 0x11, 0x01,
    0x31, 0x6c, 0x1e, 0x6a, 0x00, 0x4d, 0x00, 0x69, 0x00, 0x63, 0x00, 0x72,
    0x00, 0x6f, 0x00, 0x73, 0x00, 0x6f, 0x00, 0x66, 0x00, 0x74, 0x00, 0x20,
    0x00, 0x45, 0x00, 0x6e, 0x00, 0x68, 0x00, 0x61, 0x00, 0x6e, 0x00, 0x63,
    0x00, 0x65, 0x00, 0x64, 0x00, 0x20, 0x00, 0x52, 0x00, 0x53, 0x00, 0x41,
    0x00, 0x20, 0x00, 0x61, 0x00, 0x6e, 0x00, 0x64, 0x00, 0x20, 0x00, 0x41,
    0x00, 0x45, 0x00, 0x53, 0x00, 0x20, 0x00, 0x43, 0x00, 0x72, 0x00, 0x79,
    0x00, 0x70, 0x00, 0x74, 0x00, 0x6f, 0x00, 0x67, 0x00, 0x72, 0x00, 0x61,
    0x00, 0x70, 0x00, 0x68, 0x00, 0x69, 0x00, 0x63, 0x00, 0x20, 0x00, 0x50,
    0x00, 0x72, 0x00, 0x6f, 0x00, 0x76, 0x00, 0x69, 0x00, 0x64, 0x00, 0x65,
    0x00, 0x72, 0x30, 0x31, 0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2b, 0x0e,
    0x03, 0x02, 0x1a, 0x05, 0x00, 0x04, 0x14, 0x93, 0xa8, 0xb2, 0x7e, 0xb7,
    0xab, 0xf1, 0x1c, 0x3c, 0x36, 0x58, 0xdc, 0x67, 0x6d, 0x42, 0xa6, 0xfc,
    0x53, 0x01, 0xe6, 0x04, 0x08, 0x77, 0x57, 0x22, 0xa1, 0x7d, 0xb9, 0xa2,
    0x69, 0x02, 0x02, 0x08, 0x00
};

static void test_client_cert_authentication(void)
{
    HINTERNET ses, req, con;
    BOOL ret;
    CRYPT_DATA_BLOB pfx;
    HCERTSTORE store;
    const CERT_CONTEXT *cert;

    ses = WinHttpOpen( L"winetest", WINHTTP_ACCESS_TYPE_NO_PROXY, NULL, NULL, 0 );
    ok( ses != NULL, "failed to open session %lu\n", GetLastError() );

    con = WinHttpConnect( ses, L"test.winehq.org", 443, 0 );
    ok( con != NULL, "failed to open a connection %lu\n", GetLastError() );

    req = WinHttpOpenRequest( con, NULL, L"/tests/clientcert/", NULL, NULL, NULL, WINHTTP_FLAG_SECURE );
    ok( req != NULL, "failed to open a request %lu\n", GetLastError() );

    ret = WinHttpSendRequest( req, NULL, 0, NULL, 0, 0, 0 );
    ok( ret || broken(!ret && GetLastError() == ERROR_WINHTTP_SECURE_FAILURE) /* win7 */,
        "failed to send request %lu\n", GetLastError() );
    if (!ret) goto done;

    SetLastError( 0xdeadbeef );
    ret = WinHttpReceiveResponse( req, NULL );
    ok( !ret, "unexpected success\n" );
    ok( GetLastError() == ERROR_WINHTTP_CLIENT_AUTH_CERT_NEEDED, "got %lu\n", GetLastError() );

    pfx.pbData = (BYTE *)pfxdata;
    pfx.cbData = sizeof(pfxdata);
    store = PFXImportCertStore( &pfx, NULL, CRYPT_EXPORTABLE|CRYPT_USER_KEYSET|PKCS12_NO_PERSIST_KEY );
    ok( store != NULL, "got %lu\n", GetLastError() );

    cert = CertFindCertificateInStore( store, X509_ASN_ENCODING, 0, CERT_FIND_ANY, NULL, NULL );
    ok( cert != NULL, "got %lu\n", GetLastError() );

    ret = WinHttpSetOption( req, WINHTTP_OPTION_CLIENT_CERT_CONTEXT, (void *)cert, sizeof(*cert) );
    ok( ret, "failed to set client cert %lu\n", GetLastError() );

    ret = WinHttpSendRequest( req, NULL, 0, NULL, 0, 0, 0 );
    ok( ret, "failed to send request %lu\n", GetLastError() );

    SetLastError( 0xdeadbeef );
    ret = WinHttpReceiveResponse( req, NULL );
    todo_wine {
    ok( !ret, "unexpected success\n" );
    ok( GetLastError() == ERROR_WINHTTP_SECURE_FAILURE || GetLastError() == SEC_E_CERT_EXPIRED, /* win8 */
        "got %lu\n", GetLastError() );
    }

    CertFreeCertificateContext( cert );
    CertCloseStore( store, 0 );
done:
    WinHttpCloseHandle( req );
    WinHttpCloseHandle( con );
    WinHttpCloseHandle( ses );
}

static void test_connection_cache(int port)
{
    HINTERNET ses, con, req;
    DWORD status, size;
    char buffer[256];
    BOOL ret;

    ses = WinHttpOpen(L"winetest", WINHTTP_ACCESS_TYPE_NO_PROXY, NULL, NULL, 0);
    ok(ses != NULL, "failed to open session %lu\n", GetLastError());

    con = WinHttpConnect(ses, L"localhost", port, 0);
    ok(con != NULL, "failed to open a connection %lu\n", GetLastError());

    req = WinHttpOpenRequest(con, L"GET", L"/cached", NULL, NULL, NULL, 0);
    ok(req != NULL, "failed to open a request %lu\n", GetLastError());
    ret = WinHttpSendRequest(req, NULL, 0, NULL, 0, 0, 0);
    ok(ret, "failed to send request %lu\n", GetLastError());
    ret = WinHttpReceiveResponse(req, NULL);
    ok(ret, "failed to receive response %lu\n", GetLastError());
    size = sizeof(status);
    ret = WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &size, NULL);
    ok(ret, "failed to query status code %lu\n", GetLastError());
    ok(status == HTTP_STATUS_OK, "request failed unexpectedly %lu\n", status);
    ret = WinHttpReadData(req, buffer, sizeof buffer, &size);
    ok(ret, "failed to read data %lu\n", GetLastError());
    ok(!size, "got size %lu.\n", size);
    WinHttpCloseHandle(req);

    req = WinHttpOpenRequest(con, L"GET", L"/cached", NULL, NULL, NULL, 0);
    ok(req != NULL, "failed to open a request %lu\n", GetLastError());
    ret = WinHttpSendRequest(req, L"Connection: close", ~0u, NULL, 0, 0, 0);
    ok(ret, "failed to send request %lu\n", GetLastError());
    ret = WinHttpReceiveResponse(req, NULL);
    ok(ret, "failed to receive response %lu\n", GetLastError());
    size = sizeof(status);
    ret = WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &size, NULL);
    ok(ret, "failed to query status code %lu\n", GetLastError());
    ok(status == HTTP_STATUS_OK, "request failed unexpectedly %lu\n", status);
    ret = WinHttpReadData(req, buffer, sizeof buffer, &size);
    ok(ret, "failed to read data %lu\n", GetLastError());
    ok(!size, "got size %lu.\n", size);
    WinHttpCloseHandle(req);

    req = WinHttpOpenRequest(con, L"GET", L"/notcached", NULL, NULL, NULL, 0);
    ok(req != NULL, "failed to open a request %lu\n", GetLastError());
    ret = WinHttpSendRequest(req, L"Connection: close", ~0u, NULL, 0, 0, 0);
    ok(ret, "failed to send request %lu\n", GetLastError());
    ret = WinHttpReceiveResponse(req, NULL);
    ok(ret, "failed to receive response %lu\n", GetLastError());
    size = sizeof(status);
    ret = WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &size, NULL);
    ok(ret, "failed to query status code %lu\n", GetLastError());
    ok(status == HTTP_STATUS_OK, "request failed unexpectedly %lu\n", status);
    WinHttpCloseHandle(req);

    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
}

START_TEST (winhttp)
{
    struct server_info si;
    HANDLE thread;
    DWORD ret;
    HMODULE mod = GetModuleHandleA("winhttp.dll");

    pWinHttpWebSocketClose = (void *)GetProcAddress(mod, "WinHttpWebSocketClose");
    pWinHttpWebSocketCompleteUpgrade = (void *)GetProcAddress(mod, "WinHttpWebSocketCompleteUpgrade");
    pWinHttpWebSocketQueryCloseStatus = (void *)GetProcAddress(mod, "WinHttpWebSocketQueryCloseStatus");
    pWinHttpWebSocketSend = (void *)GetProcAddress(mod, "WinHttpWebSocketSend");
    pWinHttpWebSocketShutdown = (void *)GetProcAddress(mod, "WinHttpWebSocketShutdown");
    pWinHttpWebSocketReceive = (void *)GetProcAddress(mod, "WinHttpWebSocketReceive");

    test_WinHttpOpenRequest();
    test_WinHttpSendRequest();
    test_connect_error();
    test_WinHttpTimeFromSystemTime();
    test_WinHttpTimeToSystemTime();
    test_WinHttpAddHeaders();
    test_secure_connection();
    test_client_cert_authentication();
    test_request_parameter_defaults();
    test_WinHttpQueryOption();
    test_set_default_proxy_config();
    test_empty_headers_param();
    test_timeouts();
    test_resolve_timeout();
    test_credentials();
    test_IWinHttpRequest_Invoke();
    test_WinHttpDetectAutoProxyConfigUrl();
    test_WinHttpGetIEProxyConfigForCurrentUser();
    test_chunked_read();
    test_max_http_automatic_redirects();

    si.event = CreateEventW(NULL, 0, 0, NULL);
    si.port = 7532;
    thread = CreateThread(NULL, 0, server_thread, &si, 0, NULL);
    ok(thread != NULL, "failed to create thread %lu\n", GetLastError());

    ret = WaitForSingleObject(si.event, 10000);
    ok(ret == WAIT_OBJECT_0, "failed to start winhttp test server %lu\n", GetLastError());
    if (ret != WAIT_OBJECT_0)
    {
        CloseHandle(thread);
        return;
    }

    test_IWinHttpRequest(si.port);
    test_connection_info(si.port);
    test_basic_request(si.port, NULL, L"/basic");
    test_basic_request(si.port, L"PUT", L"/test");
    test_chunked_request(si.port);
    test_no_headers(si.port);
    test_no_content(si.port);
    test_head_request(si.port);
    test_not_modified(si.port);
    test_basic_authentication(si.port);
    test_multi_authentication(si.port);
    test_large_data_authentication(si.port);
    test_bad_header(si.port);
    test_multiple_reads(si.port);
    test_cookies(si.port);
    test_request_path_escapes(si.port);
    test_passport_auth(si.port);
    test_websocket(si.port);
    test_redirect(si.port, L"/redirect-temp", L"temporary");
    test_redirect(si.port, L"/redirect-perm", L"permanent");
    test_WinHttpGetProxyForUrl(si.port);
    test_connection_cache(si.port);

    /* send the basic request again to shutdown the server thread */
    test_basic_request(si.port, NULL, L"/quit");

    WaitForSingleObject(thread, 3000);
    CloseHandle(thread);
}
