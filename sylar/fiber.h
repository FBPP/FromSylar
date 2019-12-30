#ifndef __SYLAR_FIBER_H__
#define __SYLAR_FIBER_H__
#include <ucontext.h>
#include <memory>
#include <functional>
#include "thread.h"
namespace sylar
{ //sylar

class Scheduler;
class Fiber : public std::enable_shared_from_this<Fiber>
{
friend class Scheduler;
public:
	typedef std::shared_ptr<Fiber> ptr;

	enum State
		{
			INIT,
			HOLD,
			EXEC,
			TERM,
			READY,
			EXCEPT
		};
			
	Fiber(std::function<void()> cb, size_t stacksize = 0, bool user_caller = false);
	~Fiber();

	//重置协程函数, 并重置状态
	void reset(std::function<void()> cb);
	//切换到当前协程执行
	void swapIn(); 
	//当前协程切换到后台
	void swapOut(); 
	void call();
	void back();
	uint32_t getId() const {return m_id;}
	State getState() const {return m_state;}	
	
	//设置当前协程
	static void SetThis(Fiber *f);
	//获得当前的协程
	static Fiber::ptr GetThis(); 
	//协程切换到后台并且设置为ready状态
	static void YieldToReady(); 
	//协程切换到后台并且设置为hold状态
	static void YieldToHold();
	//协程总数
	static uint64_t TotalFibers();
	//
	static uint32_t GetFiberId();
	
	static void MainFunc();
	static void CallerMainFunc();
private:
	Fiber();
	uint64_t m_id = 0; //子协程的id
	uint64_t m_stacksize = 0;
	State m_state = INIT; //主协程的状态
	
	ucontext_t m_ctx; //协程的上下文 
	void *m_stack = nullptr; //子协程的栈指针

	std::function<void()> m_cb; //子协程的回调
	
};
	

} //syar

#endif
