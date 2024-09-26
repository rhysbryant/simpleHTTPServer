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

#if defined(_WIN32) || defined(__linux__)
#include "mock-tcp.h"
#else
#include "lwip/tcp.h"

#endif
#include <stdint.h>
#include <queue>
#if !defined(LWIP_TCPIP_CORE_LOCKING) || LWIP_TCPIP_CORE_LOCKING == 0
#define LOCK_TCPIP_CORE()
#define UNLOCK_TCPIP_CORE()
#else
#include "lwip/sys.h"
#include "lwip/tcpip.h"
#endif
namespace SimpleHTTP {
	//HTTP client connection
	class ServerConnection {
	private:
		enum ClientState {
			ClientConnected,
			Idle
		};

		struct tcp_pcb* client;
		static Result parseRequest(void* arg, uint8_t* data, uint16_t len);

		// allow writeData calls with a size larger then the IP stacks sent buffer
		// by using a queue

		struct ChunkForSend {
			const uint8_t* data;
			const uint16_t size;
		};
		std::queue<ChunkForSend> sendQueue;

		//current chuck size in flight
		int waitingForSendCompleteSize;

		bool sendNextFromQueue();

		static inline int writePlainText(tcp_pcb* pcb, const void* dataptr, u16_t len, uint8_t apiflags) {

			if (tcp_write(pcb,(uint8_t*) dataptr, len, apiflags) != ERR_OK) {
				return ErrWriteDataFailed;
			}
			tcp_output(pcb);
			return len;
		}

	public:
		static const int maxSendSize = 4096;

		uint32_t lastRequestTime;

		Request currentRequest;

		bool connectionUpgraded;

		typedef void (*SessionArgFree) (void* arg);
		void* sessionArg;
		SessionArgFree sessionArgFreeHandler;

		bool hijacted;
		int closeOnceSent;

		typedef Result(*DataReceived) (void* arg, uint8_t* data, uint16_t len);
		DataReceived dataReceived;
		void* dataReceivedArg;
		/**
		 * true if the tcp socket is in a connected state
		 */
		bool isConnected();

		ServerConnection();

		static const int ErrWriteDataFailed = -1;

		typedef int (*WriteData) (tcp_pcb *pcb, const void *dataptr, u16_t len, uint8_t apiflags);
		private:
			WriteData writeDataInternal;
		public:

		void init(struct tcp_pcb* client);
		void init(struct tcp_pcb* client, WriteData callback);
		/**
		 * internal method only
		 * in this context result means
		 * MoreData; busy more data to send
		 * ERROR; failed to send data
		 * OK; nothing more to send
		**/
		Result sendCompleteCallback(int length);

		/*
		 * writes data to the client connection
		 *
		 */
		virtual inline bool write_old(uint8_t* data, uint16_t len) {
			LOCK_TCPIP_CORE();
			if( client == 0 ){
				UNLOCK_TCPIP_CORE();
				return false;
			}
			auto result = tcp_write(client, data, len, TCP_WRITE_FLAG_COPY) == ERR_OK;
			if (result) {
				waitingForSendCompleteSize += len;
			}
			UNLOCK_TCPIP_CORE();
			return result;
		}

		static const int WriteFlagNoLock = 1;
		//don't copy the data
		static const int WriteFlagZeroCopy = 2;
		static const int WriteFlagNoFlush = 4;

		
		bool writeData(uint8_t* data, int len, int writeFlags);

		/*
		 * writes data to the client connection and flushes it
		 *TCP_WRITE_FLAG_COPY
		 */
		inline bool writeDataAndFlush_old(uint8_t* data, uint16_t len) {
			LOCK_TCPIP_CORE();
			if( client == 0 ){
				UNLOCK_TCPIP_CORE();
				return false;
			}
			auto result = (tcp_write(client, data, len, 0) == ERR_OK
				&& tcp_output(client) == ERR_OK);
			if (result) {
				waitingForSendCompleteSize += len;
			}
			UNLOCK_TCPIP_CORE();
			return result;
		}

		inline bool flushData_old() {
			LOCK_TCPIP_CORE();
			if( client == 0 ){
				UNLOCK_TCPIP_CORE();
				return false;
			}
			err_t err = tcp_output(client);
			UNLOCK_TCPIP_CORE();
			if (err != ERR_OK) {
				return false;
			}
			else {
				return true;
			}
		}

		inline bool  hasAvailableSendBuffer() {
			return waitingForSendCompleteSize <= maxSendSize;
		}

		inline bool closeWithOutLocking() {
			tcp_close(client);
			client = 0;
			if (sessionArg != 0 && sessionArgFreeHandler != 0) {
				sessionArgFreeHandler(sessionArg);
				sessionArg = 0;
			}
			dataReceived(dataReceivedArg,0,0);
			return false;
		}

		inline bool close() {
			LOCK_TCPIP_CORE();
			closeWithOutLocking();
			UNLOCK_TCPIP_CORE();
			return false;
		}

		inline void abort() {
			tcp_abort(client);
		}

	};

}
