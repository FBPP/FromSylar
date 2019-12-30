#include "../sylar/hook.h"
#include "../sylar/iomanager.h"
#include "../sylar/log.h"

#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void test_sleep()
{
	sylar::IOManager::GetThis()->schedule([](){
			sleep(2);
			SYLAR_LOG_INFO(g_logger) << "sleep 2";
		});

	sylar::IOManager::GetThis()->schedule([](){
			sleep(3);
			SYLAR_LOG_INFO(g_logger) << "sleep 3";
		});
	SYLAR_LOG_INFO(g_logger) << "test_sleep";
	
}

void test_hook()
{
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	sockaddr_in addr;
	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(80);
	inet_pton(AF_INET, "61.135.169.121", &addr.sin_addr.s_addr);

	SYLAR_LOG_INFO(g_logger) << "begin connect";
	int rt = connect(sock, (sockaddr *)&addr, sizeof(addr));
	SYLAR_LOG_INFO(g_logger) << "connect rt=" << rt << " errno=" << errno;
	SYLAR_LOG_INFO(g_logger) << EINPROGRESS;


	
	const char data[] = "GET / HTTP/1.0\r\n\r\n";
	rt = send(sock, data, sizeof(data), 0);
	SYLAR_LOG_INFO(g_logger) << "send rt=" << rt << " errno=" << errno;
	if(rt <= 0)	
		{
			SYLAR_LOG_INFO(g_logger) << rt;
			return;
		}
	std::string buff;
	buff.resize(8192);

	rt = recv(sock, &buff[0], buff.size(), 0);
	SYLAR_LOG_INFO(g_logger) << "recv rt=" << rt << " errno=" << errno;

	if(rt <= 0)	
		{
			SYLAR_LOG_INFO(g_logger) << rt;
			return;
		}
	//buff.resize(rt);
	SYLAR_LOG_INFO(g_logger) << buff;
}

int main(int argc, char *argv[])
{
	g_logger->addAppender(sylar::FileLogAppender::ptr( new sylar::FileLogAppender("log.txt")));
	sylar::IOManager iom(1);
	//iom.schedule(&test_sleep);
	iom.schedule(&test_hook);
	return 0;
}
