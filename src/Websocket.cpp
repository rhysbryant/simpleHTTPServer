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
#include "Websocket.h"
#include "libsha1.h"
//#include "esp_log.h"
extern "C"
{
#include "cencode.h"
#if (!defined(_WIN32))
#include "freertos/semphr.h"
#endif
}
#include <stdint.h>
#include "log.h"
using SimpleHTTP::Result;
using SimpleHTTP::Websocket;

void Websocket::dataReceivedHandler(uint8_t *data, int dataSize)
{
	
	if (recvBuffer.put((char *)data, dataSize) == ERROR)
	{
		SHTTP_LOGE(__FUNCTION__, "got %d bytes but buffer is full", dataSize);
	}
}

Result Websocket::writeFrame(FrameType frameType, char *headerExta, const uint16_t headerExtraSize, char *payload, int size)
{
	return writeFrame(conn, frameType, headerExta, headerExtraSize, payload, size);
}

Result Websocket::writeFrame(ServerConnection *conn, FrameType frameType, char *headerExta, const uint16_t headerExtraSize, char *payload, int size)
{
	uint8_t header[5] = "";
	uint8_t *headerPtr = header + 1;
	header[0] = FlagFIN | frameType;
	uint16_t totalPayloadSize = size + headerExtraSize;

	if (totalPayloadSize <= 125)
	{
		*(headerPtr++) = (totalPayloadSize & 0xFF);
	}
	else if (size < 65536)
	{
		*(headerPtr++) = 126;
		*(headerPtr++) = (totalPayloadSize >> 8);
		*(headerPtr++) = (totalPayloadSize & 0xFF);
	}
	else
	{
		// for now only allow up to 16bit length
		return ERROR;
	}

	LOCK_TCPIP_CORE();
	if (!conn->writeData(header, headerPtr - header, ServerConnection::WriteFlagNoFlush | ServerConnection::WriteFlagNoLock) ||
		!conn->writeData((uint8_t *)headerExta, headerExtraSize, ServerConnection::WriteFlagNoFlush | ServerConnection::WriteFlagNoLock))
	{ //|| !conn->writeData((uint8_t*)payload,size,0)
		UNLOCK_TCPIP_CORE();
		return ERROR;
	}
	UNLOCK_TCPIP_CORE();
	return OK;
}

int Websocket::readFrame(Frame *frame)
{

	recvBuffer.markTail();
	int dataSize = recvBuffer.backLogSize();
	int initalSize = dataSize;

	uint8_t opCodeAndType = 0;
	

	if (dataSize >= 1)
	{
		recvBuffer >> opCodeAndType;
		if (frame)
		{
			frame->frameType = (FrameType)(opCodeAndType & opCodeMask);
			frame->isFinalFrame = (opCodeAndType & FlagFIN) == FlagFIN;
		}
		dataSize--;
	}
	else
	{
		recvBuffer.resetTail();
		return 0;
	}

	bool masked = false;
	uint16_t payloadLength = 0;

	if (dataSize >= 1)
	{
		uint8_t maskAndFirstPayloadByte = 0;
		recvBuffer >> maskAndFirstPayloadByte;
		masked = (maskAndFirstPayloadByte)&FlagMask;
		payloadLength = (maskAndFirstPayloadByte) & (FlagMask - 1);

		if (payloadLength == 126)
		{
			if (dataSize < 1)
			{
				recvBuffer.resetTail();
				return 0; // we need one more byte before we can continue
			}
			char tmp[2];
			recvBuffer.get(tmp, sizeof(tmp));

			uint16_t len16 = (tmp[0] << 8) | tmp[1];
			payloadLength = len16;
			dataSize -= 2;
		}
		else if (payloadLength == 127)
		{
			// TODO 64bit length
			recvBuffer.resetTail();
			return 0;
		}
		dataSize--;
	}
	else
	{
		recvBuffer.resetTail();
		return 0;
	}

	// more frame data coming
	int sizeNeeded = payloadLength + (masked ? 4 : 0);
	if (sizeNeeded > dataSize)
	{
		recvBuffer.resetTail();
		return 0;
	}
	else if (frame == nullptr)
	{
		recvBuffer.resetTail();
		// only checking we have all the data return what we read
		return (initalSize - dataSize);
	}

	// unrecoverable error
	if (frame->payloadLength <= payloadLength)
	{
		recvBuffer.resetTail();
		return 0;
	}

	if (masked)
	{
		
		uint8_t mask[4] = {};
		dataSize -= sizeof(mask);
		recvBuffer.get((char *)mask, sizeof(mask));
		recvBuffer.get((char *)frame->payload, payloadLength);

		for (int i = 0; i < payloadLength; i++)
		{
			frame->payload[i] = (frame->payload[i] ^ mask[i % sizeof(mask)]);
		}
	}else{
		recvBuffer.get((char *)frame->payload, payloadLength);
	}
	dataSize -= payloadLength;

	frame->payloadLength = payloadLength;

	return (initalSize - dataSize);
}

bool Websocket::hasNextFrame()
{
	return readFrame(0) > 0;
}

Result Websocket::nextFrame(Frame *frame)
{
	int bytesRead = readFrame(frame);
	if (bytesRead == 0)
	{
		return ERROR;
	}

	return OK;
}

SimpleHTTP::Result Websocket::sendCloseFrame(uint16_t code)
{
	closeRequestedByServer = true;
	return writeFrame(FrameTypeConnectionClose, (char *)&code, 2, "", 0);
}

bool Websocket::bufferLock()
{
#if (defined(RTOS))
	return xSemaphoreTake(_bufferLock, 1000 / portTICK_PERIOD_MS) == pdTRUE;
#else
	return true;
#endif
}

void Websocket::bufferUnLock()
{
#if (defined(RTOS))
	xSemaphoreGive(_bufferLock);
#endif
}
