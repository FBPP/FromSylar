#include "../sylar/tcp_server.h"
#include "../sylar/log.h"
#include "../sylar/iomanager.h"
#include "../sylar/bytearray.h"

using namespace sylar;
using namespace std;

static Logger::ptr g_logger = SYLAR_LOG_NAME("system");

class EchoServer : public TcpServer
{
public:
	EchoServer(int type); 
	void handleClient(Socket::ptr client);

private:
	// type = 1 文本协议 或 type = 2 二进制协议
	int m_type = 0;
};

EchoServer::EchoServer(int type)
	: m_type(type)
{
	
}

void EchoServer::handleClient(Socket::ptr client)
{
	SYLAR_LOG_INFO(g_logger) << "handleClient" << *client;
	ByteArray::ptr ba(new ByteArray);
	while(true)
		{
			ba->clear();
			vector<iovec> iovs;
			ba->getWriteBuffers(iovs, 1024);

			int rt = client->recv(&iovs[0], iovs.size());
			if(rt == 0)
				{
					SYLAR_LOG_INFO(g_logger) << "client close: " << *client;
					break;
				}
			else if(rt < 0)
				{
					SYLAR_LOG_ERROR(g_logger) << "client error rt=" << rt
							<< " error=" << errno << " errstr=" << strerror(errno);
					break;
				}

			ba->setPosition(ba->getPosition() + rt);
			ba->setPosition(0);
			//SYLAR_LOG_INFO(g_logger) << "recv rt=" << rt << " data=" << std::string((char *)iovs[0].iov_base, rt);
			if(m_type == 1) //text
				{
					cout << ba->toString();
				}
			else // 二进制
				{
					cout << ba->toHexString();
				}
			std::cout.flush();
		}
	
}

int type = 1;

void run()
{
	EchoServer::ptr es(new EchoServer(type));
	auto addr = Address::LookupAny("172.20.10.11:8020");
	while(!es->bind(addr))
		{
			sleep(2);
		}
	es->start();
}

int main(int argc, char *argv[])
{
	if(argc < 2)
		{
			SYLAR_LOG_INFO(g_logger) << "used as[" << argv[0] << " -t] or [" <<  argv[0] << " -b]";
			return 0;
		}
	if(!strcmp(argv[1], "-b"))
		{
			type = 2;
		}

	IOManager iom(2);
	iom.schedule(run);
	
	return 0;
}
