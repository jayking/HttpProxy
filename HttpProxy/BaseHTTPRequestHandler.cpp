#include <io.h>

#include <curl\curl.h>
#include "SSLSocketStream.h"
#include "httprequest.h"
#include "CommonFuncs.h"
#include "ContentHandle.h"
#include "BaseSSLConfig.h"
#include "Parser.h"
#include "BaseHTTPRequestHandler.h"

extern BaseSSLConfig* g_BaseSSLConfig;

//默认接收到请求后的回调函数
void __stdcall Default_Request_Callback(PCALLBACK_DATA pcallback_data) {
#ifdef _DEBUG
    //::OutputDebugStringA("Default_Request_Callback\n");
#endif
}

void __stdcall Default_Response_Callback(PCALLBACK_DATA pcallback_data) {
#ifdef _DEBUG
   // ::OutputDebugStringA("Default_Response_Callback\n");
#endif
}

#define HTTP_1_1    "HTTP/1.1"
#define HTTP_1_0    "HTTP/1.0"

#define RELEASE_RECVBUF() { \
    if (m_precv_buf != NULL) { \
        free(m_precv_buf); \
        m_precv_buf = NULL; \
    } \
    m_len_recvbuf = 0; \
}

BaseHTTPRequestHandler::BaseHTTPRequestHandler(HTTPSERVICE_PARAMS *pHttpService_Params, HttpSession * pHttpSession)
    :m_pHttpSession(pHttpSession) {

    m_pBaseSockeStream = NULL;
    m_precv_buf = NULL;
    m_len_recvbuf = 0;

    m_pHttpService_Params = pHttpService_Params;
    if (m_pHttpService_Params->request_callback == NULL)
        m_pHttpService_Params->request_callback = Default_Request_Callback;
    if (m_pHttpService_Params->response_callback == NULL)
        m_pHttpService_Params->response_callback = Default_Response_Callback;

    //这个地方可以判断是否为SSH服务
    if (m_pHttpService_Params->bSSH) {
        m_pBaseSockeStream = new SSLSocketStream(&m_precv_buf, &m_len_recvbuf, &m_pHttpSession->m_pSendbuf, &m_pHttpSession->m_SizeofSendbuf);
    }
    else
        m_pBaseSockeStream = new BaseSocketStream(&m_precv_buf, &m_len_recvbuf, &m_pHttpSession->m_pSendbuf, &m_pHttpSession->m_SizeofSendbuf);    //初始化基础的Socket处理器
}

BaseHTTPRequestHandler::~BaseHTTPRequestHandler() {
    if (m_pBaseSockeStream != NULL) {
        delete m_pBaseSockeStream;
        m_pBaseSockeStream = NULL;
    }

    reset();
    RELEASE_RECVBUF();
}
//do_OPTIONS
void BaseHTTPRequestHandler::do_OPTIONS() {
    return;
}
//do_GET
void BaseHTTPRequestHandler::do_GET() {
    int ret = 0;
    char *result = NULL;
    HttpHeaders response_httpheaders;
    HttpContent response_httpcontent;
    char *result_phttpheaders = NULL;
    char *result_phttpcontent = NULL;
    size_t len_httpheaders = 0;
    size_t len_httpcontent = 0;
    char classname[256] = { 0 };
    size_t result_size = 0;

    Parser::RequestHttpHeadersParser(&http_items);  //处理请求的相关http头信息
    
    if (m_pHttpService_Params->bSSH) {
        ret = httprequest.https_request(&http_items, &httpcontent, &response_httpheaders, &response_httpcontent);
    }
    else {
        if (_stricmp("SSLSocketStream", m_pBaseSockeStream->_classname(classname, 256)) == 0) {
            http_items.m_port = m_port; //这个地方需要修改端口号
            ret = httprequest.https_request(&http_items, &httpcontent, &response_httpheaders, &response_httpcontent);
        }
        else {
            ret = httprequest.http_request(&http_items, &httpcontent, &response_httpheaders, &response_httpcontent);
        }
    }

    do {//返回处理顺序应当可以优化
        if (ret == HttpRequest::CURLE_OK) {
            if (response_httpheaders.m_response_status == 400) {
                m_pHttpSession->m_resultstate = HttpSession::HS_RESULT_SERVER_NOEXIST;
                m_pHttpSession->m_bKeepAlive = FALSE;
                break;
            }
            m_pHttpSession->m_resultstate = HttpSession::HS_RESULT_OK;
            Parser::ResponseHttpHeadersParser(&response_httpheaders, &http_items);   //处理返回的相关http头信息          

            result_phttpcontent = response_httpcontent.getbuffer(&len_httpcontent);
            result_phttpheaders = response_httpheaders.getbuffer(&len_httpheaders);
         
            result_size = len_httpheaders + len_httpcontent + strlen(response_httpheaders.m_version) + 50;
            result = (char*)::malloc(result_size);
            memset(result, 0, result_size);
            char *descript = HttpHeaders::get_status_code_descript(response_httpheaders.m_response_status);
            wsprintfA(result, "%s %d %s\r\n", response_httpheaders.m_version, response_httpheaders.m_response_status, descript);
            size_t len_title = strlen(result);
            memcpy_s(result + len_title, result_size - len_title, result_phttpheaders, len_httpheaders);
            memcpy_s(result + len_httpheaders + len_title, result_size - len_title - len_httpheaders, result_phttpcontent, len_httpcontent);

            // 处理返回回调
            result_size = len_title + len_httpheaders + len_httpcontent;
            invokeResponseCallback(&result,&result_size);
            m_pBaseSockeStream->write(result, result_size);
        }

    } while (0);
    // 清理工作
    if (result_phttpcontent != NULL) {
        free(result_phttpcontent);
        result_phttpcontent = NULL;
    }

    if (result_phttpheaders != NULL) {
        free(result_phttpheaders);
        result_phttpheaders = NULL;
    }

    if (result != NULL) {
        free(result);
        result = NULL;
    }

    response_httpheaders.release();
    response_httpcontent.release();
}
//do_HEADER
void BaseHTTPRequestHandler::do_HEAD() {
    return;
}
//do_POST
void BaseHTTPRequestHandler::do_POST() {
    return;
}
//do_PUT
void BaseHTTPRequestHandler::do_PUT() {

}
//do_DELETE
void BaseHTTPRequestHandler::do_DELETE() {

}
//do_TRACE
void BaseHTTPRequestHandler::do_TRACE() {
    return;
}
//do_CONNECT
void BaseHTTPRequestHandler::do_CONNECT() {

    //判断证书文件是否为空
    if (g_BaseSSLConfig!=NULL && 
        g_BaseSSLConfig->status()==BaseSSLConfig::STATUS_INITFINAL) {
#if 1
        connect_intercept();//中断，获取数据后转发
#endif
    }
    else
        connect_relay();//直接转发
    return;
}

void BaseHTTPRequestHandler::connect_intercept() {
    BOOL bFind_Colon = FALSE;
    char *pUrl = NULL; 


    if (m_pBaseSockeStream != NULL) {
        delete m_pBaseSockeStream;
    }


    pUrl = http_items.m_uri;
       
    if( *pUrl == '/'){
        m_port = HttpHeaders::HTTPS_DEFALUT_PORT;
    }
    else{
       while (*pUrl != '\0') {
           if (*pUrl == ':') {
               bFind_Colon = TRUE;
               break;
           }
           pUrl++;
       }

       if (bFind_Colon) {
           if (*(pUrl + 1) != '\0') {
               pUrl++;
               m_port = (WORD)strtol(pUrl, NULL, 10);
           }
       }
    }
   
    SSLSocketStream::_init_syn();
    //
    m_pBaseSockeStream = new SSLSocketStream(&m_precv_buf, &m_len_recvbuf, &m_pHttpSession->m_pSendbuf, 
                                             &m_pHttpSession->m_SizeofSendbuf);
    if(!m_pBaseSockeStream->init(http_items.m_host,strlen(http_items.m_host))) {
        return;
    }
    //响应CONNECT消息
    char temp[1024] = { 0 };
    wsprintfA(temp, "%s %d %s\r\n\r\n", http_items.m_version, 200, "Connection Established");
    m_pHttpSession->m_pSendbuf = (char*)malloc(strlen(temp));
    memset(m_pHttpSession->m_pSendbuf, 0, strlen(temp));
    memcpy_s(m_pHttpSession->m_pSendbuf, strlen(temp), temp, strlen(temp));
    m_pHttpSession->m_SizeofSendbuf = strlen(temp);
    m_pHttpSession->m_bKeepAlive = TRUE;
    m_pHttpSession->m_resultstate = HttpSession::HS_RESULT_OK;
}
// 直接转发
void BaseHTTPRequestHandler::connect_relay() {
    // 暂时不支持透明转发，
    // 透明转发需要与服务器端建立socket连接，暂时不能加入到固有的iocp服务中，
    // 待版本的改进
}
// 这个地方接收处理数据，返回给服务器需要处理的方法
void BaseHTTPRequestHandler::handler_request(void *recvbuf, DWORD len, BaseDataHandler_RET * ret) {
    BaseDataHandler_RET* p_ret = NULL;
    size_t headersize = 0;
    int result = BaseSocketStream::BSS_RET_UNKNOWN;
    char * pContent_Length = NULL;
    long Content_Length = 0;

    ret->dwOpt = RET_UNKNOWN;
    result = m_pBaseSockeStream->read(recvbuf, len); 

    if (result == BaseSocketStream::BSS_RET_RESULT) {
        if (m_len_recvbuf > 0 && m_precv_buf != NULL)
        {//处理的数据正确，在查找数据，否则不处理

            if ((headersize = find_httpheader(m_precv_buf,m_len_recvbuf)) > 0) {

                if (http_items.parse_httpheaders((const char*)m_precv_buf, m_len_recvbuf, HttpHeaders::HTTP_REQUEST)) {

                    if ((pContent_Length = http_items["Content-Length"]) != NULL) {
                        //发现长度
                        Content_Length = strtol(pContent_Length, NULL, 10);
                        if (headersize + Content_Length <= m_len_recvbuf) {//说明已经接收完全部数据
                            httpcontent.insert(m_precv_buf + headersize, Content_Length);
                            //invokeRequestCallback(&http_items);
                            invokeMethod(http_items.m_method);
                            reset();
                            RELEASE_RECVBUF()
                            ret->dwOpt = RET_SEND; 
                        }
                        else {
                            reset();
                            ret->dwOpt = RET_RECV;
                        }
                    }
                    else {
                        //invokeRequestCallback(&http_items);
                        invokeMethod(http_items.m_method);
                        reset(); 
                        RELEASE_RECVBUF()
                        ret->dwOpt = RET_SEND; 
                    }
                }//if (http_items.parse_httpheaders((const char*)m_precv_b........
                else {
                    reset(); 
                    RELEASE_RECVBUF()
                    ret->dwOpt = RET_RECV;
                }
            }//if ((headersize = find_httpheader(m_precv_buf,m_len_recvbuf)) > 0) 
            else {
                ret->dwOpt = RET_RECV;
            }
        }//if (m_len_recvbuf > 0 && m_precv_buf == NULL) 
    }
    else if (result == BaseSocketStream::BSS_RET_RECV) {
        ret->dwOpt = RET_RECV;
    }
    else if (result == BaseSocketStream::BSS_RET_SEND) {
        RELEASE_RECVBUF()
        ret->dwOpt = RET_SEND;
    }

    return;
}

size_t BaseHTTPRequestHandler::find_httpheader(const char* buf, size_t bufsize) {
    size_t httplen = 0;
    char *pHttpFlag = NULL;
    if (buf != NULL && bufsize>0) {
        if (ContentHandle::search_content(buf, bufsize, "^\\S{3,}\\s{1,}\\S{1,}\\s{1,}HTTP\\/[0-9]\\.[0-9]\\s{2}",NULL,NULL)) {
            if (ContentHandle::search_content(buf, bufsize, "\r\n\r\n", &pHttpFlag, &httplen)) {
                httplen += strlen("\r\n\r\n");
            }
        }
    }
    return httplen;
};
// 分配调用对应的处理器方法
void BaseHTTPRequestHandler::invokeMethod(const char * method) {

    invokeRequestCallback(&http_items);

    if (_stricmp(method, "CONNECT") == 0) {
        do_CONNECT();
    }
    else
        do_GET();
}
// 请求回调前，回调函数
void BaseHTTPRequestHandler::invokeRequestCallback(HttpHeaders *http_headers) {
    CALLBACK_DATA callback_data;
    memset(&callback_data, 0, sizeof(CALLBACK_DATA));
    callback_data.len = http_headers->get_request_uri(NULL, 0);

    if(callback_data.len == 0)
        return;


    callback_data.buf = (char*)malloc(callback_data.len + 1);
    memset(callback_data.buf, 0, callback_data.len + 1);

    http_headers->get_request_uri(callback_data.buf, callback_data.len);

    m_pHttpService_Params->request_callback(&callback_data);

    //处理完之后，清理掉内存
    if (callback_data.buf != NULL) {
        free(callback_data.buf);
        callback_data.buf = NULL;
    }
}
// 请求返回后，调用回调函数
void BaseHTTPRequestHandler::invokeResponseCallback(char **buf, size_t *plen) {
    CALLBACK_DATA callback_data;
    if(*buf == NULL || *plen == 0)
        return;

    memset(&callback_data, 0, sizeof(CALLBACK_DATA));
    callback_data.buf = (char*)malloc(*plen);
    memset(callback_data.buf, 0, *plen);

    memcpy_s(callback_data.buf, *plen, *buf, *plen);
    callback_data.len = *plen;
    

    m_pHttpService_Params->response_callback(&callback_data);

    if(*buf != NULL){
        free(*buf);
        *buf = NULL;
    }

    *buf = (char*) malloc(callback_data.len);
    memset(*buf, 0, callback_data.len);
    memcpy_s(*buf, callback_data.len, callback_data.buf, callback_data.len);

    *plen = callback_data.len;

    //处理完之后，清理掉内存
    if (callback_data.buf != NULL) {
        free(callback_data.buf);
        callback_data.buf = NULL;
    }
}
// 没有完全接收数据就需要重置，保证后续数据解析的正确性
void BaseHTTPRequestHandler::reset() {
    http_items.release();
    httpcontent.release();
}