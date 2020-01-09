#ifndef _SYLAR_HTTP_SERVER_H__
#define _SYLAR_HTTP_SERVER_H__

#include "../tcp_server.h"
#include "http_session.h"
#include "http_serverlet.h"

namespace sylar
{
namespace http
{

class HttpServer : public TcpServer
{
public:
	typedef std::shared_ptr<HttpServer> ptr;

	HttpServer(	bool keepalive = false, 
				IOManager *worker = IOManager::GetThis(), 
				IOManager *accept_worker = IOManager::GetThis());

	ServletDispath::ptr getServletDispath() const { return m_dispatch; }	
	void setServletDispath(ServletDispath::ptr v) { m_dispatch = v; }
protected:
	void handleClient(Socket::ptr client) override;

private:	
	bool m_isKeepalive; //是否支持长连接
	ServletDispath::ptr m_dispatch;
};

}
}


#endif