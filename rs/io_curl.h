#ifndef IO_CURL_H
#define IO_CURL_H

#include <curl/curl.h>
 

class CurlStream : public MemoryStream
{
    CURL *curl;
public:
    TParameterList *response_headers;
    TStr status_text;
    int status;

    TStr hostname;
    TStr hostaddr;
    TStr fname;

    CurlStream();
    CurlStream(const char *url, TNameValueList *headers=NULL, TNameValueList *curl_opts=NULL);

    bool canread();
    bool canwrite();
    const char *filename();

    static size_t OnHeader(void *data, size_t size, size_t nmemb, void *_this);
    static size_t OnData(void *data, size_t size, size_t nmemb, void *_this);

};

#endif // IO_CURL_H
