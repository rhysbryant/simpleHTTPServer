#pragma once
extern "C" {
#include "lwip/tcp.h"

#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/x509.h"
#include "mbedtls/ssl.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/error.h"
#include "mbedtls/debug.h"

#if defined(MBEDTLS_SSL_CACHE_C)
#include "mbedtls/ssl_cache.h"
#endif
}
#include "queue.h"
#include "ServerConnection.h"
#include "common.h"

namespace SimpleHTTP {
	class SecureServerConnection {
    private:
        ServerConnection* planTextLayerConn;
        static const uint8_t ChunkForSendFlagSent = 1;
        static const uint8_t ChunkForSendFlagMem = 2;
        static const uint8_t ChunkForSendFlagClear = 4;
        struct ChunkForSend {
            const uint8_t* data;
            const uint16_t size;
            const uint16_t plainTextSize;
            const uint8_t* plainTextPtr;
            uint8_t flags;

        };

        pbuf bufTail;
        LinkedListQueue<pbuf*>  readQueue;
        LinkedListQueue<ChunkForSend>  writeQueue;//deque
        LinkedListQueue<ChunkForSend> sentWaitingAckQueue;
        int lastRemainingLength;
        int plainTextLengthPendingNotification;

        uint16_t lastPlainTextSize;
        uint8_t* lastPlainTextPtr;

        int writeBufWaitingAck;
        int writeBlockedWaitingOnAckForSize;

        bool clearWriteBlock;
        uint8_t recvBuf[512];

        tcp_pcb* tpcb;
        mbedtls_ssl_context ssl;

        static const int maxSendSize = ServerConnection::maxSendSize + 29;

        int mbedtlsTCPSendCallback(const unsigned char* buf, size_t len);
        static int mbedtlsTCPSendCallback(void* ctx, const unsigned char* buf, size_t len);

        int mbedtlsTCPRecvCallback(unsigned char* buf, size_t len);
        static int mbedtlsTCPRecvCallback(void* ctx, unsigned char* buf, size_t len);


        int plainTextLayerWriteCallback(const void* dataptr, u16_t len, u8_t apiflags);
        static int plainTextLayerWriteCallback(tcp_pcb* pcb, const void* dataptr, u16_t len, u8_t apiflags);

    public:
        SecureServerConnection(struct tcp_pcb* client,ServerConnection* planTextLayerConn); 
        ~SecureServerConnection(); 

        int initSSLContext(mbedtls_ssl_config* conf,mbedtls_ssl_send_t *f_send, mbedtls_ssl_recv_t *f_recv);

        bool queueSSLDataForSend(uint8_t* data, int len);
        
        err_t sendNextSSLDataChunk();

        pbuf* getNextBufferForRead();

        int sslSessionProcess(pbuf* newData);

        bool isReviceQueueEmpty();

        int writeData(const void* dataptr, u16_t len, u8_t apiflags);
        Result sendCompleteCallback(int len);
    };
}