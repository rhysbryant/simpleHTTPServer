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
#include "ServerConnection.h"
#include <string>
namespace SimpleHTTPTest {

	class MockTransport : public SimpleHTTP::Internal::Transport {
	public:
		std::string* buffer;
		explicit MockTransport(std::string* buf) : buffer(buf) {}
		err_t shutdown() override { return ERR_OK; }
		int write(const void* dataptr, u16_t len, uint8_t) override {
			buffer->append((const char*)dataptr, len);
			return len;
		}
		int getAvailableSendBuffer() override { return 4096; }
		bool getRemoteIPAddress(char*, int) override { return false; }
	};

	//Dummy Connection for Testing
	class MockServerConnection : public SimpleHTTP::ServerConnection {
	public:
		std::string buffer;
	private:
		tcp_pcb mockSocket{};
		MockTransport mockTransport;
	public:
		MockServerConnection() : mockTransport(&buffer) {
			init(&mockSocket, &mockTransport);
		}

		inline bool write(uint8_t* data, uint16_t len) {
			buffer.append((char*)data, len);
			return true;
		}
	};
}