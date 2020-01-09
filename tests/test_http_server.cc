#include "../sylar/http/http_server.h"
#include "../sylar/log.h"

using namespace sylar;
using namespace sylar::http;
static Logger::ptr g_logger = SYLAR_LOG_ROOT();

void run()
{
	http::HttpServer::ptr server(new http::HttpServer);
	Address::ptr addr = Address::LookupAnyIPAddress("0.0.0.0:8020");
	while(!server->bind(addr))
		{
			sleep(2);
		}
	auto sd = server->getServletDispath();
	sd->addServlet("/sylar/xx", [](HttpRequest::ptr req, HttpResponse::ptr rsp, HttpSession::ptr session)
		{	
			rsp->setBody(req->toString());
			return 0;
		}
		);
	sd->addGlobServlet("/sylar/*", [](HttpRequest::ptr req, HttpResponse::ptr rsp, HttpSession::ptr session)
		{	
			rsp->setBody("Glob:\r\n" + req->toString());
			return 0;
		}
		);
	server->start();
}

int main(int argc, char *argv[])
{
	IOManager iom(2);
	iom.schedule(run);
	
	return 0;
}
