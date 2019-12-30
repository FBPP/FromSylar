#include "hook.h"
#include "fiber.h"
#include "iomanager.h"
#include "fd_manager.h"
#include "log.h"
#include "config.h"
#include "macro.h"

#include <dlfcn.h>
#include <stdarg.h>


sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

namespace sylar
{

ConfigVar<int>::ptr g_tcp_connect_timeout = Config::Lookup("tcp.connect.timeout", 5000, "tcp connect timeout");


static thread_local bool t_hook_enable = false;

#define HOOK_FUN(XX) \
	XX(sleep) \
	XX(usleep) \
	XX(nanosleep)\
	XX(socket)\
	XX(connect)\
	XX(accept)\
	XX(read)\
	XX(readv)\
	XX(recv)\
	XX(recvfrom)\
	XX(recvmsg)\
	XX(write)\
	XX(writev)\
	XX(send)\
	XX(sendto)\
	XX(sendmsg)\
	XX(close)\
	XX(ioctl)\
	XX(fcntl)\
	XX(getsockopt)\
	XX(setsockopt)
	

void hook_init()
{
	static bool is_inited = false;
	if(is_inited)
		return;
#define XX(name) name ## _f = (name ## _fun)dlsym(RTLD_NEXT, #name);

	HOOK_FUN(XX)
	//sleep_f = (sleep_fun)dlsym(RTLD_NEXT, "sleep");

#undef XX
}

static uint64_t s_connect_timeout = -1;

struct _HookIniter
{
	_HookIniter()
	{
		hook_init();
		
		s_connect_timeout = g_tcp_connect_timeout->getValue();
		g_tcp_connect_timeout->addListener([](const int &old_value, const int &new_value){
				SYLAR_LOG_INFO(g_logger) << "tcp timeout chanaged from"
										<< old_value << "to" << new_value;
				s_connect_timeout = new_value;
			});
	}
};

static _HookIniter s_hook_initer;


bool Is_hook_enable()
{
	return t_hook_enable;	
}

void Set_hook_enable(bool flag)
{
	t_hook_enable = flag;
	
}

struct timer_info
{
	int cancelled = 0;
};

template<typename OriginFun, typename ... Args>
static ssize_t do_io(int fd, OriginFun fun, const char *hook_fun_name, 
				uint32_t event, int timeout_so, Args&&... args)
{
	if(!sylar::Is_hook_enable())
		return fun(fd, std::forward<Args>(args)...);

	sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->get(fd);
	if(!ctx)
		return fun(fd, std::forward<Args>(args)...);
	if(ctx->isClose()){
			errno = EBADF;
			return -1;
		}
	if(!ctx->isSocket() || ctx->getUserNonblock())
		return fun(fd, std::forward<Args>(args)...);
	
	uint64_t to = ctx->getTimeout(timeout_so);

	std::shared_ptr<timer_info> tinfo(new timer_info);
retry:	
	ssize_t n = fun(fd, std::forward<Args>(args)...);

	SYLAR_LOG_DEBUG(g_logger) << "do io"  << hook_fun_name << " n = " << n;
	//中断
	while(n == -1 && errno == EINTR)
		{
			SYLAR_LOG_DEBUG(g_logger) << "errno == EINTR";
			n = fun(fd, std::forward<Args>(args)...);
		}
	//缓冲区满
	if(n == -1 && errno == EAGAIN)
		{
			SYLAR_LOG_DEBUG(g_logger) << "errno == EAGAIN";
			sylar::IOManager *iom = sylar::IOManager::GetThis();
			sylar::Timer::ptr timer;
			std::weak_ptr<timer_info> winfo(tinfo);

			if(to != (uint64_t) -1)
				timer = iom->addConditionTimer(to, [winfo, fd, iom, event](){
					auto t = winfo.lock();
					if(!t || t->cancelled)
						return;
					t->cancelled = ETIMEDOUT;
					iom->cancelEvent(fd, (sylar::IOManager::Event)event);
				}, winfo);

			int rt = iom->addEvent(fd, (sylar::IOManager::Event)event);
			if(SYLAR_UNLIKELY(rt))
				{
					SYLAR_LOG_ERROR(g_logger) << hook_fun_name << " addEvent (" 
						<< fd << ", " << event << ")";
					if(timer)
						timer->cancel();
					return -1;
				}
			else{
					sylar::Fiber::YieldToHold();
					if(timer)
						timer->cancel();
					if(tinfo->cancelled){
							errno = tinfo->cancelled;
							SYLAR_LOG_DEBUG(g_logger) << "errno = ETIMEDOUT";
							return -1;
						}
					
					goto retry;
				}
				
		}
		
	return n;
		
}


extern "C"
{
#define XX(name) name ## _fun name ## _f = nullptr;

	HOOK_FUN(XX)
	// sleep_fun sleep_f = nullptr;
#undef XX


unsigned int sleep(unsigned int seconds)
{
	if(!sylar::t_hook_enable)
		return sleep_f(seconds);
	sylar::Fiber::ptr fiber = sylar::Fiber::GetThis();
	sylar::IOManager *iom = sylar::IOManager::GetThis();
	iom->addTimer(seconds * 1000, 
			std::bind((void(sylar::Scheduler::*)(sylar::Fiber::ptr, int thread))&sylar::IOManager::schedule, iom, fiber, -1)
		);
	sylar::Fiber::YieldToHold();
	return 0;
}

int usleep(unsigned int seconds)
{
	if(!sylar::t_hook_enable)
		return usleep_f(seconds);
	sylar::Fiber::ptr fiber = sylar::Fiber::GetThis();
	sylar::IOManager *iom = sylar::IOManager::GetThis();
	iom->addTimer(seconds / 1000, 
			std::bind((void(sylar::Scheduler::*)(sylar::Fiber::ptr, int thread))&sylar::IOManager::schedule, iom, fiber, -1)
		);
	sylar::Fiber::YieldToHold();
	return 0;
}

int nanpsleep(const struct timespec *req, struct timespec *rem)
{
	if(!sylar::t_hook_enable)
		return nanosleep_f(req, rem);
	int timeout_ms = req->tv_sec * 1000 + req->tv_nsec / 1000 / 1000;
	sylar::Fiber::ptr fiber = sylar::Fiber::GetThis();
	sylar::IOManager *iom = sylar::IOManager::GetThis();
	iom->addTimer(timeout_ms, 
			std::bind((void(sylar::Scheduler::*)(sylar::Fiber::ptr, int thread))&sylar::IOManager::schedule, iom, fiber, -1)
		);
	sylar::Fiber::YieldToHold();
	return 0;
}

//socket
int socket(int domain, int type, int protocol)
{
	if(!t_hook_enable)
		return socket_f(domain, type, protocol);
	int fd = socket_f(domain, type, protocol);
	if(fd == -1)
		return fd;
	FdMgr::GetInstance()->get(fd, true);
	return fd;
}

//
int connect_with_timeout(int fd, const struct sockaddr *addr, socklen_t addrlen, uint64_t time_out)
{
	if(!t_hook_enable)
		return connect_f(fd, addr, addrlen);

	FdCtx::ptr ctx = FdMgr::GetInstance()->get(fd);
	if(!ctx || ctx->isClose()){
			errno = EBADF;
			return -1;
		}
	if(!ctx->isSocket() || ctx->getUserNonblock())
		return connect_f(fd, addr, addrlen);
	
	int n = connect_f(fd, addr, addrlen);
	
	//connect成功
	if(n == 0 || n != -1 || (n == -1 && errno == EINPROGRESS))
		return n;
	
	IOManager *iom = IOManager::GetThis();
	Timer::ptr timer;
	std::shared_ptr<timer_info> tinfo(new timer_info);
	std::weak_ptr<timer_info> winfo(tinfo);

	if(time_out != (uint64_t)-1)
		{
			timer = iom->addConditionTimer(time_out, [winfo, fd, iom]()
				{
					auto t = winfo.lock();
					if(!t || t->cancelled)
						return;
					t->cancelled = ETIMEDOUT;
					iom->cancelEvent(fd, IOManager::WRITE);
				}, winfo);
		}
	
	int rt = iom->addEvent(fd, IOManager::WRITE);
	if(rt == 0)
		{
			Fiber::YieldToHold();
			if(timer)
				timer->cancel();
			if(tinfo->cancelled){
					errno = tinfo->cancelled;
					return -1;
				}
		}
	else{
			if(timer)
				timer->cancel();
			SYLAR_LOG_ERROR(g_logger) << "connect addEvent " << fd << ", WRITE) error";
		}
	int error = 0;
	socklen_t len = sizeof(int);
	if(-1 == getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len))
		return -1;
	
	if(!error)
			return 0;
	errno = error;
	return -1;
	
	
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
	return connect_with_timeout(sockfd, addr, addrlen, s_connect_timeout);
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
	int fd = do_io(sockfd, accept_f, "accept", IOManager::READ, SO_RCVTIMEO, addr, addrlen);
	if(fd >= 0)
		FdMgr::GetInstance()->get(sockfd, true);
	return fd;
}

//read
ssize_t read(int fd, void *buf, size_t count)
{
	// 第二个参数一定要用原始方法, 不然会陷入死循环
	return do_io(fd, read_f, "read", IOManager::READ, SO_RCVTIMEO, buf, count);
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt)
{
	return do_io(fd, readv_f, "readv", IOManager::READ, SO_RCVTIMEO, iov, iovcnt);
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags)
{
	return do_io(sockfd, recv_f, "recv", IOManager::READ, SO_RCVTIMEO, buf, len, flags);
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen)
{
	return do_io(sockfd, recvfrom_f, "recvfrom", IOManager::READ, SO_RCVTIMEO, buf, len, flags, src_addr, addrlen);
}

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags)
{
	return do_io(sockfd, recvmsg_f, "recvmsg", IOManager::READ, SO_RCVTIMEO, msg, flags);
}

//write
ssize_t write(int fildes, const void *buf, size_t nbyte)
{
	return do_io(fildes, write_f, "write", IOManager::WRITE, SO_SNDTIMEO, buf, nbyte);
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt)
{
	return do_io(fd, writev_f, "writev", IOManager::WRITE, SO_SNDTIMEO, iov, iovcnt);
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags)
{
	return do_io(sockfd, send_f, "send", IOManager::WRITE, SO_SNDTIMEO, buf, len, flags);
}

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)
{
	return do_io(sockfd, sendto_f, "sendto", IOManager::WRITE, SO_SNDTIMEO, buf, len, flags, dest_addr, addrlen);
}

ssize_t sendmsg_fun(int sockfd, const struct msghdr *msg, int flags)
{
	return do_io(sockfd, sendmsg_f, "sendmsg", IOManager::WRITE, SO_SNDTIMEO, msg, flags);
}

int close(int fd)
{
	if(!t_hook_enable)
		return close_f(fd);
	FdCtx::ptr ctx = FdMgr::GetInstance()->get(fd);
	if(ctx)
		{
			IOManager *iom = IOManager::GetThis();
			if(iom)
				iom->cancelAll(fd);
			FdMgr::GetInstance()->del(fd);
		}
	return close_f(fd);
}


//cntl
int fcntl(int fd, int cmd, ... /* arg */ )
{
	va_list va;
	va_start(va, cmd);
	switch(cmd)
		{
			case F_SETFL:
				{
					int arg = va_arg(va, int);
					va_end(va);
					FdCtx::ptr ctx = FdMgr::GetInstance()->get(fd, false);
					if(!ctx || ctx->isClose() || !ctx->isSocket())
						return fcntl_f(fd, cmd, arg);
					ctx->setUserNonblock(arg & O_NONBLOCK);
					if(ctx->getSysNonblock())
						arg |= O_NONBLOCK;
					else
						arg &= ~O_NONBLOCK;
					return fcntl_f(fd, cmd, arg);
				}
				break;
			case F_GETFL:
				{
					va_end(va);
					int arg = fcntl_f(fd, cmd);
					FdCtx::ptr ctx = FdMgr::GetInstance()->get(fd, false);
					if(!ctx || ctx->isClose() || !ctx->isSocket())
						return arg;
					if(ctx->getUserNonblock())
						return arg | O_NONBLOCK;
					else
						return arg & ~O_NONBLOCK;
				}
				break;
			case F_DUPFD:
			case F_DUPFD_CLOEXEC:
			case F_SETFD:
			case F_SETOWN:
			case F_SETSIG:
			case F_SETLEASE:
			case F_NOTIFY:
			case F_SETPIPE_SZ:
				{
					int arg = va_arg(va, int);
					va_end(va);
					return fcntl_f(fd, cmd, arg);		
				}
				break;
			
			//void
			case F_GETFD:
			case F_GETOWN:
			case F_GETSIG:
			case F_GETLEASE:
			case F_GETPIPE_SZ:
				{
					va_end(va);
					return fcntl_f(fd, cmd);
				}
				break;
			case F_SETLK:
			case F_SETLKW: 
			case F_GETLK:
				{
					struct flock *arg = va_arg(va, struct flock *);
					va_end(va);
					return fcntl_f(fd, cmd, arg);
				}
				break;
			case F_GETOWN_EX:
			case F_SETOWN_EX:
				{
					struct  f_owner_ex *arg = va_arg(va, struct f_owner_ex *);
					va_end(va);
					return fcntl_f(fd, cmd, arg);
				}
			default:
				va_end(va);
				return fcntl_f(fd, cmd);
		}
		 
}

int ioctl(int fd, unsigned long request, ...)
{
	va_list va;
	va_start(va, request);
	void *arg = va_arg(va, void *);
	va_end(va);

	if(FIONBIO == request)
		{
		    bool user_nonblock = *(int*)arg;
		    sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->get(fd);
		    if(!ctx || ctx->isClose() || !ctx->isSocket()) {
		        	return ioctl_f(fd, request, arg);
		    	}
		    ctx->setUserNonblock(user_nonblock);
		    
		}
    return ioctl_f(fd, request, arg);
}

int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen)
{
	return getsockopt_f(sockfd, level ,optname, optval, optlen);
}

int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen)
{
	if(!t_hook_enable)
		return setsockopt_f(sockfd, level, optname, optval, optlen);
	
	if(level == SOL_SOCKET && (optname == SO_RCVTIMEO || optname == SO_SNDTIMEO) )
		{
			FdCtx::ptr ctx = FdMgr::GetInstance()->get(sockfd);
			if(ctx){
					const timeval *v = (const timeval *)optval;
					ctx->setTimeout(optname, v->tv_sec * 1000 + v->tv_usec / 1000);
				}
		}
	return setsockopt_f(sockfd, level, optname, optval, optlen);
}

}


}





