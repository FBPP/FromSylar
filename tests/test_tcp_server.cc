#include "../sylar/tcp_server.h"
#include "../sylar/iomanager.h"
#include "../sylar/log.h"

using namespace sylar;
using namespace std;

Logger::ptr g_logger = SYLAR_LOG_ROOT();

void run()
{                    
	auto addr = IPAddress::LookupAny("0.0.0.0:8020");
	auto addr2 = UnixAddress::ptr(new UnixAddress("/tmp/unix_addr"));
	SYLAR_LOG_INFO(g_logger) << *addr << " - " << *addr2;
	vector<Address::ptr> addrs;
	addrs.push_back(addr2);
	addrs.push_back(addr);

	vector<Address::ptr> fails;
	TcpServer::ptr tcp_server(new TcpServer);
	while(!tcp_server->bind(addrs, fails))
		{
			sleep(2);
		}
	tcp_server->start();
}

int main(int argc, char *argv[])
{
	IOManager iom(2);
	iom.schedule(run);

	return 0;
}
