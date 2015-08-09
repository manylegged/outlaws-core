
#include "StdAfx.h"

#if WIN32
#include "../curl/include/curl/curl.h"
#else
#include <curl/curl.h>
#endif

struct ReadData {
    string input;
    int    input_idx;
};

static size_t log_read_callback(char *buffer, size_t size, size_t nitems, void *instream)
{
    ReadData *self = (ReadData*) instream;
    const size_t tocopy = std::min(size * nitems, self->input.size() - self->input_idx);
    memcpy(buffer, self->input.c_str() + self->input_idx, tocopy);
    self->input_idx += tocopy;
    return tocopy;
}

static size_t log_write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    std::string msg;
    msg.append(ptr, size * nmemb);
    DPRINT(NETWORK, ("%s", msg.c_str()));
    return size * nmemb;
}

int OLG_UploadLog(const char* logdata, int loglen)
{
    if (!kNetworkEnable) {
        DPRINT(NETWORK, ("Not Uploading error log, size %d", loglen));
        return 0;
    }
    DPRINT(NETWORK, ("Uploading error log, size %d", loglen));
    if (loglen == 0)
        return 0;
    
    CURL *curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long) kNetworkTimeout);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);

    /* enable verbose for easier tracing */ 
    curl_easy_setopt(curl, CURLOPT_VERBOSE, IS_DEVEL ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, debug_callback);

    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

    ReadData data = { ZF_Compress(logdata, loglen), 0 };
    curl_easy_setopt(curl, CURLOPT_READDATA, &data);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, log_read_callback);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)data.input.size());
    curl_slist *headers = curl_slist_append(NULL, "Content-Encoding: gzip");
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, log_write_callback);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    string url = str_format("%s?version=%s&op=crashlog", kNetworkURL.c_str(), getVersionStr());
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    CURLcode res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        DPRINT(NETWORK, ("curl_easy_perform() failed: %s", curl_easy_strerror(res)));
        return 0;
    }

    if (headers)
        curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return 1;
}
