#ifndef __SYLAR_HTTP_SERVERLET_H__
#define __SYLAR_HTTP_SERVERLET_H__

#include <memory>
#include <functional>
#include <string>
#include <vector>
#include <unordered_map>
#include "http_session.h"
#include "../thread.h"

namespace sylar
{
namespace http
{

class Servlet
{
public:
	typedef std::shared_ptr<Servlet> ptr;

	Servlet(const std::string &name)
		: m_name(name) {}
		
	virtual ~Servlet(){}
	virtual int32_t handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) = 0;

	const std::string &getName() const { return m_name; }
protected:
	std::string m_name;
};


class FunctionServlet : public Servlet
{
public:
	typedef std::shared_ptr<FunctionServlet> ptr;
	typedef std::function<int32_t (HttpRequest::ptr request, 
									HttpResponse::ptr response, 
									HttpSession::ptr session)> callback;
	FunctionServlet(callback cb);

	int32_t handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) override;
	
private:
	callback m_cb;
};

class ServletDispath : public Servlet
{
public:
	typedef std::shared_ptr<ServletDispath> ptr;
	typedef RWMutex RWMutexType;

	ServletDispath();

	int32_t handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) override;

	void addServlet(const std::string &uri, Servlet::ptr slt);
	void addServlet(const std::string &uri, FunctionServlet::callback cb);
	
	void addGlobServlet(const std::string &uri, Servlet::ptr slt);
	void addGlobServlet(const std::string &uri, FunctionServlet::callback cb);

	void delServlet(const std::string &uri);
	void delGlobServlet(const std::string &uri);

	Servlet::ptr getDefault() const { return m_default; }
	void serDefault(Servlet::ptr v) { m_default = v; }
	
	Servlet::ptr getServlet(const std::string &uri);
	Servlet::ptr getGlobServlet(const std::string &uri);

	Servlet::ptr getMatchedServlet(const std::string &uri);
private:
	RWMutexType m_mutex;
	//uri(/sylar/xxx)->Servlet 精准匹配
	std::unordered_map<std::string, Servlet::ptr> m_datas;
	//uri(/sylar/*)->Servlet 模糊匹配
	std::vector<std::pair<std::string, Servlet::ptr>> m_globs;
	//默认Servlet, 所有路径没有匹配时使用
	Servlet::ptr m_default;
};

class NotFoundServlet : public Servlet
{
public:
	typedef std::shared_ptr<NotFoundServlet> ptr;
	
	NotFoundServlet();
	int32_t handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) override;
};

}
}

#endif
