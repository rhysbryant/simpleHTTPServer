#include "SecureServerConnection.h"
#include "log.h"

using namespace SimpleHTTP;

SecureServerConnection::SecureServerConnection(struct tcp_pcb* client,ServerConnection* planTextLayerConn){
	this->tpcb = client;
	this->planTextLayerConn = planTextLayerConn;



	bufTail.payload = nullptr;
	bufTail.len = 0;
	lastRemainingLength = 0;
	writeBlockedWaitingOnAckForSize = 0;
	clearWriteBlock = false;
	plainTextLengthPendingNotification = 0;
	writeBufWaitingAck = 0;
	lastPlainTextSize = 0;
    lastPlainTextPtr = 0;

	planTextLayerConn->init(client,plainTextLayerWriteCallback);

}

int SecureServerConnection::initSSLContext(mbedtls_ssl_config* conf,mbedtls_ssl_send_t *f_send, mbedtls_ssl_recv_t *f_recv){
    mbedtls_ssl_init(&ssl);
    auto result = mbedtls_ssl_setup(&ssl, conf);
    if(result == 0){
        mbedtls_ssl_set_bio(&ssl, this, mbedtlsTCPSendCallback, mbedtlsTCPRecvCallback, NULL);
		//maxSendSize = mbedtls_ssl_get_max_out_record_payload(&ssl);
    }
    return result;
}

SecureServerConnection::~SecureServerConnection(){
	mbedtls_ssl_free(&ssl);
	while(!readQueue.empty()){
		pbuf_free(readQueue.front());
		readQueue.pop();
	}

	while(!writeQueue.empty()){
		auto item = writeQueue.front();
		if(item.flags & ChunkForSendFlagMem){
			delete item.data;
		}
		writeQueue.pop();
	}
	planTextLayerConn->init(0);
	
}

bool SecureServerConnection::queueSSLDataForSend(uint8_t* data, int len) {

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
			((uint16_t)toSend) == size ? lastPlainTextSize : (uint16_t)0,
			lastPlainTextPtr,
			chunkFlags
		};

		SHTTP_LOGI(__FUNCTION__, "ChunkForSend, size %d", (int)size);

		writeQueue.push(c);

		dataToSend += size;
		toSend -= size;

	}

	return chunkFlags & ChunkForSendFlagMem;
}

err_t  SecureServerConnection::sendNextSSLDataChunk() {
	SHTTP_LOGI(__FUNCTION__, "%d chunks in queue", writeQueue.size());
	while (!writeQueue.empty()) {
		auto current = writeQueue.front();


		if (writeBufWaitingAck + current.size > maxSendSize) {
			SHTTP_LOGI(__FUNCTION__, "sendNextChunk stopping at %d next chunk %d", writeBufWaitingAck, (int)current.size);
			break;
		}

		SHTTP_LOGI(__FUNCTION__, "sending addr %d %d(%d)", (int)current.data, (int)current.size, (int)current.plainTextSize);

		auto result = tcp_write(tpcb, current.data, current.size, 0);
		if (result == ERR_OK) {
			writeBufWaitingAck += current.size;

			if ((current.flags & ChunkForSendFlagMem) == 0) {
				writeBlockedWaitingOnAckForSize = current.plainTextSize;
				//writeIsBlocked = true;
			}

			current.flags |= ChunkForSendFlagSent;
			sentWaitingAckQueue.push(current);
			writeQueue.pop();
		}
		else {
			SHTTP_LOGE(__FUNCTION__, "write returned %d", result);
			return result;
		}

	}

	return tcp_output(tpcb);
}

bool SecureServerConnection::isReviceQueueEmpty(){
    return readQueue.empty();
}

pbuf* SecureServerConnection::getNextBufferForRead() {
	if (readQueue.empty()) {
		SHTTP_LOGI(__FUNCTION__, "queue empty");
		return 0;
	}
	//if nothing left in the current buffer free it and move to the next
	if (bufTail.len == 0) {
		//if there was a buffer before free it 
		if (bufTail.payload != nullptr) {

			auto current = readQueue.front();
			SHTTP_LOGI(__FUNCTION__, "freeing buffer, size %d", current->len);
			tcp_recved(tpcb, current->len);
			pbuf_free(current);
			readQueue.pop();
			bufTail.payload = nullptr;
		}

		if (!readQueue.empty()) {
			bufTail = *readQueue.front();
			SHTTP_LOGI(__FUNCTION__, "assigning buffer, size %d", bufTail.len);
		}
		else {
			SHTTP_LOGI(__FUNCTION__, "queue now empty heap free %d min seen %d", (int)esp_get_free_heap_size(), (int)esp_get_minimum_free_heap_size());
			return 0;
		}
	}

	return &bufTail;
}

int SecureServerConnection::sslSessionProcess(pbuf* newData) {
    if(newData != nullptr){
		SHTTP_LOGI(__FUNCTION__, "got %d bytes putting, queuing", newData->len);
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
			if (result > 0) {
				recvBuf[result] = 0;
				SHTTP_LOGD(__FUNCTION__, "got [%s]", recvBuf);
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

int SecureServerConnection::writeData(const void* dataptr, u16_t len, u8_t apiflags){
	uint8_t* data = (uint8_t*)dataptr;

	SHTTP_LOGI(__FUNCTION__, "calling mbedtls_ssl_write()");

	lastPlainTextSize = len;
	lastPlainTextPtr = data;
	auto result = mbedtls_ssl_write(&ssl, (const unsigned char*)dataptr, len);
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

int SecureServerConnection::plainTextLayerWriteCallback(tcp_pcb* pcb, const void* dataptr, u16_t len, u8_t apiflags) {
	return static_cast<SecureServerConnection*>(pcb->callback_arg)->plainTextLayerWriteCallback(dataptr,len,apiflags);
}

int SecureServerConnection::plainTextLayerWriteCallback(const void* dataptr, u16_t len, u8_t apiflags) {
	if (len == 0) {
		return ERR_OK;
	}

	return writeData(dataptr,len,apiflags);
}

int SecureServerConnection::mbedtlsTCPSendCallback(void* ctx, const unsigned char* buf, size_t len) {
	return static_cast<SecureServerConnection*>(ctx)->mbedtlsTCPSendCallback(buf,len);
}

int SecureServerConnection::mbedtlsTCPSendCallback(const unsigned char* buf, size_t len){
	SHTTP_LOGI(__FUNCTION__, "length %d waiting on send of %d", (int)len, (int)0);

	if (writeBlockedWaitingOnAckForSize) {
		if (clearWriteBlock) {
			writeBlockedWaitingOnAckForSize = 0;
			clearWriteBlock = false;
			SHTTP_LOGI(__FUNCTION__, "finished blocking write");
			return len;
		}
		else {
			SHTTP_LOGE(__FUNCTION__, "blocked");
			return MBEDTLS_ERR_SSL_WANT_WRITE;
		}
	}

	auto wasCopied = queueSSLDataForSend((uint8_t*)buf, len);

	auto result = sendNextSSLDataChunk();
	if (result != ERR_OK) {
		return MBEDTLS_ERR_NET_SEND_FAILED;
	}

	//if the data was not copied this method needs to go into a "blocking" like state
	if (wasCopied) {
		return len;
	}
	return MBEDTLS_ERR_SSL_WANT_WRITE;
}

int SecureServerConnection::mbedtlsTCPRecvCallback(void* ctx, unsigned char* buf, size_t len) {
	return static_cast<SecureServerConnection*>(ctx)->mbedtlsTCPRecvCallback(buf,len);
}


int SecureServerConnection::mbedtlsTCPRecvCallback(unsigned char* buf, size_t len) {

	if (isReviceQueueEmpty()) {
		SHTTP_LOGI(__FUNCTION__, "want %d bytes not here yet", (int)len);
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
	SHTTP_LOGI(__FUNCTION__, "len %d, waiting on %d blocked %d, waiting processing %d", (int)len, (int)writeBufWaitingAck, (int)writeBlockedWaitingOnAckForSize, lastRemainingLength);

	writeBufWaitingAck -= len;

	//the write method is in a "blocking state don't notify the plan text layer until all the chunks have been sent
	//in that case all are acked at once only the final one holds the sent size

	int size = lastRemainingLength + len;
	if (size >= writeBlockedWaitingOnAckForSize) {
		SHTTP_LOGI(__FUNCTION__, "waiting on ack for %d chunks", (int)sentWaitingAckQueue.size());
		while (!sentWaitingAckQueue.empty()) {
			const ChunkForSend n = sentWaitingAckQueue.front();
			SHTTP_LOGI(__FUNCTION__, "size %d, chunk size %d(%d), acked %d", size, (int)n.size, (int)n.plainTextSize, (int)len);
			if (size - n.size < 0) {
				break;
			}

			size -= n.size;
			sentWaitingAckQueue.pop();
			//if mbedtls_ssl_write is in a "blocking" state i.e returned want write unblock it
			//by calling it with the original arguments
			if ((n.flags & ChunkForSendFlagMem) == 0 && n.plainTextSize != 0) {
				clearWriteBlock = true;
				auto writeResult = mbedtls_ssl_write(&ssl, (const unsigned char*)n.plainTextPtr, n.plainTextSize);
				SHTTP_LOGI(__FUNCTION__, "mbedtls_ssl_write returned %d", (int)writeResult);
			}
			if (writeQueue.empty()) {
				// notify the plain text layer the data has been sent if the next check is non blocking
				SHTTP_LOGI(__FUNCTION__, "calling sendCompleteCallback(%d)", (int)n.plainTextSize);
				planTextLayerConn->sendCompleteCallback(n.plainTextSize + plainTextLengthPendingNotification);
				plainTextLengthPendingNotification = 0;
			}
			else {
				plainTextLengthPendingNotification += n.plainTextSize;
			}

			//if the data was copied to a new pointer free it now 
			if (n.flags & ChunkForSendFlagMem) {
				delete n.data;
			}
		}

		if (writeBufWaitingAck <= 0 && sentWaitingAckQueue.empty()) {
			auto result = sendNextSSLDataChunk();
			if (result != ERR_OK) {
				SHTTP_LOGI(__FUNCTION__, "ERROR %d", (int)result);
			}
		}
	}
	lastRemainingLength = size;
	return OK;
}