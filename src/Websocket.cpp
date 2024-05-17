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
extern "C" {
#include "cencode.h"
#if(!defined(_WIN32))
#include "freertos/semphr.h"
#endif
}
#include <stdint.h>

using SimpleHTTP::Websocket;
using SimpleHTTP::Result;

void Websocket::dataReceivedHandler(uint8_t *data, int dataSize){

	appendToBuffer((char*)data,dataSize);
}

Result Websocket::writeFrame(FrameType frameType,char* headerExta,const uint16_t headerExtraSize, char* payload, int size) {
	return writeFrame(conn,frameType,headerExta,headerExtraSize,payload,size);
}

Result Websocket::writeFrame(ServerConnection* conn,FrameType frameType,char* headerExta,const uint16_t headerExtraSize, char* payload, int size) {

	uint8_t header[5] = "";
	uint8_t* headerPtr = header + 1;
	header[0] = FlagFIN | frameType;
	uint16_t totalPayloadSize = size + headerExtraSize;

	if (totalPayloadSize <= 125) {
		*(headerPtr++) = (totalPayloadSize & 0xFF);
	}
	else if (size < 65536) {
		*(headerPtr++) = 126;
		*(headerPtr++) = (totalPayloadSize >> 8);
		*(headerPtr++) = (totalPayloadSize & 0xFF);
	}
	else {
		//for now only allow up to 16bit length
		return ERROR;
	}



	if(!conn->write(header,headerPtr - header) || !conn->write((uint8_t*)headerExta,headerExtraSize) ){//|| !conn->writeData((uint8_t*)payload,size,0)
		return ERROR;
	}

	if(!conn->flushData()){
		return ERROR;
	}

	return OK;
}

Result Websocket::appendToBuffer(char* data, int size) {
	//TODO FIFO
	if(requestBufferPos + size > requestBuffer + requestBufferSize && (size < requestBufferSize)){
		resetBuffer();
	}
	
	if (requestBufferPos + size > requestBuffer + requestBufferSize) {
		return ERROR;
	}

	memcpy(requestBufferPos, data, size);
	requestBufferPos += size;
	return MoreData;
}

int Websocket::readFrame(char* data, int dataSize, Frame* frame) {

	char* startPtr = data;

	if (dataSize >= 1) {
		if (frame) {
			frame->frameType = (FrameType)(*data & opCodeMask);
			frame->isFinalFrame = (*data & FlagFIN) == FlagFIN;
		}
		dataSize--;
		data++;
	}
	else {
		return 0;
	}

	bool masked = false;
	uint16_t payloadLength = 0;

	if (dataSize >= 1) {

		dataSize--;
		masked = (*data) & FlagMask;
		payloadLength = (*data) & (FlagMask - 1);
		data++;
		if (payloadLength == 126) {
			if (dataSize < 1) {
				return 1;//we need one more byte before we can continue
			}

			uint16_t len16 = (data[0] << 8) | data[1];
			payloadLength = len16;
			dataSize -= 2;
			data += 2;
		}
		else if (payloadLength == 127) {
			//TODO 64bit length
			return 0;
		}

	}
	else {
		return 0;
	}

	//more frame data coming
	int sizeNeeded = payloadLength + (masked ? 4 : 0);
	if (sizeNeeded > dataSize) {
		return 0;
	}else if(frame == nullptr) {
		//only checking we have all the data return what we read
		return (data - startPtr);
	}

	if (masked) {
		uint8_t mask[4] = {};
		memcpy(mask, data, sizeof(mask));
		data += 4;

		char* payload = data;

		for (int i = 0; i < payloadLength; i++) {
			payload[i] = (payload[i] ^ mask[i % sizeof(mask)]);
		}
	}

	frame->payload = (uint8_t*)data;
	frame->payloadLength = payloadLength;

	return (data - startPtr) + payloadLength;
}

int Websocket::readFrame(Frame *frame) {
	
	auto result = readFrame((char*)requestBuffer, requestBufferPos - requestBuffer , frame);
	
	return result;
}

bool Websocket::hasNextFrame() {
	return readFrame(0)>0;
}

Result Websocket::nextFrame(Frame *frame) {
	int bytesRead = readFrame(frame);
	if(bytesRead == 0){
		return ERROR;
	}
	//TODO add a FIFO
	if( requestBufferPos + bytesRead > (requestBuffer + requestBufferSize) ){
		resetBuffer();
	}else{
		requestBufferPos += bytesRead;
	}

	return OK;
}

SimpleHTTP::Result Websocket::sendCloseFrame(uint16_t code){
	closeRequestedByServer = true;
	return writeFrame(FrameTypeConnectionClose,(char*)&code,2,"",0);
}

bool Websocket::bufferLock(){
#if(defined(RTOS))
	return xSemaphoreTake(_bufferLock, 100 / portTICK_PERIOD_MS) == pdTRUE;
#else
	return true;
#endif
}

void Websocket::bufferUnLock(){
#if(defined(RTOS))
	xSemaphoreGive(_bufferLock);
#endif
}
