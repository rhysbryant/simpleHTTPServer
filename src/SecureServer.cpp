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
#include "SecureServer.h"
#include "Router.h"
#include <queue>
#include "log.h"
 /*
 * data sending flow
 *
 * the payload for sent is divided into chunks of upto maxSendSize in size
 *
 * based round 3 queues plan text data queue (in ClientConnection class) -> tls write data queue -> chunk waiting Ack queue

 * tcp_write_tls callback from http lib - with plan text data for sending
 * mbedtls_tcp_send callback from mbedtls - with tls data for sending
 * tcp_sent_cb callback from network stack - data has been acked
 *
 * (connection).sendCompleteCallback will send the next plan text chunk if any i.e will call tcp_write_tls
 * sendNextChunk will send the next chunk of tls data
 *
 * if the payload is over maxSendSize mbedtls_ssl_write will go into a "blocking" like state i.e will return a busy status
 * until the blocking data has been acked. otherwise (under maxSendSize) a copy is made of the payload and mbedtls_ssl_write can be called again
 * before the previously sent data has been acked
 *
 * the two main use cases we need to account for are are lots of small chunks being sent in parallel and large of maxSendSize being sent one at a time
 *
 * data receive flow
 *
 * the pbuf from the lwip receive callback is queued and freed in the mbedtls mbedtls_tcp_recv callback
 * called from within mbedtls_ssl_read() and mbedtls_ssl_handshake()
 */
using namespace SimpleHTTP;

err_t SecureServer::tcp_accept_cb(void* arg, struct tcp_pcb* newpcb, err_t err)
{

	auto conn = Router::getFreeConnection();
	if (conn == 0) {
		tcp_abort(newpcb);
		return ERR_ABRT;
	}
	SHTTP_LOGI(__FUNCTION__, "before setup; heap free %d min seen %d", (int)esp_get_free_heap_size(), (int)esp_get_minimum_free_heap_size());
	auto s = new SecureServerConnection(newpcb,conn);

	tcp_arg(newpcb, s);
	tcp_err(newpcb, tcp_err_cb);
	tcp_sent(newpcb, tcp_sent_cb);
	tcp_recv(newpcb, tcp_recv_cb);

	auto result = s->initSSLContext(&conf,nullptr, nullptr);
	if ( result != 0) {
		SHTTP_LOGE(__FUNCTION__, "mbedtls_ssl_setup failed %d", result);
		tcp_arg(newpcb, nullptr);
		delete s;
		//tcp_abort(newpcb);
		return ERR_ABRT;
	}

	SHTTP_LOGI(__FUNCTION__, "connection accepted;heap free %d min seen %d", (int)esp_get_free_heap_size(), (int)esp_get_minimum_free_heap_size());
	s->sslSessionProcess(nullptr);

	return ERR_OK;

}

err_t SecureServer::tcp_recv_cb(void* arg, struct tcp_pcb* tpcb, struct pbuf* p,
	err_t err)
{
	if (arg != 0)
	{
		auto conn = static_cast<SecureServerConnection*>(arg);
		if (p == 0)
		{
			SHTTP_LOGI(__FUNCTION__, "closing");
			delete conn;
			SHTTP_LOGI(__FUNCTION__, "heap free %d min seen %d", (int)esp_get_free_heap_size(), (int)esp_get_minimum_free_heap_size());
			tcp_arg(tpcb, nullptr);

			return ERR_OK;
		}
		
		conn->sslSessionProcess(p);
	}

	return ERR_OK;
}

err_t SecureServer::tcp_sent_cb(void* arg, struct tcp_pcb* tpcb, u16_t len)
{
	if (arg != 0)
	{
		auto conn = static_cast<SecureServerConnection*>(arg);
		conn->sendCompleteCallback(len);
	}

	return ERR_OK;
}

void SecureServer::tcp_err_cb(void* arg, err_t err)
{
	if (arg != 0)
	{
		auto conn = static_cast<SecureServerConnection*>(arg);
		delete conn;
		SHTTP_LOGI(__FUNCTION__, "err %d", (int)err);
	}
}

int SecureServer::loadPrivateKey(SimpleString* pk) {
	mbedtls_pk_init(&pkey);
	return mbedtls_pk_parse_key(&pkey, (const unsigned char*)pk->value,
		pk->size, NULL, 0,
		mbedtls_ctr_drbg_random, &ctr_drbg);
}

int SecureServer::loadCert(SimpleString* cert) {
	if (!crtInitDone) {
		mbedtls_x509_crt_init(&srvcert);
		crtInitDone = true;
	}

	return mbedtls_x509_crt_parse(&srvcert, (unsigned const char*)cert->value, cert->size);
}

mbedtls_x509_crt* SecureServer::getCertChain() {
	if (!crtInitDone) {
		return 0;
	}
	return &srvcert;
}

int SecureServer::TLSInit() {
	//mbedtls_net_init(&listen_fd);
	//mbedtls_net_init(&client_fd);
	mbedtls_ssl_config_init(&conf);
#if defined(MBEDTLS_SSL_CACHE_C)
	mbedtls_ssl_cache_init(&cache);
#endif

	mbedtls_entropy_init(&entropy);
	mbedtls_ctr_drbg_init(&ctr_drbg);

	const char* pers = __FUNCTION__;

	auto result = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
		(const unsigned char*)pers,
		strlen(pers));
	if (result != 0) {
		return result;
	}

	result = mbedtls_ssl_config_defaults(&conf,
		MBEDTLS_SSL_IS_SERVER,
		MBEDTLS_SSL_TRANSPORT_STREAM,
		MBEDTLS_SSL_PRESET_DEFAULT);
	if (result != 0) {
		return result;
	}

	mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);

#if defined(MBEDTLS_SSL_CACHE_C)
	mbedtls_ssl_conf_session_cache(&conf, &cache,
		mbedtls_ssl_cache_get,
		mbedtls_ssl_cache_set);
#endif

	mbedtls_ssl_conf_ca_chain(&conf, srvcert.next, NULL);

	result = mbedtls_ssl_conf_own_cert(&conf, &srvcert, &pkey);
	if (result != 0) {
		return result;
	}

	return 0;
}

void SecureServer::listen(int port)
{

	struct tcp_pcb* tmp = tcp_new();
	tcp_bind(tmp, IP4_ADDR_ANY, port);
	tcpServer = tcp_listen(tmp);
	tcp_accept(tcpServer, tcp_accept_cb);
	return;
}

struct tcp_pcb* SecureServer::tcpServer = 0;
mbedtls_entropy_context SecureServer::entropy;
mbedtls_ctr_drbg_context SecureServer::ctr_drbg;
mbedtls_ssl_config SecureServer::conf;
mbedtls_x509_crt SecureServer::srvcert;
mbedtls_pk_context SecureServer::pkey;
mbedtls_ssl_cache_context SecureServer::cache;
bool SecureServer::crtInitDone = false;
