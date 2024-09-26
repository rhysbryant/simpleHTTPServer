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
#include "RequestTest.h"

using SimpleHTTPTest::RequestTest;
using SimpleHTTP::SimpleString;
#include "gtest/gtest.h"
#include <string.h>
using namespace SimpleHTTP;

bool RequestTest::testParseMethod(const char* strMethod, SimpleHTTP::Request::Method m) {

	return m == parseMethod(SimpleString{ strMethod,(int)strlen(strMethod) });
}

TEST(Request, methodParseGET) {
	RequestTest t;
	ASSERT_TRUE(t.testParseMethod("GET", RequestTest::GET));
}

TEST(Request, methodParsePOST) {
	RequestTest t;
	ASSERT_TRUE(t.testParseMethod("POST", RequestTest::POST));
}

TEST(Request, methodParsePUT) {
	RequestTest t;
	ASSERT_TRUE(t.testParseMethod("PUT", RequestTest::PUT));
}

TEST(Request, methodParseDELETE) {
	RequestTest t;
	ASSERT_TRUE(t.testParseMethod("DELETE", RequestTest::DELETE));
}

TEST(Request, methodParseOPTIONS) {
	RequestTest t;
	ASSERT_TRUE(t.testParseMethod("OPTIONS", RequestTest::OPTIONS));
}

TEST(Request, methodParseHEAD) {
	RequestTest t;
	ASSERT_TRUE(t.testParseMethod("HEAD", RequestTest::HEAD));
}

TEST(Request, methodParseSEND) {
	RequestTest t;
	ASSERT_TRUE(t.testParseMethod("SEND", RequestTest::SEND));
}

TEST(Request, fullRequestGET) {
	Request r;
	string req("GET /abc HTTP/1.1\r\nHost: hello\r\n\r\n");
	auto result = r.parse((char*)req.c_str(),req.length());
	GTEST_ASSERT_EQ(result, Result::OK);
	GTEST_ASSERT_EQ(r.method, Request::GET);
	GTEST_ASSERT_EQ(r.headers["HOST"], "hello");
	GTEST_ASSERT_EQ(r.path, "/abc");
}

TEST(Request, requestGET) {
	Request r;
	string req("GET /abc HTTP/1.1\r\nHost: hello\r\n\r\n");
	const char* strRequest = req.c_str();

	while (r.parse((char*)strRequest, 1) == MoreData) strRequest++;
	//GTEST_ASSERT_EQ(result, Result::OK);
	GTEST_ASSERT_EQ(r.method, Request::GET);
	GTEST_ASSERT_EQ(r.headers["HOST"], "hello");
	GTEST_ASSERT_EQ(r.path, "/abc");
}

//one parse call consumes the full payload
TEST(Request, fullRequestPOSTFullBodyContentLength) {
	Request r;
	string req("POST /abc HTTP/1.1\r\nHost: hello\r\nContent-Length: 4\r\n\r\nTest");
	auto result = r.parse((char*)req.c_str(), req.length());
	GTEST_ASSERT_EQ(result, Result::MoreData);

	GTEST_ASSERT_EQ(r.method, Request::POST);
	GTEST_ASSERT_EQ(r.path, "/abc");
	GTEST_ASSERT_EQ(r.getBodyLength(),4);

	char buffer[20] = "";
	int size = sizeof(buffer);
	auto bodyReadResult = r.readBody(buffer, &size);
	GTEST_ASSERT_EQ(bodyReadResult, Result::OK);

	string str(buffer, size);
	char expectedText[] = "Test";
	GTEST_ASSERT_EQ(str, expectedText);
}
//one parse call consumes the full payload
TEST(Request, fullRequestPOSTFullBodyChunked) {
	Request r;
	string req("POST /abc HTTP/1.1\r\nHost: hello\r\nTransfer-Encoding: chunked\r\n\r\n4\r\nTest\r\n0\r\n\r\n");
	auto result = r.parse((char*)req.c_str(), req.length());
	GTEST_ASSERT_EQ(result, Result::MoreData);

	GTEST_ASSERT_EQ(r.method, Request::POST);
	GTEST_ASSERT_EQ(r.path, "/abc");

	char buffer[20] = "";
	int size = sizeof(buffer);
	auto bodyReadResult = r.readBody(buffer, &size);
	GTEST_ASSERT_EQ(bodyReadResult, Result::OK);
	string str(buffer, size);
	char expectedText[] = "Test";
	GTEST_ASSERT_EQ(str, expectedText);
}