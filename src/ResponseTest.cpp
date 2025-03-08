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

#include "gtest/gtest.h"
#include "Response.h"
#include "MockServerConnection.h"
using SimpleHTTP::Response;
using SimpleHTTPTest::MockServerConnection;

TEST(Response, DefaultResponse) {
	MockServerConnection conn;
	Response r(&conn, true, SimpleHTTP::HTTP11);
	r.finalize();
	ASSERT_EQ(conn.buffer, "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nKeep-Alive: timeout=15, max=1000\r\n\r\n");
}

TEST(Response, SingleWrite) {
	MockServerConnection conn;
	Response r(&conn, true,  SimpleHTTP::HTTP11);
	string str = "Hello World";
	r.write(str.c_str(), str.length());
	r.finalize();
	string expected = "HTTP/1.1 200 OK\r\nContent-Length: 11\r\nKeep-Alive: timeout=15, max=1000\r\n\r\nHello World";
	ASSERT_EQ(conn.buffer, expected);
}

TEST(Response, SingleChunk) {
	MockServerConnection conn;
	Response r(&conn, true, SimpleHTTP::HTTP11);
	string str = "Hello World";
	r.write(str.c_str(), str.length());
	r.flush();
	string sizeChunk = "B\r\n";
	string expectedResponse = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nKeep-Alive: timeout=15, max=1000\r\n\r\n" + sizeChunk + str+"\r\n";
	
	ASSERT_EQ(conn.buffer, expectedResponse);
	
	conn.buffer.clear();

	sizeChunk = "0\r\n\r\n";
	r.finalize();
	ASSERT_EQ(conn.buffer, sizeChunk);
	
}

TEST(Response, ManyChunks) {
	MockServerConnection conn;
	
	Response r(&conn, true,SimpleHTTP::HTTP11);

	string expectedResponse = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nKeep-Alive: timeout=15, max=1000\r\n\r\n17E\r\nHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello Wo\r\nA8\r\nrldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello World\r\n0\r\n\r\n";
	string msg = "Hello World";

	for (int i = 0; i < 50; i++) {
		r.write(msg.c_str(), msg.length());
	}
	r.finalize();

	ASSERT_EQ(conn.buffer, expectedResponse);


}