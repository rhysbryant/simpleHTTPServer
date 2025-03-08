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

using namespace SimpleHTTP;

bool ServerConnection::isConnected() {
	return transport != 0;
}

ServerConnection::ServerConnection() {
	init(0);
}

void ServerConnection::init(struct tcp_pcb* client) {

	hijacted = false;
	closeOnceSent = 0;
	waitingForSendCompleteSize = 0;

	lastRequestTime = 0;

	dataReceived = parseRequest;
	dataReceivedArg = this;

	while (!sendQueue.empty()) {
		sendQueue.pop();
	}

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

bool ServerConnection::writeData(uint8_t* data, int len, int writeFlags) {
	if (!isConnected()) {
		return false;
	}
	int apiFlags = (writeFlags & Transport::WriteFlagZeroCopy) ? 0 : TCP_WRITE_FLAG_COPY;
	bool locked = false;
	if ((writeFlags & Transport::WriteFlagNoLock) == 0) {
		LOCK_TCPIP_CORE();
		locked = true;
	}

	if (sendQueue.empty() || apiFlags != 0) {
		//try to directly send the first chunk
		int size = len > maxSendSize && apiFlags == 0 ? maxSendSize : len;
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

	if ((apiFlags == 0 && len > 0)) {
		//put any remaining data on the queue
		int toSend = len;
		uint8_t* dataToSend = data;
		while (toSend > 0) {
			uint16_t size = toSend < maxSendSize ? toSend : maxSendSize;

			ChunkForSend c{
				dataToSend,
				size
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

bool ServerConnection::sendNextFromQueue() {

	ChunkForSend c = sendQueue.front();
	int dataWritten = transport->write((uint8_t*)c.data, c.size, 0);
	if (dataWritten >= 0) {
		sendQueue.pop();
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
	if (waitingForSendCompleteSize) {
		waitingForSendCompleteSize -= length;
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
