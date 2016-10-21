#include "rslib.h"
#pragma hdrstop

#ifndef NO_CURL_STREAM
#include "io_curl.h"

#if (LIBCURL_VERSION_MAJOR*100+LIBCURL_VERSION_MINOR) < 719
#define CURLINFO_PRIMARY_IP (CURLINFO) (CURLINFO_STRING + 32)
#endif

CurlStream::CurlStream() : curl(NULL), response_headers(NULL)
{
}

CurlStream::CurlStream(const char *url, TNameValueList *headers, TNameValueList *curl_opts)
 : response_headers(NULL)
{
    status_text = "";
    curl = curl_easy_init();
    if (curl && url)
    {
        struct curl_slist *chunk = NULL;
        if (headers && headers->Count()) {
            if (!headers->Has("User-Agent"))
                headers->Set("User-Agent", "Spoken API Connector/1.0");
            for (size_t i=0; i < headers->Count(); i++) {
                TStr header(headers->Name(i), ": ", headers->Value(i));
                // printf("Custom header-> %s\n", header.c_str());
                chunk = curl_slist_append(chunk, header.c_str());
            }
        }

        // Additional options:
        long int connect_timeout = 10L;
        long int timeout = 20L;
        bool is_post = false;
        TStr post_data = "";

        if (curl_opts && curl_opts->Count()) {
            if (curl_opts->Has("timeout"))
                timeout = curl_opts->GetInt("timeout", timeout);
            if (curl_opts->Has("connect_timeout"))
                connect_timeout = curl_opts->GetInt("connect_timeout", connect_timeout);
            if (curl_opts->Has("method")) {
                const char *method = (*curl_opts)("method");
                if (!strcasecmp(method, "POST"))
                    is_post = true;
            }
            post_data = (*curl_opts)("post_data");
        }

        /* printf("connect_timeout = %d, timeout = %d, post = %s, post_data = '%s'\n",
            connect_timeout, timeout, is_post ? "true" : "false", post_data.c_str());
        */
        curl_easy_setopt(curl, CURLOPT_URL, url);
        if (chunk)
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
        else
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "Spoken API Connector/1.0");
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, connect_timeout);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);  // No progress meter
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, CurlStream::OnHeader);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, this);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlStream::OnData); 
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
        if (is_post || post_data != "")
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());

        CURLcode res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            error = new xdb("curl_easy_perform() failed", curl_easy_strerror(res));
            status_text = "";
            status = 0;
            if (response_headers) {
                delete response_headers;
                response_headers = NULL;
            }
            Type = NotOpen;
            return;
        }
        rewind();
        // Retrieve information about the transfer that took place:
        char *url, *ip_addr;
        curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &url);
        fname = url;
        hostname = url;
        curl_easy_getinfo(curl, CURLINFO_PRIMARY_IP, &ip_addr); 
        hostaddr = ip_addr;
        curl_easy_cleanup(curl);
    }
    Type = ReadOnly;
}

bool CurlStream::canread()
 {
     return Type != NotOpen;
 }


bool CurlStream::canwrite()
 {
     return false;
 }

 const char *CurlStream::filename()
 {
     return fname;
 }

size_t CurlStream::OnHeader(void *data, size_t size, size_t nmemb, void *_this)
{
    // printf("OnHeader called with: '%s'\n", (const char *)data);
    size_t data_size = size * nmemb;

    CurlStream *that = static_cast<CurlStream *>(_this);

    if (data_size <= 2)  // No data or just CL-RF
        return data_size;

    char * cdata = (char *) data;
    cdata[data_size - 2] = '\0';

    if (!that->response_headers) {  // First header line is the status text
        that->response_headers = new TParameterList();
        that->status_text.Set(cdata, data_size - 2);
        char *c = strchr(cdata,' ');
        if (c)
            that->status = atoi(c + 1);
        return data_size;
    }

    TStr header_name, header_val;
    if (strsplit(cdata, ": ", header_name, header_val)) {
        that->response_headers->Set(header_name, header_val);
    }

    return data_size;
}

size_t CurlStream::OnData(void *data, size_t size, size_t nmemb, void *_this)
{
    // printf("OnData called!\n");
    size_t data_size = size * nmemb;
    CurlStream *that = static_cast<CurlStream *>(_this);
    that->write((char *) data, data_size);
    return data_size;
}

#endif

