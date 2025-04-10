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

TEST(Request, fullRequestRTSPDescribe) {
	char request[] = {
	0x44, 0x45, 0x53, 0x43, 0x52, 0x49, 0x42, 0x45,
	0x20, 0x72, 0x74, 0x73, 0x70, 0x3a, 0x2f, 0x2f,
	0x31, 0x37, 0x32, 0x2e, 0x32, 0x31, 0x2e, 0x30,
	0x2e, 0x31, 0x32, 0x30, 0x3a, 0x38, 0x30, 0x20,
	0x52, 0x54, 0x53, 0x50, 0x2f, 0x31, 0x2e, 0x30,
	0x0d, 0x0a, 0x55, 0x73, 0x65, 0x72, 0x2d, 0x41,
	0x67, 0x65, 0x6e, 0x74, 0x3a, 0x20, 0x57, 0x4d,
	0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x2f, 0x31,
	0x32, 0x2e, 0x30, 0x30, 0x2e, 0x31, 0x39, 0x30,
	0x34, 0x31, 0x2e, 0x35, 0x31, 0x32, 0x39, 0x20,
	0x67, 0x75, 0x69, 0x64, 0x2f, 0x33, 0x33, 0x30,
	0x30, 0x41, 0x44, 0x35, 0x30, 0x2d, 0x32, 0x43,
	0x33, 0x39, 0x2d, 0x34, 0x36, 0x43, 0x30, 0x2d,
	0x41, 0x45, 0x30, 0x41, 0x2d, 0x44, 0x44, 0x42,
	0x33, 0x42, 0x30, 0x44, 0x41, 0x45, 0x32, 0x38,
	0x34, 0x0d, 0x0a, 0x41, 0x63, 0x63, 0x65, 0x70,
	0x74, 0x3a, 0x20, 0x61, 0x70, 0x70, 0x6c, 0x69,
	0x63, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x2f, 0x73,
	0x64, 0x70, 0x0d, 0x0a, 0x41, 0x63, 0x63, 0x65,
	0x70, 0x74, 0x2d, 0x43, 0x68, 0x61, 0x72, 0x73,
	0x65, 0x74, 0x3a, 0x20, 0x55, 0x54, 0x46, 0x2d,
	0x38, 0x2c, 0x20, 0x2a, 0x3b, 0x71, 0x3d, 0x30,
	0x2e, 0x31, 0x0d, 0x0a, 0x58, 0x2d, 0x41, 0x63,
	0x63, 0x65, 0x70, 0x74, 0x2d, 0x41, 0x75, 0x74,
	0x68, 0x65, 0x6e, 0x74, 0x69, 0x63, 0x61, 0x74,
	0x69, 0x6f, 0x6e, 0x3a, 0x20, 0x4e, 0x65, 0x67,
	0x6f, 0x74, 0x69, 0x61, 0x74, 0x65, 0x2c, 0x20,
	0x4e, 0x54, 0x4c, 0x4d, 0x2c, 0x20, 0x44, 0x69,
	0x67, 0x65, 0x73, 0x74, 0x2c, 0x20, 0x42, 0x61,
	0x73, 0x69, 0x63, 0x0d, 0x0a, 0x41, 0x63, 0x63,
	0x65, 0x70, 0x74, 0x2d, 0x4c, 0x61, 0x6e, 0x67,
	0x75, 0x61, 0x67, 0x65, 0x3a, 0x20, 0x65, 0x6e,
	0x2d, 0x75, 0x73, 0x2c, 0x20, 0x2a, 0x3b, 0x71,
	0x3d, 0x30, 0x2e, 0x31, 0x0d, 0x0a, 0x58, 0x2d,
	0x41, 0x63, 0x63, 0x65, 0x70, 0x74, 0x2d, 0x50,
	0x72, 0x6f, 0x78, 0x79, 0x2d, 0x41, 0x75, 0x74,
	0x68, 0x65, 0x6e, 0x74, 0x69, 0x63, 0x61, 0x74,
	0x69, 0x6f, 0x6e, 0x3a, 0x20, 0x4e, 0x65, 0x67,
	0x6f, 0x74, 0x69, 0x61, 0x74, 0x65, 0x2c, 0x20,
	0x4e, 0x54, 0x4c, 0x4d, 0x2c, 0x20, 0x44, 0x69,
	0x67, 0x65, 0x73, 0x74, 0x2c, 0x20, 0x42, 0x61,
	0x73, 0x69, 0x63, 0x0d, 0x0a, 0x43, 0x53, 0x65,
	0x71, 0x3a, 0x20, 0x31, 0x0d, 0x0a, 0x53, 0x75,
	0x70, 0x70, 0x6f, 0x72, 0x74, 0x65, 0x64, 0x3a,
	0x20, 0x63, 0x6f, 0x6d, 0x2e, 0x6d, 0x69, 0x63,
	0x72, 0x6f, 0x73, 0x6f, 0x66, 0x74, 0x2e, 0x77,
	0x6d, 0x2e, 0x73, 0x72, 0x76, 0x70, 0x70, 0x61,
	0x69, 0x72, 0x2c, 0x20, 0x63, 0x6f, 0x6d, 0x2e,
	0x6d, 0x69, 0x63, 0x72, 0x6f, 0x73, 0x6f, 0x66,
	0x74, 0x2e, 0x77, 0x6d, 0x2e, 0x73, 0x73, 0x77,
	0x69, 0x74, 0x63, 0x68, 0x2c, 0x20, 0x63, 0x6f,
	0x6d, 0x2e, 0x6d, 0x69, 0x63, 0x72, 0x6f, 0x73,
	0x6f, 0x66, 0x74, 0x2e, 0x77, 0x6d, 0x2e, 0x65,
	0x6f, 0x73, 0x6d, 0x73, 0x67, 0x2c, 0x20, 0x63,
	0x6f, 0x6d, 0x2e, 0x6d, 0x69, 0x63, 0x72, 0x6f,
	0x73, 0x6f, 0x66, 0x74, 0x2e, 0x77, 0x6d, 0x2e,
	0x66, 0x61, 0x73, 0x74, 0x63, 0x61, 0x63, 0x68,
	0x65, 0x2c, 0x20, 0x63, 0x6f, 0x6d, 0x2e, 0x6d,
	0x69, 0x63, 0x72, 0x6f, 0x73, 0x6f, 0x66, 0x74,
	0x2e, 0x77, 0x6d, 0x2e, 0x6c, 0x6f, 0x63, 0x69,
	0x64, 0x2c, 0x20, 0x63, 0x6f, 0x6d, 0x2e, 0x6d,
	0x69, 0x63, 0x72, 0x6f, 0x73, 0x6f, 0x66, 0x74,
	0x2e, 0x77, 0x6d, 0x2e, 0x72, 0x74, 0x70, 0x2e,
	0x61, 0x73, 0x66, 0x2c, 0x20, 0x64, 0x6c, 0x6e,
	0x61, 0x2e, 0x61, 0x6e, 0x6e, 0x6f, 0x75, 0x6e,
	0x63, 0x65, 0x2c, 0x20, 0x64, 0x6c, 0x6e, 0x61,
	0x2e, 0x72, 0x74, 0x78, 0x2c, 0x20, 0x64, 0x6c,
	0x6e, 0x61, 0x2e, 0x72, 0x74, 0x78, 0x2d, 0x64,
	0x75, 0x70, 0x2c, 0x20, 0x63, 0x6f, 0x6d, 0x2e,
	0x6d, 0x69, 0x63, 0x72, 0x6f, 0x73, 0x6f, 0x66,
	0x74, 0x2e, 0x77, 0x6d, 0x2e, 0x73, 0x74, 0x61,
	0x72, 0x74, 0x75, 0x70, 0x70, 0x72, 0x6f, 0x66,
	0x69, 0x6c, 0x65, 0x0d, 0x0a, 0x0d, 0x0a };

	Request r;
	auto result = r.parse(request, sizeof(request));
	GTEST_ASSERT_EQ(result, Result::OK);

	GTEST_ASSERT_EQ(r.method, Request::RTSP_DESCRIBE);
	GTEST_ASSERT_EQ(r.getBodyLength(), 0);
	GTEST_ASSERT_EQ(r.getAndClearForProcessing(), true);


}