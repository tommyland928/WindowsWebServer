#ifndef UNICODE
#define UNICODE
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <http.h>
#include <stdio.h>
#include<locale.h>

#pragma comment(lib, "httpapi.lib")
#pragma comment(lib, "Ws2_32.lib")
//
// Macros.
//
#define INITIALIZE_HTTP_RESPONSE( resp, status, reason )    \
    do                                                      \
    {                                                       \
        RtlZeroMemory( (resp), sizeof(*(resp)) );           \
        (resp)->StatusCode = (status);                      \
        (resp)->pReason = (reason);                         \
        (resp)->ReasonLength = (USHORT) strlen(reason);     \
    } while (FALSE)

#define ADD_KNOWN_HEADER(Response, HeaderId, RawValue)               \
    do                                                               \
    {                                                                \
        (Response).Headers.KnownHeaders[(HeaderId)].pRawValue =      \
                                                          (RawValue);\
        (Response).Headers.KnownHeaders[(HeaderId)].RawValueLength = \
            (USHORT) strlen(RawValue);                               \
    } while(FALSE)

#define ALLOC_MEM(cb) HeapAlloc(GetProcessHeap(), 0, (cb))

#define FREE_MEM(ptr) HeapFree(GetProcessHeap(), 0, (ptr))

//
// Prototypes.
//
DWORD DoReceiveRequests(HANDLE hReqQueue);

DWORD SendHttpResponse(
    IN HANDLE        hReqQueue,
    IN PHTTP_REQUEST pRequest,
    IN USHORT        StatusCode,
    IN PSTR          pReason,
    IN PSTR          pEntity
);


int __cdecl wmain(
    int argc,
    wchar_t* argv[]
)
{
    setlocale(LC_ALL, "Japanese");
    ULONG           retCode;
    HANDLE          hReqQueue = NULL;
    HTTPAPI_VERSION HttpApiVersion = HTTPAPI_VERSION_2;

    //ver2
    HTTP_SERVER_SESSION_ID ServerSessionId;
    HTTP_URL_GROUP_ID UrlGroupId;

    if (argc < 2)
    {
        wprintf(L"%ws: <Url1> [Url2] ... \n", argv[0]);
        return -1;
    }

    //HTTPサーバAPIの初期化
    retCode = HttpInitialize(
        HttpApiVersion,
        HTTP_INITIALIZE_SERVER,    // Flags
        NULL                       // Reserved
    );

    if (retCode != NO_ERROR)
    {
        wprintf(L"HttpInitialize failed with %lu \n", retCode);
        return retCode;
    }

    //要求をため込むキューを作成
    retCode = HttpCreateRequestQueue(
        HttpApiVersion,
        L"httpQue",
        NULL,
        NULL,
        &hReqQueue
    );

    if (retCode != NO_ERROR)
    {
        wprintf(L"HttpCreateHttpHandle failed with %lu \n", retCode);
        goto CleanUp;
    }

    //セッションの作成
    retCode = HttpCreateServerSession(
        HttpApiVersion,
        &ServerSessionId,
        0
    );

    if (retCode != NO_ERROR)
    {
        wprintf(L"HttpCreateServerSession failed with %lu \n", retCode);
        goto CleanUp;
    }


    //HTTP_LOGGING_INFO logInfo = { 0 };
    //logInfo.Flags.Present = 1;
    //logInfo.LoggingFlags = HTTP_LOGGING_FLAG_LOG_SUCCESS_ONLY;
    //logInfo.DirectoryNameLength = 260;
    //logInfo.DirectoryName = L"C:\\Users\\rikutotomizuka\\Desktop\\講師業務\\Winソフト開発\\ATT11-E\\Practice\\Practice\\HTTPServerAPI ver2\\Log\\test.log";
    //logInfo.Format = HttpLoggingTypeW3C;

    ////セッションに対してログ記録を設定
    //retCode = HttpSetServerSessionProperty(
    //    ServerSessionId,
    //    HttpServerLoggingProperty,
    //    &logInfo,
    //    sizeof(logInfo)
    //    );

    //URLグループの作成、セッションに紐づける
    retCode = HttpCreateUrlGroup(
        ServerSessionId,
        &UrlGroupId,
        0
    );

    if (retCode != NO_ERROR)
    {
        wprintf(L"HttpCreateUrlGroup failed with %lu \n", retCode);
        goto CleanUp;
    }

    

    //すべてのコマンド引数で与えられたURLをURLグループに追加
    for (int i = 1; i < argc; i++)
    {
        wprintf(L"listening for requests on the following url: %s\n", argv[i]);

        retCode = HttpAddUrlToUrlGroup(
            UrlGroupId,
            argv[i],
            0,
            0
        );

        if (retCode != NO_ERROR)
        {
            wprintf(L"HttpAddUrlToUrlGroup failed with %lu \n", retCode);
            goto CleanUp;
        }
    }


    //URLグループと要求をため込むキューを関連付ける
    HTTP_BINDING_INFO PropertyInformation;
    PropertyInformation.Flags.Present = 1;
    PropertyInformation.RequestQueueHandle = hReqQueue;

    retCode = HttpSetUrlGroupProperty(
        UrlGroupId,
        HttpServerBindingProperty,
        &PropertyInformation,
        sizeof(PropertyInformation)
    );

    if (retCode != NO_ERROR)
    {
        wprintf(L"HttpSetUrlGroupProperty failed with %lu \n", retCode);
        goto CleanUp;
    }

    const wchar_t Directory[] = L"C:\\Log";
    const wchar_t app[] = L"myApp";
    HTTP_LOGGING_INFO logInfo = { 0 };
    logInfo.Flags.Present = 1;
    logInfo.LoggingFlags = 0;
    logInfo.DirectoryNameLength = wcslen(L"C:\\Log") * sizeof(wchar_t);
    logInfo.DirectoryName = Directory;
    logInfo.Format = HttpLoggingTypeNCSA;
    logInfo.RolloverType = HttpLoggingRolloverDaily;
    logInfo.Fields = HTTP_LOG_FIELD_DATE | HTTP_LOG_FIELD_TIME | HTTP_LOG_FIELD_CLIENT_IP | HTTP_LOG_FIELD_USER_NAME | HTTP_LOG_FIELD_SITE_NAME | HTTP_LOG_FIELD_COMPUTER_NAME | HTTP_LOG_FIELD_SERVER_IP | HTTP_LOG_FIELD_METHOD;



    retCode = HttpSetUrlGroupProperty(
        UrlGroupId,
        HttpServerLoggingProperty,
        &logInfo,
        sizeof(logInfo)
    );

    if (retCode != NO_ERROR)
    {
        wprintf(L"HttpSetUrlGroupProperty failed with %lu \n", retCode);
        goto CleanUp;
    }


    //HTTPリクエストを待ち受ける自作関数
    DoReceiveRequests(hReqQueue);

//エラー処理
CleanUp:

    //URLグループを削除
    HttpCloseUrlGroup(
        UrlGroupId
    );

    //要求をため込むキューの削除
    if (hReqQueue)
    {
        CloseHandle(hReqQueue);
    }

    //HTTPサーバAPIの終了
    HttpTerminate(HTTP_INITIALIZE_SERVER, NULL);

    return retCode;
}

DWORD DoReceiveRequests(
    IN HANDLE hReqQueue
)
{
    ULONG              result;
    HTTP_REQUEST_ID    requestId;
    DWORD              bytesRead;
    PHTTP_REQUEST      pRequest;
    PCHAR              pRequestBuffer;
    ULONG              RequestBufferLength;

    // 受信バッファを2KB動的確保
    RequestBufferLength = sizeof(HTTP_REQUEST) + 2048;
    pRequestBuffer = (PCHAR)ALLOC_MEM(RequestBufferLength);

    if (pRequestBuffer == NULL)
    {
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    pRequest = (PHTTP_REQUEST)pRequestBuffer;


    //新しいリクエストはrequestId=HTTP_NULL_IDで受信する。
    //バッファーサイズの関係で一度に受信できなかった時は、requestIdを指定して、以前の要求を取りに行く

    //requestIdをHTTP_NULL_IDに設定
    HTTP_SET_NULL_ID(&requestId);

    while(1)
    {
        RtlZeroMemory(pRequest, RequestBufferLength);

        //HTTPリクエストを受信
        result = HttpReceiveHttpRequest(
            hReqQueue,          // Req Queue
            requestId,          // Req ID
            0,                  // Flags
            pRequest,           // HTTP request buffer
            RequestBufferLength,// req buffer length
            &bytesRead,         // bytes received
            NULL                // LPOVERLAPPED
        );

        //受信成功
        if (NO_ERROR == result)
        {
            //リクエストメソッド(pRequest->Verm)により分岐
            switch (pRequest->Verb)
            {
            case HttpVerbGET:
                wprintf(L"Got a GET request for %ws \n",
                    pRequest->CookedUrl.pFullUrl);

                //レスポンスを返す自作関数
                result = SendHttpResponse(
                    hReqQueue,
                    pRequest,
                    200,
                    (PSTR)("OK"),
                    (PSTR)("Hey! You hit the server \r\n")
                );
                break;

            case HttpVerbPOST:

                wprintf(L"Got a POST request for %ws \n",
                    pRequest->CookedUrl.pFullUrl);

                //result = SendHttpPostResponse(hReqQueue, pRequest);
                break;

            default:
                wprintf(L"Got a unknown request for %ws \n",
                    pRequest->CookedUrl.pFullUrl);

                result = SendHttpResponse(
                    hReqQueue,
                    pRequest,
                    503,
                    (PSTR)("Not Implemented"),
                    NULL
                );
                break;
            }

            if (result != NO_ERROR)
            {
                break;
            }

            //次のリクエスト受信に備え、requestId=HTTP_NULL_IDに設定
            HTTP_SET_NULL_ID(&requestId);
        }
        //リクエストを受信するバッファが受信データより小さかった場合
        //バッファを拡張して、再度受信する
        else if (result == ERROR_MORE_DATA)
        {
            //requestIdを直前の受信データに紐づくrequestIdに設定する
            requestId = pRequest->RequestId;

            //以前のバッファを解放し、新しく受信データのサイズに応じてバッファを確保する
            RequestBufferLength = bytesRead;
            FREE_MEM(pRequestBuffer);
            pRequestBuffer = (PCHAR)ALLOC_MEM(RequestBufferLength);

            if (pRequestBuffer == NULL)
            {
                result = ERROR_NOT_ENOUGH_MEMORY;
                break;
            }

            pRequest = (PHTTP_REQUEST)pRequestBuffer;

        }
        else if (ERROR_CONNECTION_INVALID == result &&
            !HTTP_IS_NULL_ID(&requestId))
        {
            //バッファを受信データに合わせて拡張した後に接続エラーになった場合
            HTTP_SET_NULL_ID(&requestId);
        }
        else
        {
            break;
        }

    }

    if (pRequestBuffer)
    {
        FREE_MEM(pRequestBuffer);
    }

    return result;
}

DWORD SendHttpResponse(
    IN HANDLE        hReqQueue,
    IN PHTTP_REQUEST pRequest,
    IN USHORT        StatusCode,
    IN PSTR          pReason,
    IN PSTR          pEntityString
)
{
    HTTP_RESPONSE   response;
    HTTP_DATA_CHUNK dataChunk;
    DWORD           result;
    DWORD           bytesSent;

    //
    // Initialize the HTTP response structure.
    //
    INITIALIZE_HTTP_RESPONSE(&response, StatusCode, pReason);

    //
    // Add a known header.
    //
    ADD_KNOWN_HEADER(response, HttpHeaderContentType, "text/html");

    if (pEntityString)
    {
        // 
        // Add an entity chunk.
        //
        dataChunk.DataChunkType = HttpDataChunkFromMemory;
        dataChunk.FromMemory.pBuffer = pEntityString;
        dataChunk.FromMemory.BufferLength =
            (ULONG)strlen(pEntityString);

        response.EntityChunkCount = 1;
        response.pEntityChunks = &dataChunk;
    }

    // 
    // Because the entity body is sent in one call, it is not
    // required to specify the Content-Length.
    //

    HTTP_LOG_FIELDS_DATA logFields = { 0 };
    logFields.Base.Type = HttpLogDataTypeFields;
    logFields.Method = "GET";
    logFields.ProtocolStatus = StatusCode;
    char str[1024] = { 0 };
    inet_ntop(AF_INET,&((struct sockaddr_in*)pRequest->Address.pLocalAddress)->sin_addr, str, INET_ADDRSTRLEN);
    logFields.ClientIp = str;
    logFields.ClientIpLength = strlen(str);
    result = HttpSendHttpResponse(
        hReqQueue,           // ReqQueueHandle
        pRequest->RequestId, // Request ID
        0,                   // Flags
        &response,           // HTTP response
        NULL,                // Cache Policy(OPTIONAL)
        &bytesSent,          // bytes sent  (OPTIONAL)
        NULL,                // pReserved2  (must be NULL)
        0,                   // Reserved3   (must be 0)
        NULL,                // LPOVERLAPPED(OPTIONAL)
        (PHTTP_LOG_DATA)&logFields         // ログ構造体
    );

    if (result != NO_ERROR)
    {
        wprintf(L"HttpSendHttpResponse failed with %lu \n", result);
    }

    return result;
}
