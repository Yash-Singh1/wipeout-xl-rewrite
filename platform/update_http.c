#include <SDL3/SDL.h>
#include "updates.h"

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
int PortUpdateFetch(char *url, size_t size)
{
    HINTERNET session = NULL, connection = NULL, request = NULL;
    DWORD status = 0, bytes = sizeof(status), policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    wchar_t location[256];
    int ok = 0;
    session = WinHttpOpen(L"WipeoutXL release check", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                         WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) goto done;
    WinHttpSetTimeouts(session, 2000, 2000, 2000, 2000);
    connection = WinHttpConnect(session, L"github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connection) goto done;
    request = WinHttpOpenRequest(connection, L"HEAD", L"/Yash-Singh1/wipeout-xl-rewrite/releases/latest",
                                NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!request || !WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy)) ||
        !WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request, NULL) ||
        !WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status, &bytes, WINHTTP_NO_HEADER_INDEX) ||
        (status != 301 && status != 302 && status != 307 && status != 308)) goto done;
    bytes = sizeof(location);
    if (WinHttpQueryHeaders(request, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX,
                            location, &bytes, WINHTTP_NO_HEADER_INDEX)) {
        ok = WideCharToMultiByte(CP_UTF8, 0, location, -1, url, (int)size, NULL, NULL) > 0;
    }
done:
    if (request) WinHttpCloseHandle(request);
    if (connection) WinHttpCloseHandle(connection);
    if (session) WinHttpCloseHandle(session);
    return ok;
}
#elif defined(SDL_PLATFORM_ANDROID)
#include <jni.h>
int PortUpdateFetch(char *url, size_t size)
{
    JNIEnv *env = SDL_GetAndroidJNIEnv();
    jobject activity = SDL_GetAndroidActivity();
    jclass type;
    jmethodID method;
    jstring result = NULL;
    int ok = 0;
    if (!env || !activity) return 0;
    type = (*env)->GetObjectClass(env, activity);
    method = (*env)->GetMethodID(env, type, "latestReleaseUrl", "()Ljava/lang/String;");
    if (method) result = (*env)->CallObjectMethod(env, activity, method);
    if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
    else if (result) {
        const char *text = (*env)->GetStringUTFChars(env, result, NULL);
        if (text) { ok = SDL_strlcpy(url, text, size) < size; (*env)->ReleaseStringUTFChars(env, result, text); }
    }
    if (result) (*env)->DeleteLocalRef(env, result);
    (*env)->DeleteLocalRef(env, type);
    (*env)->DeleteLocalRef(env, activity);
    return ok;
}
#elif defined(PORT_HAVE_CURL)
#include <curl/curl.h>
int PortUpdateFetch(char *url, size_t size)
{
    CURL *request = curl_easy_init();
    char *location = NULL;
    long status = 0;
    int ok = 0;
    if (!request) return 0;
    curl_easy_setopt(request, CURLOPT_URL, PORT_RELEASES "/latest");
    curl_easy_setopt(request, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(request, CURLOPT_USERAGENT, "WipeoutXL release check");
    curl_easy_setopt(request, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(request, CURLOPT_CONNECTTIMEOUT, 3L);
    curl_easy_setopt(request, CURLOPT_TIMEOUT, 8L);
    if (curl_easy_perform(request) == CURLE_OK) {
        curl_easy_getinfo(request, CURLINFO_RESPONSE_CODE, &status);
        curl_easy_getinfo(request, CURLINFO_REDIRECT_URL, &location);
        if (status >= 300 && status < 400 && location) ok = SDL_strlcpy(url, location, size) < size;
    }
    curl_easy_cleanup(request);
    return ok;
}
#elif !defined(__APPLE__)
int PortUpdateFetch(char *url, size_t size) { (void)url; (void)size; return 0; }
#endif
