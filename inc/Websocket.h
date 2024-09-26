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

#include "Request.h"
#include "Response.h"
#include "CBuffer.h"
#define RTOS
namespace SimpleHTTP
{
	class Websocket
	{
	public:
		typedef enum : uint8_t
		{
			FrameTypeText = 0x1,
			FrameTypeBin = 0x2,
			FrameTypeContinuation = 0x0,
			FrameTypeConnectionClose = 0x8,
			FrameTypePing = 0x9,
			FrameTypePong = 0xA
		} FrameType;

		typedef struct
		{
			uint8_t *payload;
			uint16_t payloadLength;
			FrameType frameType;
			bool isFinalFrame;
		} Frame;

		uint32_t lastPongReceived;
		uint32_t lastPingSent;

	private:
		static const int requestBufferSize = 2048;
#if(defined(_WIN32))
		int _bufferLock;
		inline int xSemaphoreCreateMutex() { return 0; }
#else
		SemaphoreHandle_t _bufferLock;
#endif
		ServerConnection *conn;
		
		char recvBufferBuff[requestBufferSize];
		CBuffer recvBuffer;		
		char* requestBufferPos;

		int readFrame(Frame *frame);
		bool closeRequestedByServer;
		static const int opCodeMask=0x7f;

	public:

		static const uint8_t FlagFIN = 128;
		static const uint8_t FlagMask = 128;

		bool isCloseRequestedByServer(){
			return closeRequestedByServer;
		};
		/**
		 * sends a close message to the client and waits for a response
		 * or close wait timeout
		*/
		SimpleHTTP::Result sendCloseFrame(uint16_t code);
		/*
		 * writes the websocket frame
		 * the headerExta data is written with the COPY flag (i.e copied to the LWIP heap)  where as payload is not
		 *
		 */
		Result writeFrame(FrameType frameType, char *headerExta, const uint16_t headerExtraSize, char *payload, int size);
		/*
		 * writes the websocket frame
		 * the headerExta data is written with the COPY flag (i.e copied to the LWIP heap)  where as payload is not
		 *
		 */
		static Result writeFrame(ServerConnection* conn,FrameType frameType, char *headerExta, const uint16_t headerExtraSize, char *payload, int size);
		/**
		*	parses data as a websocket frame and populates frame structure
		*	if the frame is incomplete returns the amount of data missing 
		**/
		int readFrame(char *data, int dataSize, Frame *frame);
		/*
		* returns true if a frame can be read from the internal buffer
		*/
		bool hasNextFrame();
		/*
		* reads out a frame from the internal buffer
		* and advances the position
		*/
		Result nextFrame(Frame *frame);
		/**
		* reset the internal buffer
		*/
		inline void resetBuffer() {
			recvBuffer.reset();
		}
		/**
		 * populates the internal buffer used by readFrame()
		 * this is called by WebsocketManager
		*/
		void dataReceivedHandler(uint8_t *data, int dataSize);
		/*
		* pooled object may exist without an associated connection
		* i.e not in use
		*/
		bool inline isInUse() {
			return conn != nullptr && conn->isConnected();
		}
		/**
		 * assign a connection to this pooled object
		*/
		void inline assign(ServerConnection* conn) {
			this->conn = conn;
		}
		/**
		 * remove a connection from this pooled object
		*/
		void inline unAssign() {
			conn = nullptr;
			lastPongReceived = 0;
			lastPingSent = 0;
			closeRequestedByServer = false;
			resetBuffer();
		}

		ServerConnection* getConnection() {
			return conn;
		}

		bool bufferLock();
		void bufferUnLock();

		Websocket():recvBuffer(recvBufferBuff,sizeof(recvBufferBuff)){resetBuffer();_bufferLock = xSemaphoreCreateMutex();unAssign(); }

	};

};
