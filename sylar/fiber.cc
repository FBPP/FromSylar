#include "fiber.h"
#include "config.h"
#include "macro.h"
#include "log.h"
#include "scheduler.h"
#include <atomic>

namespace sylar
{
static Logger::ptr g_logger = SYLAR_LOG_NAME("system");
static std::atomic<uint64_t> s_fiber_id {0};
static std::atomic<uint64_t> s_fiber_count {0};

static thread_local Fiber *t_fiber = nullptr; //指向当前协程的指针
static thread_local Fiber::ptr t_threadFiber = nullptr; //指向线程的主协程的智能指针

static ConfigVar<uint32_t>::ptr g_fiber_stack_size = Config::Lookup<uint32_t>("fiber.stack_size", 1024 * 1024, "fiber stack size"); 

class MallocStackAllocator
{
public:
	static void * Alloc(size_t size)
	{
		return malloc(size);
	}
	
	static void Dealloc(void *vp, size_t size)
	{
		return free(vp);
	}
};

using StackAllocater = MallocStackAllocator;

//保存主协程的上下文在m_ctx
Fiber::Fiber()
{
	m_state = EXEC;
	SetThis(this);

	if(getcontext(&m_ctx))
			SYLAR_ASSERT2(false, "getcontext");
	++s_fiber_count;
	SYLAR_LOG_DEBUG(g_logger) << "Fiber::Fiber main";
}

//创建一个子协程, m_ctx保存子协程的上下文, 并指定子协程的回调函数
Fiber::Fiber(std::function<void()> cb, size_t stacksize, bool user_caller)
	: m_id(++s_fiber_id), m_cb(cb)
{
	++s_fiber_count;
	m_stacksize = stacksize ? stacksize : g_fiber_stack_size->getValue();

	m_stack = StackAllocater::Alloc(m_stacksize);
	if(getcontext(&m_ctx))
			SYLAR_ASSERT2(false, "getcontext");
	m_ctx.uc_link = nullptr;
	m_ctx.uc_stack.ss_sp = m_stack;
	m_ctx.uc_stack.ss_size = m_stacksize;

	if(!user_caller)
		makecontext(&m_ctx, &Fiber::MainFunc, 0);
	else
		makecontext(&m_ctx, &Fiber::CallerMainFunc, 0);
	//SYLAR_LOG_DEBUG(g_logger) << "Fiber::Fiber() id=" << m_id;
}

Fiber::~Fiber()
{
	--s_fiber_count;
	if(m_stack)
		{
			SYLAR_ASSERT(m_state == TERM || m_state == INIT || m_state == EXCEPT);
			StackAllocater::Dealloc(m_stack, m_stacksize);
		}
	else
		{
			SYLAR_ASSERT(!m_cb);
			SYLAR_ASSERT(m_state == EXEC);

			Fiber *cur = t_fiber;
			if(cur == this)
					SetThis(nullptr);
		}
	//SYLAR_LOG_DEBUG(g_logger) << "Fiber::~Fiber() id=" << m_id;
}

//重置协程函数, 并重置状态
void Fiber::reset(std::function<void()> cb)
{
	SYLAR_ASSERT(m_stack);
	SYLAR_ASSERT(m_state == TERM || m_state == INIT || m_state == EXCEPT);
	m_cb = cb;
	if(getcontext(&m_ctx))
		SYLAR_ASSERT2(false, "getcontext");
	m_ctx.uc_link = nullptr;
	m_ctx.uc_stack.ss_sp = m_stack;
	m_ctx.uc_stack.ss_size = m_stacksize;

	makecontext(&m_ctx, &Fiber::MainFunc, 0);
	m_state = INIT;
}
//从当前线程的主协程, 切换到当前对象对应的协程
void Fiber::call()
{
	SetThis(this);
	m_state = EXEC;
	if(swapcontext(&t_threadFiber->m_ctx, &m_ctx)){
			SYLAR_ASSERT2(false, "swapcontext");
		}
}
//从当前对象的的协程, 切换到主协程
void Fiber::back()
{
	SetThis(t_threadFiber.get());
	if(swapcontext(&m_ctx, &t_threadFiber->m_ctx))
			SYLAR_ASSERT2(false, "swapcontext");
}

//切换到当前协程执行
void Fiber::swapIn()
{
	SetThis(this);
	SYLAR_ASSERT(m_state != EXEC);
	m_state = EXEC;
	if(swapcontext(&Scheduler::GetMainFiber()->m_ctx, &m_ctx))
			SYLAR_ASSERT2(false, "swapcontext");
}

//当前协程切换到后台
void Fiber::swapOut()
{
	SetThis(Scheduler::GetMainFiber());
		if(swapcontext(&m_ctx, &Scheduler::GetMainFiber()->m_ctx))
			SYLAR_ASSERT2(false, "swapcontext");
}

//设置执行当前线程正在执行的协程
void Fiber::SetThis(Fiber *f)
{
	t_fiber = f;
}

//返回正在运行的协程的智能指针
Fiber::ptr Fiber::GetThis()
{
	if(t_fiber)
		return t_fiber->shared_from_this();
	//static函数, 如果没有创建一个协程, 会创建一个朱携程, 并将 t_fiber 和t_threadFiber指向这个主协程
	Fiber::ptr main_fiber(new Fiber);
	SYLAR_ASSERT(t_fiber == main_fiber.get());
	t_threadFiber = main_fiber;
	return t_fiber->shared_from_this();
}
//协程切换到后台并且设置为ready状态
void Fiber::YieldToReady()
{
	Fiber::ptr cur = GetThis();
	cur->m_state = READY;
	cur->swapOut();
}
//协程切换到后台并且设置为hold状态
void Fiber::YieldToHold()
{
	Fiber::ptr cur = GetThis();
	//cur->m_state = HOLD;
	cur->swapOut();
}
//协程总数
uint64_t Fiber::TotalFibers()
{
	return s_fiber_count;
}
//获得子协程id
uint32_t Fiber::GetFiberId()
{
	if(t_fiber)
			return t_fiber->getId();
	return 0;
}

//static
//!user_caller 
//子协程的运行函数, 执行完后会导致线程终止, 因此不能执行完, 执行完后通过reset的方式清空栈以及添加函数
void Fiber::MainFunc()
{
	Fiber::ptr cur = GetThis(); //返回当前运行的协程的所对应的对象的指针指针
	SYLAR_ASSERT(cur);
	try {
			cur->m_cb();	   //在当前协程栈中调用对象中的回调函数
			cur->m_cb = nullptr;
			cur->m_state = TERM;
		}
	catch(std::exception &ex)
		{
			cur->m_state = EXCEPT;
			SYLAR_LOG_ERROR(g_logger) << "Fiber Except: " << ex.what()
				<< std::endl
				<< sylar::BacktraceToString(); 
		}
	catch(...)
		{
			cur->m_state = EXCEPT;
			SYLAR_LOG_ERROR(g_logger) << "Fiber Except: "
				<< std::endl
				<< sylar::BacktraceToString();
		}

	auto raw_ptr = cur.get();
	cur.reset(); //智能指针计数--
	raw_ptr->swapOut(); //如果直接用智能指针swapout, 会导致智能指针无法销毁, 引用计数无法-1
	SYLAR_ASSERT2(false, "never reach");
}

//user_caller
void Fiber::CallerMainFunc()
{
	Fiber::ptr cur = GetThis();
	SYLAR_ASSERT(cur);
	try {
			cur->m_cb();	
			cur->m_cb = nullptr;
			cur->m_state = TERM;
		}
	catch(std::exception &ex)
		{
			cur->m_state = EXCEPT;
			SYLAR_LOG_ERROR(g_logger) << "Fiber Except: " << ex.what()
				<< std::endl
				<< sylar::BacktraceToString(); 
		}
	catch(...)
		{
			cur->m_state = EXCEPT;
			SYLAR_LOG_ERROR(g_logger) << "Fiber Except: "
				<< std::endl
				<< sylar::BacktraceToString();
		}

	auto raw_ptr = cur.get();
	cur.reset(); //智能指针计数--
	raw_ptr->back();
	SYLAR_ASSERT2(false, "never reach");
}


}
