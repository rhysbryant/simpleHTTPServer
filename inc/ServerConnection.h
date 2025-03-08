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
#include "BasicLWIPTransport.h"

#if defined(_WIN32) || defined(__linux__)
#include "mock-tcp.h"
#else
#include "lwip/tcp.h"

#endif
#include <stdint.h>
#include "queue.h"
#if !defined(LWIP_TCPIP_CORE_LOCKING) || LWIP_TCPIP_CORE_LOCKING == 0
#define LOCK_TCPIP_CORE()
#define UNLOCK_TCPIP_CORE()
#else
#include "lwip/sys.h"
#include "lwip/tcpip.h"
#endif
using SimpleHTTP::Internal::Transport;
namespace SimpleHTTP {
	//HTTP client connection
	class ServerConnection {
	private:
		enum ClientState {
			ClientConnected,
			Idle
		};

		//struct tcp_pcb* client;
		static Result parseRequest(void* arg, uint8_t* data, uint16_t len);

		// allow writeData calls with a size larger then the IP stacks sent buffer
		// by using a queue

		struct ChunkForSend {
			const uint8_t* data;
			const uint16_t size;
		};
		LinkedListQueue<ChunkForSend> sendQueue;

		//current chuck size in flight
		int waitingForSendCompleteSize;

		bool sendNextFromQueue();

		SimpleHTTP::Internal::BasicLWIPTransport defaultTransport;
		Transport* transport;

	public:
		static const int maxSendSize = 4096;

		uint32_t lastRequestTime;

		Request currentRequest;

		bool connectionUpgraded;

		typedef void (*SessionArgFree) (void* arg);
		void* sessionArg;
		SessionArgFree sessionArgFreeHandler;

		void runFreeSessionHandler() {
			if (sessionArgFreeHandler != 0) {
				sessionArgFreeHandler(sessionArg);
				sessionArg = 0;
			}
		}

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

		void init(struct tcp_pcb* client);
		void init(struct tcp_pcb* client, Transport* transport);
		/**
		 * internal method only
		 * in this context result means
		 * MoreData; busy more data to send
		 * ERROR; failed to send data
		 * OK; nothing more to send
		**/
		Result sendCompleteCallback(int length);

		/*
		for backwards compatibility alias the flags here
		*/

		static const int WriteFlagNoLock = Transport::WriteFlagNoLock;
		//don't copy the data
		static const int WriteFlagZeroCopy = Transport::WriteFlagZeroCopy;

		static const int WriteFlagNoFlush = Transport::WriteFlagNoFlush;

		bool writeData(uint8_t* data, int len, int writeFlags);

		inline bool  hasAvailableSendBuffer() {
			return transport && transport->getAvailableSendBuffer() > 0; // waitingForSendCompleteSize <= maxSendSize;
		}

		inline bool closeWithOutLocking() {
			if(transport){
				transport->shutdown();
				transport = 0;
			}
			runFreeSessionHandler();
			dataReceived(dataReceivedArg, 0, 0);
			return false;
		}

		inline bool close() {
			LOCK_TCPIP_CORE();
			closeWithOutLocking();
			UNLOCK_TCPIP_CORE();
			return false;
		}
		/*
		inline void abort() {
			tcp_abort(client);
		}*/

	};

}
