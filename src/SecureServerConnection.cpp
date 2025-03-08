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
#include "SecureServerConnection.h"
#include "log.h"

using namespace SimpleHTTP::Internal;
using namespace SimpleHTTP;

SecureServerConnection::SecureServerConnection() {
	mbedtls_ssl_init(&ssl);
	firstUse = true;

}

void SecureServerConnection::init(struct tcp_pcb* client, ServerConnection* planTextLayerConn) {
	this->tpcb = client;
	this->planTextLayerConn = planTextLayerConn;

	while (!readQueue.empty()) {
		pbuf_free(readQueue.front());
		readQueue.pop();
	}

	bufTail.payload = nullptr;
	bufTail.next = nullptr;
	bufTail.len = 0;
	writeBufWaitingAck = 0;
	lastPlainTextSize = 0;
	lastPlainTextPtr = 0;

	incompleteOutLength = 0;
	incompleteOutPtr = nullptr;

	planTextLayerConn->init(client, this);

}

int SecureServerConnection::initSSLContext(mbedtls_ssl_config* conf) {
	int result = -1;

	result = mbedtls_ssl_setup(&ssl, conf);
	if (result != 0) {
		firstUse = false;
		return result;
	}


	if (result == 0) {
		mbedtls_ssl_set_bio(&ssl, this, mbedtlsTCPSendCallback, mbedtlsTCPRecvCallback, NULL);
	}
	return result;
}

SecureServerConnection::~SecureServerConnection() {

	while (!readQueue.empty()) {
		pbuf_free(readQueue.front());
		readQueue.pop();
	}
	planTextLayerConn->runFreeSessionHandler();
	planTextLayerConn->dataReceived(planTextLayerConn->dataReceivedArg, 0, 0);
	planTextLayerConn->init(0);

}

bool SecureServerConnection::isReviceQueueEmpty() {
	return readQueue.empty();
}

pbuf* SecureServerConnection::getNextBufferForRead() {
	if (readQueue.empty()) {
		SHTTP_LOGD(__FUNCTION__, "queue empty");
		return 0;
	}
	//if nothing left in the current buffer free it and move to the next
	if (bufTail.len == 0) {
		//if there was a buffer before free it 
		if (bufTail.payload != nullptr) {
			if (bufTail.next != nullptr) {
				SHTTP_LOGE(__FUNCTION__, "unimplemented chained buffer");
			}

			auto current = readQueue.front();
			SHTTP_LOGD(__FUNCTION__, "freeing buffer, size %d", current->len);
			tcp_recved(tpcb, current->len);
			pbuf_free(current);
			readQueue.pop();
			bufTail.payload = nullptr;
		}

		if (!readQueue.empty()) {
			bufTail = *readQueue.front();
			SHTTP_LOGD(__FUNCTION__, "assigning buffer, size %d", bufTail.len);
		}
		else {
			SHTTP_LOGD(__FUNCTION__, "queue now empty heap free %d min seen %d", (int)esp_get_free_heap_size(), (int)esp_get_minimum_free_heap_size());
			return 0;
		}
	}

	return &bufTail;
}

int SecureServerConnection::sslSessionProcess(pbuf* newData) {
	if (newData != nullptr) {
		SHTTP_LOGD(__FUNCTION__, "got %d bytes putting, queuing", newData->len);
		readQueue.push(newData);
	}

	if (!mbedtls_ssl_is_handshake_over(&ssl))
	{
		auto result = mbedtls_ssl_handshake(&ssl);
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

		while (true) {
			auto result = mbedtls_ssl_read(&ssl, recvBuf, sizeof(recvBuf) - 1);
			if (result >= 0) {
				recvBuf[result] = 0;
				SHTTP_LOGD(__FUNCTION__, "got payload [%s]", recvBuf);
				planTextLayerConn->dataReceived(planTextLayerConn->dataReceivedArg, (uint8_t*)recvBuf, result);
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

			if (mbedtls_ssl_check_pending(&ssl) == 0) {
				break;
			}
		}
	}
	return 0;
}

int SecureServerConnection::write(const void* dataptr, u16_t len, u8_t apiflags) {

	if (len == 0) {
		return ERR_OK;
	}

	uint8_t* data = (uint8_t*)dataptr;

	lastPlainTextSize = len;
	lastPlainTextPtr = data;
	auto result = mbedtls_ssl_write(&ssl, (const unsigned char*)dataptr, len);
	SHTTP_LOGD(__FUNCTION__, "len %d result %d", len, result);
	if (result == MBEDTLS_ERR_SSL_WANT_WRITE) {
		SHTTP_LOGD(__FUNCTION__, "mbedtls_ssl_write blocking pending send of %d", (int)len);
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


int SecureServerConnection::mbedtlsTCPSendCallback(void* ctx, const unsigned char* buf, size_t len) {
	return static_cast<SecureServerConnection*>(ctx)->mbedtlsTCPSendCallback(buf, len);
}

int SecureServerConnection::mbedtlsTCPSendCallback(const unsigned char* buf, size_t len) {
	SHTTP_LOGD(__FUNCTION__, "length %d waiting on send of %d", (int)len, (int)0);

	//if the last call failed ensure we are continuing from where we left off
	if (((const unsigned char*)incompleteOutPtr) != buf && incompleteOutLength > 0) {
		SHTTP_LOGE(__FUNCTION__, "is blocked waiting for buffer space");
		return MBEDTLS_ERR_SSL_WANT_WRITE;
	}


	auto bufLen = tcp_sndbuf(tpcb);

	err_t  err;
	int written = 0;
	if (bufLen > 0) {
		incompleteOutLength = 0;
		if (bufLen > len) {
			err = tcp_write(tpcb, buf, len, TCP_WRITE_FLAG_COPY);
			written = len;
		}
		else {
			err = tcp_write(tpcb, buf, bufLen, TCP_WRITE_FLAG_COPY);
			written = bufLen;
		}

		if (err != ERR_OK) {
			SHTTP_LOGE(__FUNCTION__, "tcp_write failed %d %d", (int)err, (int)len);
			return MBEDTLS_ERR_SSL_BUFFER_TOO_SMALL;
		}
		auto oErr = tcp_output(tpcb);
		if (oErr != ERR_OK) {
			SHTTP_LOGE(__FUNCTION__, "tcp_output failed %d %d", (int)err, (int)len);
			return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
		}
		writeBufWaitingAck += written;
		return written;
	}
	else {
		incompleteOutLength = len;
		incompleteOutPtr = (char*)buf;
		//no more buffer space signal up the stack write will need to be called again
		return MBEDTLS_ERR_SSL_WANT_WRITE;
	}

}

int SecureServerConnection::mbedtlsTCPRecvCallback(void* ctx, unsigned char* buf, size_t len) {
	return static_cast<SecureServerConnection*>(ctx)->mbedtlsTCPRecvCallback(buf, len);
}


int SecureServerConnection::mbedtlsTCPRecvCallback(unsigned char* buf, size_t len) {

	if (isReviceQueueEmpty()) {
		SHTTP_LOGD(__FUNCTION__, "want %d bytes not here yet", (int)len);
		return MBEDTLS_ERR_SSL_WANT_READ;
	}

	int readLen = 0;

	while (auto pBuf = getNextBufferForRead()) {

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

Result SecureServerConnection::sendCompleteCallback(int len) {
	SHTTP_LOGD(__FUNCTION__, "len %d, waiting on %d blocked, waiting processing", (int)len, (int)writeBufWaitingAck);

	writeBufWaitingAck -= len;
	//write is incomplete (last call could not complete) and is waiting for more space to continue
	//call again with the same arguments
	//if sucsessfull more data will be written from the ssl output buf
	if (incompleteOutLength > 0) {
		auto result = write(lastPlainTextPtr, lastPlainTextSize, 0);
		if (result < 0) {
			SHTTP_LOGE(__FUNCTION__, "call to write failed, %d", result);
			return ERROR;
		}
	}
	else {
		//there is nothing in the ssl output buf request more data from the plan text layer
		auto result = planTextLayerConn->sendCompleteCallback(lastPlainTextSize);
		if (result == ERROR) {
			SHTTP_LOGE(__FUNCTION__, "pt sendCompleteCallback failed");
			return ERROR;
		}
	}


	return OK;
}

err_t SecureServerConnection::shutdown() {
	tcp_arg(tpcb, nullptr);
	tcp_close(tpcb);
	tpcb = 0;
	mbedtls_ssl_free(&ssl);
	return 0;
}

void SecureServerConnection::close() {
	planTextLayerConn->close();
}

void SecureServerConnection::closeWithoutLock() {
	planTextLayerConn->closeWithOutLocking();
}

int SecureServerConnection::getAvailableSendBuffer() {
	return tcp_sndbuf(tpcb);
}