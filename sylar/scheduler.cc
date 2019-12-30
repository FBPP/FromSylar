#include "scheduler.h"
#include "log.h"
#include "macro.h"
#include "hook.h"

namespace sylar
{

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");
static thread_local Scheduler *t_scheduler = nullptr; //指向调度器的对象的指针
static thread_local Fiber *t_fiber = nullptr;  //指向每个线程的run函数所在协程的指针

Scheduler::Scheduler(size_t threads, bool use_caller, const std::string &name)
	: m_name(name)
{
	SYLAR_ASSERT(threads > 0);
	//使用主线程创建协程. 主线程会参与处理消息的工作
	if(use_caller)
		{
			sylar::Fiber::GetThis();
			--threads;
			SYLAR_ASSERT(GetThis() == nullptr);
			t_scheduler = this;
			//创建子协程
			m_rootFiber.reset(new Fiber(std::bind(&Scheduler::run, this), 0, true));
			sylar::Thread::SetName(m_name);
			t_fiber = m_rootFiber.get();
			m_rootThread = sylar::GetThreadId();
			m_threadIds.push_back(m_rootThread);
		}
	//不使用主线程创建协程, 主线程不会参与处理消息的工作
	else
		{
			m_rootThread = -1;
		}
	m_threadCount = threads;
}

Scheduler::~Scheduler()
{
	SYLAR_ASSERT(m_stopping);
	if(GetThis() == this)
		t_scheduler = nullptr;
}

Scheduler *Scheduler::GetThis()
{
	return t_scheduler;
}

Fiber *Scheduler::GetMainFiber()
{
	return t_fiber;
}

void Scheduler::start()
{
	MutexType::Lock lock(m_mutex);
	if(!m_stopping)
		return;
	m_stopping = false;
	SYLAR_ASSERT(m_threads.empty());

	m_threads.resize(m_threadCount);
	for(size_t i = 0; i < m_threadCount; ++i)
		{
			m_threads[i].reset(new Thread(std::bind(&Scheduler::run, this), m_name + "_" + std::to_string(i)));
			m_threadIds.push_back(m_threads[i]->getId());
		}
}

void Scheduler::stop()
{
	SYLAR_LOG_DEBUG(g_logger) << "stop";
	m_autoStop = true;
	if(m_rootFiber && m_threadCount == 0 && (m_rootFiber->getState() == Fiber::TERM || m_rootFiber->getState() == Fiber::INIT))
		{
			SYLAR_LOG_INFO(g_logger) << this << " stopped";
			m_stopping = true;
			if(stopping())
				return;
		}
	
	if(m_rootThread != -1){
			SYLAR_ASSERT(GetThis() == this);
		}
	else{
			SYLAR_ASSERT(GetThis() != this);
		}
	m_stopping = true;
	for(size_t i = 0; i < m_threadCount; ++i)
		tickle();
	if(m_rootFiber)
		tickle();
	//如果主线程的子协程存在, 切换到子协程执行
	if(m_rootFiber && !stopping())
		m_rootFiber->call();
	
	std::vector<Thread::ptr> thrs;
	{
		MutexType::Lock lock(m_mutex);
		thrs.swap(m_threads);
	}
	for(auto &i : thrs){
			i->join();
		}
	
}

void Scheduler::setThis()
{
	t_scheduler = this;
}

void Scheduler::run()
{
	SYLAR_LOG_INFO(g_logger) << "run";
	Set_hook_enable(true);
	setThis();

	//如果当前线程不是主线程, 创建协程对象, 并让当前线程的t_fiber指向这个协程对象
	if(sylar::GetThreadId() != m_rootThread)
		t_fiber = Fiber::GetThis().get();
	//在当前协程的栈中创建一个智能指针, 指向一个新的协程, ,这个协程的user_caller = false
	//这个协程的主函数是MainFunc, 使用swapin swapout切换, 回调函数是调度器对象的idle
	Fiber::ptr idle_fiber(new Fiber(std::bind(&Scheduler::idle, this)));
	//第二个智能指针, 指向另一个协程, 这个协程专门用来执行消息中的回调函数
	Fiber::ptr cb_fiber;

	FiberAndThread ft;
	while(1)
		{
			ft.reset();
			bool tickle_me = false;
			bool is_active = false;	
			{
				MutexType::Lock lock(m_mutex);
				auto it = m_fibers.begin();
				while(it != m_fibers.end())
					{
						//-1代表任意线程都可以拿出消息队列里的这个ft对象,  
						// 第二个条件可以指定,哪个线程可以拿
						if(it->thread != -1 && it->thread != sylar::GetThreadId()){
								++it;
								//我不能拿,消息又存在, 说明其他线程可以拿但是没有拿, 说明其他线程在阻塞, 唤醒他们. 让他们来拿消息
								tickle_me = true;
								continue;
							}
						SYLAR_ASSERT(it->fiber || it->cb);
						//?
						if(it->fiber && it->fiber->getState() == Fiber::EXEC){
								++it;
								continue;
							}


						//把消息拿出来, 并从队列中删除
						ft = *it;
						m_fibers.erase(it);
						++m_activeThreadCount;
						is_active = true;
						break;
					}
			}
				if(tickle_me)
					tickle();

				if(ft.fiber && (ft.fiber->getState() != Fiber::TERM || ft.fiber->getState() != Fiber::EXCEPT))
					{						
						ft.fiber->swapIn();
						--m_activeThreadCount;

						if(ft.fiber->getState() == Fiber::READY)
							schedule(ft.fiber);
						else if(ft.fiber->getState() != Fiber::TERM && ft.fiber->getState() != Fiber::EXCEPT)
							ft.fiber->m_state = Fiber::HOLD;
						ft.reset();
					}
				else if(ft.cb)
					{
						if(cb_fiber)
							//让cb_fiber指向的fiber对象重新设置协程栈, 并赋予cb
							cb_fiber->reset(ft.cb);
						else
							//没有fiber对象, 创建一个
							cb_fiber.reset(new Fiber(ft.cb));
						ft.reset();
						cb_fiber->swapIn();
						--m_activeThreadCount;
						
						if(cb_fiber->getState() == Fiber::READY){
								schedule(cb_fiber);
								cb_fiber.reset();
							}
						else if(cb_fiber->getState() == Fiber::EXCEPT || cb_fiber->getState() == Fiber::TERM){
								cb_fiber->reset(nullptr);
							}
						else{
								cb_fiber->m_state = Fiber::HOLD;
								cb_fiber.reset();
							}
							
					}
				else
					{
						if(is_active){
								--m_activeThreadCount;
								continue;
							}
						if(idle_fiber->getState() == Fiber::TERM){
								SYLAR_LOG_INFO(g_logger) << "idle fiber term";
								break;
							}
						++m_idleThreadCount;
						idle_fiber->swapIn();
						--m_idleThreadCount;
						if(idle_fiber->getState() != Fiber::TERM && idle_fiber->getState() != Fiber::EXCEPT)
							idle_fiber->m_state = Fiber::HOLD;
					}
			
		}
}

void Scheduler::tickle()
{
	SYLAR_LOG_INFO(g_logger) << "tickle";
}

bool Scheduler::stopping()
{
	MutexType::Lock lock(m_mutex);
	return m_autoStop && m_stopping && m_fibers.empty() && m_activeThreadCount == 0;
}

void Scheduler::idle()
{	
	while(!stopping()){
			SYLAR_LOG_INFO(g_logger) << "idle";
			Fiber::YieldToHold();
			sleep(1);
		}
	
}


}
