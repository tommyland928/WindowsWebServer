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

#pragma comment(lib, "httpapi.lib")

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
    ULONG           retCode;
    HANDLE          hReqQueue = NULL;
    int             UrlAdded = 0;
    HTTPAPI_VERSION HttpApiVersion = HTTPAPI_VERSION_1;

    if (argc < 2)
    {
        wprintf(L"%ws: <Url1> [Url2] ... \n", argv[0]);
        return -1;
    }

    //
    // Initialize HTTP Server APIs
    //
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

    //
    // Create a Request Queue Handle
    //
    retCode = HttpCreateHttpHandle(
        &hReqQueue,        // Req Queue
        0                  // Reserved
    );

    if (retCode != NO_ERROR)
    {
        wprintf(L"HttpCreateHttpHandle failed with %lu \n", retCode);
        goto CleanUp;
    }

    //
    // The command line arguments represent URIs that to 
    // listen on. Call HttpAddUrl for each URI.
    //
    // The URI is a fully qualified URI and must include the
    // terminating (/) character.
    //
    for (int i = 1; i < argc; i++)
    {
        wprintf(L"listening for requests on the following url: %s\n", argv[i]);

        retCode = HttpAddUrl(
            hReqQueue,    // Req Queue
            argv[i],      // Fully qualified URL
            NULL          // Reserved
        );
        

        if (retCode != NO_ERROR)
        {
            wprintf(L"HttpAddUrl failed with %lu \n", retCode);
            goto CleanUp;
        }
        else
        {
            //
            // Track the currently added URLs.
            //
            UrlAdded++;
        }
    }

    DoReceiveRequests(hReqQueue);

CleanUp:

    //
    // Call HttpRemoveUrl for all added URLs.
    //
    for (int i = 1; i <= UrlAdded; i++)
    {
        HttpRemoveUrl(
            hReqQueue,     // Req Queue
            argv[i]        // Fully qualified URL
        );
    }

    //
    // Close the Request Queue handle.
    //
    if (hReqQueue)
    {
        CloseHandle(hReqQueue);
    }

    // 
    // Call HttpTerminate.
    //
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

    //
    // Allocate a 2 KB buffer. This size should work for most 
    // requests. The buffer size can be increased if required. Space
    // is also required for an HTTP_REQUEST structure.
    //
    RequestBufferLength = sizeof(HTTP_REQUEST) + 2048;
    pRequestBuffer = (PCHAR)ALLOC_MEM(RequestBufferLength);

    if (pRequestBuffer == NULL)
    {
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    pRequest = (PHTTP_REQUEST)pRequestBuffer;

    //
    // Wait for a new request. This is indicated by a NULL 
    // request ID.
    //

    HTTP_SET_NULL_ID(&requestId);

    for (;;)
    {
        RtlZeroMemory(pRequest, RequestBufferLength);

        result = HttpReceiveHttpRequest(
            hReqQueue,          // Req Queue
            requestId,          // Req ID
            0,                  // Flags
            pRequest,           // HTTP request buffer
            RequestBufferLength,// req buffer length
            &bytesRead,         // bytes received
            NULL                // LPOVERLAPPED
        );
        if (NO_ERROR == result)
        {
            //
            // Worked! 
            // 
            switch (pRequest->Verb)
            {
            case HttpVerbGET:
                wprintf(L"Got a GET request for %ws \n",
                    pRequest->CookedUrl.pFullUrl);

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

            //
            // Reset the Request ID to handle the next request.
            //
            HTTP_SET_NULL_ID(&requestId);
        }
        else if (result == ERROR_MORE_DATA)
        {
            //
            // The input buffer was too small to hold the request
            // headers. Increase the buffer size and call the 
            // API again. 
            //
            // When calling the API again, handle the request
            // that failed by passing a RequestID.
            //
            // This RequestID is read from the old buffer.
            //
            requestId = pRequest->RequestId;

            //
            // Free the old buffer and allocate a new buffer.
            //
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
            // The TCP connection was corrupted by the peer when
            // attempting to handle a request with more buffer. 
            // Continue to the next request.

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

    result = HttpSendHttpResponse(
        hReqQueue,           // ReqQueueHandle
        pRequest->RequestId, // Request ID
        0,                   // Flags
        &response,           // HTTP response
        NULL,                // pReserved1
        &bytesSent,          // bytes sent  (OPTIONAL)
        NULL,                // pReserved2  (must be NULL)
        0,                   // Reserved3   (must be 0)
        NULL,                // LPOVERLAPPED(OPTIONAL)
        NULL                 // pReserved4  (must be NULL)
    );

    if (result != NO_ERROR)
    {
        wprintf(L"HttpSendHttpResponse failed with %lu \n", result);
    }

    return result;
}
