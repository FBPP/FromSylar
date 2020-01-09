#include <iostream>
#include "../sylar/http/http_connection.h"
#include "../sylar/log.h"
#include "../sylar/iomanager.h"

static sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void run()
{
	sylar::Address::ptr addr = sylar::Address::LookupAnyIPAddress("www.sylar.top:80");
	if(!addr)
		{
			SYLAR_LOG_ERROR(g_logger) << "get addr error";
			return;
		}
	
	sylar::Socket::ptr sock = sylar::Socket::CreateTCP(addr);
	bool rt = sock->connect(addr);
	if(!rt)
		{
			SYLAR_LOG_ERROR(g_logger) << "connect error";
			return;
		}
	sylar::http::HttpConnection::ptr conn(new sylar::http::HttpConnection(sock));

	sylar::http::HttpRequest::ptr req(new sylar::http::HttpRequest);

	req->setPath("/blog/");
	req->setHeader("host", "www.sylar.top");
	SYLAR_LOG_INFO(g_logger) << "req:" << std::endl
        << *req;
	conn->sendRequest(req);
	auto rsp = conn->recvResponse();
	if(!rsp)
		{
			SYLAR_LOG_ERROR(g_logger) << "rsp error";
			return;
		}
	SYLAR_LOG_INFO(g_logger) << *rsp;

	
	SYLAR_LOG_INFO(g_logger) << "======================";
	
}

void hc()
{
	auto rst = sylar::http::HttpConnection::DoGet("http://www.cplusplus.com/reference/algorithm/", 300);
	auto rsp2 = rst->response;
	SYLAR_LOG_INFO(g_logger) << "result=" << rst->result 
					 << " error=" << rst->error 
					 << " rsp=" << (rsp2 ? rsp2->toString() : "");
}

void test_pool()
{
	sylar::http::HttpConnectionPool::ptr pool(new sylar::http::HttpConnectionPool("www.sylar.top",
											"", 80, 10, 1000 * 30, 5));
	sylar::IOManager::GetThis()->addTimer(1000, [pool](){
			auto r = pool->doGet("/", 300);
			SYLAR_LOG_INFO(g_logger) << r->toString();
		}, true);
		
}
int main(int argc, char *argv[])
{
	sylar::FileLogAppender::ptr file_appender(new sylar::FileLogAppender("log.txt"));
	g_logger->addAppender(file_appender);
	
	sylar::IOManager iom(2);
	iom.schedule(test_pool);
	return 0;
}
