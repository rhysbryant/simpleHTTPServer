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
#include "SecureServer.h"
#include "Router.h"
#include <queue>
#include "log.h"
 /*
 * data sending flow
 *
 * the payload for sent is divided into chunks of upto maxSendSize in size
 *
 * based round 3 queues plan text data queue (in ClientConnection class) -> tls write data queue -> chunk waiting Ack queue

 * tcp_write_tls callback from http lib - with plan text data for sending
 * mbedtls_tcp_send callback from mbedtls - with tls data for sending
 * tcp_sent_cb callback from network stack - data has been acked
 *
 * (connection).sendCompleteCallback will send the next plan text chunk if any i.e will call tcp_write_tls
 * sendNextChunk will send the next chunk of tls data
 *
 * if the payload is over maxSendSize mbedtls_ssl_write will go into a "blocking" like state i.e will return a busy status
 * until the blocking data has been acked. otherwise (under maxSendSize) a copy is made of the payload and mbedtls_ssl_write can be called again
 * before the previously sent data has been acked
 *
 * the two main use cases we need to account for are are lots of small chunks being sent in parallel and large of maxSendSize being sent one at a time
 * 
 * data receive flow
 * 
 * the pbuf from the lwip receive callback is queued and freed in the mbedtls mbedtls_tcp_recv callback
 * called from within mbedtls_ssl_read() and mbedtls_ssl_handshake()
 */
using namespace SimpleHTTP;

err_t SecureServer::tcp_accept_cb(void* arg, struct tcp_pcb* newpcb, err_t err)
{

	auto conn = Router::getFreeConnection();
	if (conn == 0) {
		tcp_abort(newpcb);
		return ERR_ABRT;
	}
	SHTTP_LOGI(__FUNCTION__, "before setup; heap free %d min seen %d", (int)esp_get_free_heap_size(), (int)esp_get_minimum_free_heap_size());
	auto s = new SSLConnection{

	};
	s->tpcb = newpcb;
	s->bufTail.payload = nullptr;
	s->conn = conn;
	s->lastRemainingLength = 0;
	s->writeBlockedWaitingOnAckForSize = 0;
	s->clearWriteBlock = false;

    s->plainTextLengthPendingNotification = 0;

	conn->init(newpcb, tcp_write_tls);

	tcp_arg(newpcb, s);
	tcp_err(newpcb, tcp_err_cb);
	tcp_sent(newpcb, tcp_sent_cb);
	tcp_recv(newpcb, tcp_recv_cb);

	mbedtls_ssl_init(&s->ssl);

	auto result = mbedtls_ssl_setup(&s->ssl, &conf);
	if (result != 0) {
		SHTTP_LOGE(__FUNCTION__, "mbedtls_ssl_setup failed %d", result);
		tcp_abort(newpcb);
		return ERR_ABRT;
	}

	mbedtls_ssl_set_bio(&s->ssl, s, mbedtls_tcp_send, mbedtls_tcp_recv, NULL);
	SHTTP_LOGI(__FUNCTION__, "connection accepted;heap free %d min seen %d", (int)esp_get_free_heap_size(), (int)esp_get_minimum_free_heap_size());
	sslSessionProcess(s);

	return ERR_OK;

}

void SecureServer::cleanup(std::queue<ChunkForSend>& queue){
    while(!queue.empty()){
        auto item = &queue.front();
        if( item->flags & ChunkForSendFlagMem){
            delete item->data;
        }
        queue.pop();
    }
}

void SecureServer::cleanup(SSLConnection* conn) {
	mbedtls_ssl_free(&conn->ssl);
	conn->tpcb = 0;
	conn->conn->init(0);
    
    cleanup(conn->writeQueue);
	cleanup(conn->sentWaitingAckQueue);
    delete conn;
}

err_t SecureServer::tcp_recv_cb(void* arg, struct tcp_pcb* tpcb, struct pbuf* p,
	err_t err)
{
	if (arg != 0)
	{
		auto conn = static_cast<SSLConnection*>(arg);
		if (p == 0)
		{
			SHTTP_LOGI(__FUNCTION__, "closing");
			conn->conn->closeWithOutLocking();
			cleanup(conn);
			tcp_arg(tpcb, nullptr);

			return ERR_OK;
		}
		SHTTP_LOGI(__FUNCTION__, "got %d bytes putting, queuing", p->len);

		conn->readQueue.push(p);
		sslSessionProcess(conn);
	}

	return ERR_OK;
}

pbuf* SecureServer::getNextBufferForRead(SSLConnection* conn) {
	if (conn->readQueue.empty()) {
		SHTTP_LOGI(__FUNCTION__, "queue empty");
		return 0;
	}
	//if nothing left in the current buffer free it and move to the next
	if (conn->bufTail.len == 0) {
		//if there was a buffer before free it 
		if (conn->bufTail.payload != nullptr) {

			auto current = conn->readQueue.front();
			SHTTP_LOGI(__FUNCTION__, "freeing buffer, size %d", current->len);
			tcp_recved(conn->tpcb, current->len);
			pbuf_free(current);
			conn->readQueue.pop();
			conn->bufTail.payload = nullptr;
		}

		if (!conn->readQueue.empty()) {
			conn->bufTail = *conn->readQueue.front();
			SHTTP_LOGI(__FUNCTION__, "assigning buffer, size %d", conn->bufTail.len);
		}
		else {
			SHTTP_LOGI(__FUNCTION__, "queue now empty heap free %d min seen %d", (int)esp_get_free_heap_size(), (int)esp_get_minimum_free_heap_size());
			return 0;
		}
	}

	return &conn->bufTail;
}

int SecureServer::mbedtls_tcp_recv(void* ctx, unsigned char* buf, size_t len) {
	auto conn = static_cast<SSLConnection*>(ctx);
	if (conn->readQueue.empty()) {
		SHTTP_LOGI(__FUNCTION__, "want %d bytes not here yet", (int)len);
		return MBEDTLS_ERR_SSL_WANT_READ;
	}

	int readLen = 0;

	while (auto pBuf = getNextBufferForRead(conn)) {

		//mbed buffer smaller then the network buffer
		//won't free the network buffer until it's completely read
		if (len < pBuf->len) {
			memcpy(buf, pBuf->payload, len);
			pBuf->payload += len;
			pBuf->len -= len;
			return len;
		}

		//mbed buffer bigger then the current pbuf loop and and try appending data from the next pbuf
		if (pBuf->len <= len) {
			memcpy(buf, pBuf->payload, pBuf->len);
			readLen += pBuf->len;
			buf += pBuf->len;
			len -= pBuf->len;
			pBuf->len = 0;
		}
		else {
			break;
		}
	}

	return readLen;
}



int SecureServer::tcp_write_tls(tcp_pcb* pcb, const void* dataptr, u16_t len, u8_t apiflags) {
	if (len == 0) {
		return ERR_OK;
	}
	else if (pcb == 0) {
		SHTTP_LOGI(__FUNCTION__, "null pcb");
		return ERR_ARG;
	}

	auto conn = static_cast<SSLConnection*>(pcb->callback_arg);
	if (conn->tpcb == nullptr) {
		SHTTP_LOGI(__FUNCTION__, "null pcb.");
		return ERR_ARG;
	}

	uint8_t* data = (uint8_t*)dataptr;

	if (apiflags & TCP_WRITE_FLAG_COPY) {
		//data = new uint8_t[(int)(len * 1.5)];
		//memcpy(data, dataptr, len);
	}


	SHTTP_LOGI(__FUNCTION__, "calling mbedtls_ssl_write()");

	conn->lastPlainTextSize = len;
	conn->lastPlainTextPtr = data;
	auto result = mbedtls_ssl_write(&conn->ssl, (const unsigned char*)dataptr, len);
	SHTTP_LOGI(__FUNCTION__, "len %d result %d", len, result);
	if (result == MBEDTLS_ERR_SSL_WANT_WRITE) {
		SHTTP_LOGI(__FUNCTION__, "mbedtls_ssl_write blocking pending send of %d", (int)len);
	}
	if (result >= 0 || result == MBEDTLS_ERR_SSL_WANT_WRITE) {
		return len;
	}
	else {
		auto strErr = mbedtls_high_level_strerr(result);
		if (strErr != nullptr) {
			SHTTP_LOGE(__FUNCTION__, "mbedtls_ssl_write() failed with %s", strErr);
		}
		else {
			SHTTP_LOGE(__FUNCTION__, "mbedtls_ssl_write() failed with %d", result);
		}

		return ERR_BUF;
	}
}

bool SecureServer::queueChunksForSend(SSLConnection* conn, uint8_t* data, int len) {

	SHTTP_LOGD(__FUNCTION__, "length %d", len);

	uint8_t* buf = data;
	uint8_t chunkFlags = 0;
	///if under the max size take a copy to allow other packets to be sent in parallel
	if (len < maxSendSize) {
		buf = new uint8_t[len];
		memcpy(buf, data, len);
		chunkFlags |= ChunkForSendFlagMem;
	}


	int toSend = len;
	uint8_t* dataToSend = buf;
	while (toSend > 0) {
		uint16_t size = toSend < maxSendSize ? toSend : maxSendSize;

		ChunkForSend c{
			dataToSend,
			size,
			((uint16_t)toSend) == size ? conn->lastPlainTextSize : (uint16_t)0,
			conn->lastPlainTextPtr,
			chunkFlags
		};

		SHTTP_LOGI(__FUNCTION__, "ChunkForSend, size %d", (int)size);

		conn->writeQueue.push(c);

		dataToSend += size;
		toSend -= size;

	}

	return chunkFlags & ChunkForSendFlagMem;
}

err_t  SecureServer::sendNextChunk(SSLConnection* conn) {
	SHTTP_LOGI(__FUNCTION__, "%d chunks in queue", conn->writeQueue.size());
	while (!conn->writeQueue.empty()) {
		auto current = conn->writeQueue.front();


		if (conn->writeBufWaitingAck + current.size > maxSendSize) {
			SHTTP_LOGI(__FUNCTION__, "sendNextChunk stopping at %d next chunk %d", conn->writeBufWaitingAck, (int)current.size);
			break;
		}

		SHTTP_LOGI(__FUNCTION__, "sending addr %d %d(%d)", (int)current.data, (int)current.size, (int)current.plainTextSize);

		auto result = tcp_write(conn->tpcb, current.data, current.size, 0);
		if (result == ERR_OK) {
			conn->writeBufWaitingAck += current.size;

			if ((current.flags & ChunkForSendFlagMem) == 0) {
				conn->writeBlockedWaitingOnAckForSize = current.plainTextSize;
				//conn->writeIsBlocked = true;
			}

			current.flags |= ChunkForSendFlagSent;
			conn->sentWaitingAckQueue.push(current);
			conn->writeQueue.pop();
		}
		else {
			SHTTP_LOGE(__FUNCTION__, "write returned %d", result);
			return result;
		}

	}

	return tcp_output(conn->tpcb);
}

int SecureServer::mbedtls_tcp_send(void* ctx, const unsigned char* buf, size_t len) {

	auto conn = static_cast<SSLConnection*>(ctx);
	SHTTP_LOGI(__FUNCTION__, "length %d waiting on send of %d", (int)len, (int)0);

	if (conn->writeBlockedWaitingOnAckForSize) {
		if (conn->clearWriteBlock) {
			conn->writeBlockedWaitingOnAckForSize = 0;
			conn->clearWriteBlock = false;
			SHTTP_LOGI(__FUNCTION__, "finished blocking write");
			return len;
		}
		else {
			SHTTP_LOGE(__FUNCTION__, "blocked");
			return MBEDTLS_ERR_SSL_WANT_WRITE;
		}
	}

	auto wasCopied = queueChunksForSend(conn, (uint8_t*)buf, len);

	auto result = sendNextChunk(conn);
	if (result != ERR_OK) {
		return MBEDTLS_ERR_NET_SEND_FAILED;
	}

	//if the data was not copied this method needs to go into a "blocking" like state
	if (wasCopied) {
		return len;
	}
	return MBEDTLS_ERR_SSL_WANT_WRITE;
}

err_t SecureServer::tcp_sent_cb(void* arg, struct tcp_pcb* tpcb, u16_t len)
{
	if (arg != 0)
	{
		auto conn = static_cast<SSLConnection*>(arg);
		SHTTP_LOGI(__FUNCTION__, "len %d, waiting on %d blocked %d, waiting processing %d", (int)len, (int)conn->writeBufWaitingAck, (int)conn->writeBlockedWaitingOnAckForSize, conn->lastRemainingLength);

		conn->writeBufWaitingAck -= len;

		//the write method is in a "blocking state don't notify the plan text layer until all the chunks have been sent
		//in that case all are acked at once only the final one holds the sent size

		int size = conn->lastRemainingLength + len;
		if (size >= conn->writeBlockedWaitingOnAckForSize) {
			SHTTP_LOGI(__FUNCTION__, "waiting on ack for %d chunks", (int)conn->sentWaitingAckQueue.size());
			while (!conn->sentWaitingAckQueue.empty()) {
				const ChunkForSend n = conn->sentWaitingAckQueue.front();
				SHTTP_LOGI(__FUNCTION__, "size %d, chunk size %d(%d), acked %d", size, (int)n.size, (int)n.plainTextSize, (int)len);
				if (size - n.size < 0) {
					break;
				}

				size -= n.size;
				conn->sentWaitingAckQueue.pop();
				//if mbedtls_ssl_write is in a "blocking" state i.e returned want write unblock it
				//by calling it with the original arguments
				if ((n.flags & ChunkForSendFlagMem) == 0 && n.plainTextSize != 0) {
					conn->clearWriteBlock = true;
					auto writeResult = mbedtls_ssl_write(&conn->ssl, (const unsigned char*)n.plainTextPtr, n.plainTextSize);
					SHTTP_LOGI(__FUNCTION__, "mbedtls_ssl_write returned %d", (int)writeResult);
				}
				if (conn->writeQueue.empty()) {
					// notify the plain text layer the data has been sent if the next check is non blocking
					SHTTP_LOGI(__FUNCTION__, "calling sendCompleteCallback(%d)", (int)n.plainTextSize);
					conn->conn->sendCompleteCallback(n.plainTextSize + conn->plainTextLengthPendingNotification);
                    conn->plainTextLengthPendingNotification = 0;
				}else {
                    conn->plainTextLengthPendingNotification += n.plainTextSize;
                }

				//if the data was copied to a new pointer free it now 
				if (n.flags & ChunkForSendFlagMem) {
					delete n.data;
				}
			}

			if (conn->writeBufWaitingAck <= 0 && conn->sentWaitingAckQueue.empty()) {
				auto result = sendNextChunk(conn);
				if (result != ERR_OK) {
					SHTTP_LOGI(__FUNCTION__, "ERROR %d", (int)result);
				}
			}
		}
		conn->lastRemainingLength = size;

	}

	return ERR_OK;
}

void SecureServer::tcp_err_cb(void* arg, err_t err)
{
	if (arg != 0)
	{
		auto conn = static_cast<SSLConnection*>(arg);
		cleanup(conn);
		SHTTP_LOGI(__FUNCTION__, "err %d", (int)err);
	}
}

int SecureServer::sslSessionProcess(SSLConnection* conn) {

	if(!mbedtls_ssl_is_handshake_over(&conn->ssl))
	{
		auto result = mbedtls_ssl_handshake(&conn->ssl);
		if (result != 0) {
			if (!(result == MBEDTLS_ERR_SSL_WANT_READ || result == MBEDTLS_ERR_SSL_WANT_WRITE)) {
				auto strErr = mbedtls_high_level_strerr(result);
				if (strErr != nullptr) {
					SHTTP_LOGI(__FUNCTION__, "mbedtls_ssl_handshake() failed with %s", strErr);
				}
				else {
					SHTTP_LOGI(__FUNCTION__, "mbedtls_ssl_handshake() failed with %d", result);
				}
				return result;
			}
		}

	}
	else
	{
		//unsigned char buffer[512] = "";

		while (true) {
			auto result = mbedtls_ssl_read(&conn->ssl, conn->recvBuf, sizeof(conn->recvBuf) - 1);
			if (result > 0) {
				conn->recvBuf[result] = 0;
				SHTTP_LOGD(__FUNCTION__,"got [%s]", conn->recvBuf);
				conn->conn->dataReceived(conn->conn->dataReceivedArg, (uint8_t*)conn->recvBuf, result);
			}
			else if (!(result == MBEDTLS_ERR_SSL_WANT_READ || result == MBEDTLS_ERR_SSL_WANT_WRITE)) {

				auto strErr = mbedtls_high_level_strerr(result);
				if (strErr != nullptr) {
					SHTTP_LOGI(__FUNCTION__, "mbedtls_ssl_read() failed with %s", strErr);
				}
				else {
					SHTTP_LOGI(__FUNCTION__, "mbedtls_ssl_read() failed with %d", result);
				}
				break;
			}

			if (mbedtls_ssl_check_pending(&conn->ssl) == 0) {
				break;
			}
		}
	}
	return 0;
}

int SecureServer::loadPrivateKey(SimpleString* pk) {
	mbedtls_pk_init(&pkey);
	return mbedtls_pk_parse_key(&pkey, (const unsigned char*)pk->value,
		pk->size, NULL, 0,
		mbedtls_ctr_drbg_random, &ctr_drbg);
}

int SecureServer::loadCert(SimpleString* cert) {
	if (!crtInitDone) {
		mbedtls_x509_crt_init(&srvcert);
		crtInitDone = true;
	}

	return mbedtls_x509_crt_parse(&srvcert, (unsigned const char*)cert->value, cert->size);
}

mbedtls_x509_crt* SecureServer::getCertChain() {
	if(!crtInitDone){
		return 0;
	}
	return &srvcert;
}

int SecureServer::TLSInit() {
	//mbedtls_net_init(&listen_fd);
	//mbedtls_net_init(&client_fd);
	mbedtls_ssl_config_init(&conf);
#if defined(MBEDTLS_SSL_CACHE_C)
	mbedtls_ssl_cache_init(&cache);
#endif

	mbedtls_entropy_init(&entropy);
	mbedtls_ctr_drbg_init(&ctr_drbg);

	const char* pers = __FUNCTION__;

	auto result = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
		(const unsigned char*)pers,
		strlen(pers));
	if (result != 0) {
		return result;
	}

	result = mbedtls_ssl_config_defaults(&conf,
		MBEDTLS_SSL_IS_SERVER,
		MBEDTLS_SSL_TRANSPORT_STREAM,
		MBEDTLS_SSL_PRESET_DEFAULT);
	if (result != 0) {
		return result;
	}

	mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);

#if defined(MBEDTLS_SSL_CACHE_C)
	mbedtls_ssl_conf_session_cache(&conf, &cache,
		mbedtls_ssl_cache_get,
		mbedtls_ssl_cache_set);
#endif

	mbedtls_ssl_conf_ca_chain(&conf, srvcert.next, NULL);

	result = mbedtls_ssl_conf_own_cert(&conf, &srvcert, &pkey);
	if (result != 0) {
		return result;
	}

	return 0;
}

void SecureServer::listen(int port)
{

	struct tcp_pcb* tmp = tcp_new();
	tcp_bind(tmp, IP4_ADDR_ANY, port);
	tcpServer = tcp_listen(tmp);
	tcp_accept(tcpServer, tcp_accept_cb);
	return;
}

struct tcp_pcb* SecureServer::tcpServer = 0;
mbedtls_entropy_context SecureServer::entropy;
mbedtls_ctr_drbg_context SecureServer::ctr_drbg;
mbedtls_ssl_config SecureServer::conf;
mbedtls_x509_crt SecureServer::srvcert;
mbedtls_pk_context SecureServer::pkey;
mbedtls_ssl_cache_context SecureServer::cache;
bool SecureServer::crtInitDone = false;
