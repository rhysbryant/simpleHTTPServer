/*
 *  Copyright (c) 2023 Rhys Bryant
 *  Author Rhys Bryant
 *
 *	This file is part of SimpleHTTP
 *
 *   SimpleHTTP is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   any later version.
 *
 *   SimpleHTTP is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with SimpleHTTP.  If not, see <http://www.gnu.org/licenses/>.
 */
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

namespace SimpleHTTP::Internal {
	class SecureServerConnection final : Transport {
	private:
		ServerConnection* planTextLayerConn;
		static const uint8_t ChunkForSendFlagSent = 1;
		static const uint8_t ChunkForSendFlagMem = 2;
		static const uint8_t ChunkForSendFlagClear = 4;

		bool firstUse;

		pbuf bufTail;
		LinkedListQueue<pbuf*>  readQueue;

		uint16_t lastPlainTextSize;
		uint8_t* lastPlainTextPtr;

		int writeBufWaitingAck;
		uint8_t recvBuf[512];

		tcp_pcb* tpcb;
		mbedtls_ssl_context ssl;

		int incompleteOutLength;
		char* incompleteOutPtr;

		static const int maxSendSize = ServerConnection::maxSendSize + 29;

		int mbedtlsTCPSendCallback(const unsigned char* buf, size_t len);
		static int mbedtlsTCPSendCallback(void* ctx, const unsigned char* buf, size_t len);

		int mbedtlsTCPRecvCallback(unsigned char* buf, size_t len);
		static int mbedtlsTCPRecvCallback(void* ctx, unsigned char* buf, size_t len);

	public:
		SecureServerConnection();
		~SecureServerConnection();

		void init(struct tcp_pcb* client, ServerConnection* planTextLayerConn);
		int initSSLContext(mbedtls_ssl_config* conf);


		pbuf* getNextBufferForRead();

		int sslSessionProcess(pbuf* newData);

		bool isReviceQueueEmpty();

		int write(const void* dataptr, u16_t len, u8_t apiflags);
		//don't use */
		err_t shutdown();

		void close();
		void closeWithoutLock();
        bool getRemoteIPAddress(char *buf, int buflen);

		Result sendCompleteCallback(int len);

        int getAvailableSendBuffer();

		bool inUse() {
			return tpcb != 0;
		}
	};
}