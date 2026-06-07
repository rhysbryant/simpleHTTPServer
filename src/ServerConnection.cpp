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
#include "ServerConnection.h"
#include "log.h"

using namespace SimpleHTTP;

bool ServerConnection::isConnected() {
	return transport != 0;
}

ServerConnection::ServerConnection() {
	init(0);
}

void ServerConnection::init(struct tcp_pcb* client) {

	forceNoLocking = false;
	hijacted = false;
	closeOnceSent = 0;
	waitingForSendCompleteSize = 0;
	bytesSentTotal = 0;
	// new generation each (re)use so a stale detached Response can detect reuse
	generation++;

	lastRequestTime = 0;

	dataReceived = parseRequest;
	dataReceivedArg = this;


	sessionArg = 0;
	sessionArgFreeHandler = 0;
	this->currentRequest.reset();
	if (client != 0) {
		this->defaultTransport.setPCB(client);
		this->transport = &defaultTransport;
	}
	else {
		this->transport = 0;
	}

	while (!sendQueue.empty()) {
		auto& p = sendQueue.front();
		if (p.type == ChunkForSend::CallBackPtr) {
			p.getMoreData(this, (void*)p.arg);
		}
		sendQueue.pop();
	}
}

void ServerConnection::init(struct tcp_pcb* client, Transport* t) {
	init(client);
	if (t != nullptr) {
		this->transport = t;
	}
}

Result ServerConnection::parseRequest(void* arg, uint8_t* data, uint16_t len) {
	if (data == 0) {
		//object is being reset for reuse
		return OK;
	}

	auto conn = static_cast<ServerConnection*>(arg);
	auto result = conn->currentRequest.parse((char*)data, len);
	if (result == ERROR) {
		conn->close();
		return ERROR;
	}

	return OK;
}

bool ServerConnection::writeData(const uint8_t* data, int len, int writeFlags) {
	if (!isConnected()) {
		return false;
	}
	int apiFlags = writeFlags;//(writeFlags & Transport::WriteFlagZeroCopy) ? 0 : TCP_WRITE_FLAG_COPY;
	bool locked = false;
	if (!forceNoLocking && (writeFlags & Transport::WriteFlagNoLock) == 0) {
		LOCK_TCPIP_CORE();
		locked = true;
	}

	//the queue only supports zero copy
	if (sendQueue.empty() || !(apiFlags & Transport::WriteFlagZeroCopy)) {
		//try to directly send the first chunk
		int size = len > maxSendSize && apiFlags & Transport::WriteFlagZeroCopy ? maxSendSize : len;
		int dataLengthWritten = 0;


		dataLengthWritten = transport->write(data, size, apiFlags);


		if (dataLengthWritten < 0) {
			if (locked) {
				UNLOCK_TCPIP_CORE();
			}
			return false;
		}

		len -= size;
		data += size;
		waitingForSendCompleteSize += dataLengthWritten;
	}
	//queueChunks:

	if (((apiFlags & Transport::WriteFlagZeroCopy) && len > 0)) {
		//put any remaining data on the queue
		int toSend = len;
		const uint8_t* dataToSend = data;
		while (toSend > 0) {
			uint16_t size = toSend < maxSendSize ? toSend : maxSendSize;

			ChunkForSend c{
				.type = ChunkForSend::DataPtr,
				.data = dataToSend,
				.size = size
			};

			sendQueue.push(c);

			dataToSend += size;
			toSend -= size;

		}
	}

	if (locked) {
		UNLOCK_TCPIP_CORE();
	}

	return true;


}

void ServerConnection::queueResponseWriteCallback(MoreDataCallback callback, void* arg) {
	ChunkForSend c{
		.type = ChunkForSend::CallBackPtr,
		.getMoreData = callback,
		.arg = arg
	};
	LOCK_TCPIP_CORE();
	auto empty = sendQueue.empty();
	sendQueue.push(c);
	//trigger the inital queue processing if it was empty
	if (empty) {

		sendNextFromQueue();

	}
	UNLOCK_TCPIP_CORE();
}

bool ServerConnection::sendNextFromQueue() {

	ChunkForSend c = sendQueue.front();

	int dataWritten = 0;
	switch (c.type) {
	case ChunkForSend::DataPtr:
		dataWritten = transport->write((uint8_t*)c.data, c.size, WriteFlagZeroCopy);
		break;
	case ChunkForSend::CallBackPtr:
	{
		//get more data will likely have made calls to write() so need to ensure
		//if the chunk is to be re-queued it's placed at the back
		sendQueue.pop();
		forceNoLocking = true;
		auto done = c.getMoreData(this, (void*)c.arg);
		forceNoLocking = false;
		if (!done) {
			sendQueue.push(c);
		}
		goto skipPop;
	}
	break;
	}


	if (dataWritten >= 0) {
		sendQueue.pop();
	skipPop:
		waitingForSendCompleteSize += dataWritten;
		//auto err = tcp_output(client);
		return true;
	}
	else {
		return false;
	}
}

Result ServerConnection::sendCompleteCallback(int length) {
	//we are in lwip context here don't lock here (it's expected this is called from tcp_sent_cb)
	//SHTTP_LOGI(__FUNCTION__,"waiting on size %d, buffer %d",waitingForSendCompleteSize,(int)transport->getAvailableSendBuffer());
	if (waitingForSendCompleteSize || !sendQueue.empty()) {
		if (waitingForSendCompleteSize) {
			waitingForSendCompleteSize -= length;
		}
		if (hasAvailableSendBuffer()) {
			if (!sendQueue.empty()) {
				if (!sendNextFromQueue()) {
					return ERROR;
				}
			}
			return MoreData;
		}
	}
	return OK;
}
