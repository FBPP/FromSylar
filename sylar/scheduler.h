#ifndef __SYLAR_SCHEDULER_H__
#define __SYLAR_SCHEDULER_H__

#include <memory>
#include <vector>
#include <list>
#include <string>
#include "fiber.h"
#include "thread.h"

namespace sylar
{

class Scheduler
{
public:
	typedef std::shared_ptr<Scheduler> ptr;
	typedef Mutex MutexType;

	Scheduler(size_t threads = 1, bool use_caller = true, const std::string &name = "");
	virtual ~Scheduler();

	const std::string &getName() const {return m_name;}
	
	static Scheduler *GetThis();
	static Fiber *GetMainFiber();

	void start();
	void stop();

	template<typename FiberOrCb>
	void schedule(FiberOrCb fc, int thread = -1)
	{
		bool need_tickle = false;
		{
			MutexType::Lock lock(m_mutex);	
			need_tickle = scheduleNoLock(fc, thread);	
		}
		if(need_tickle)
			tickle();
	}

	template<typename InputIterator>
	void schedule(InputIterator begin, InputIterator end)
	{
		bool need_tickle = false;
		{
			MutexType::Lock lock(m_mutex);
			while(begin != end){
					need_tickle = scheduleNoLock(&*begin, -1) || need_tickle;
					++begin;
				}
		}
		if(need_tickle)
			tickle();
	}
protected:
	virtual void tickle();
	void run();
	virtual bool stopping();
	virtual void idle();
	
	void setThis();
	
	bool hasIdleThread() { return m_idleThreadCount > 0; }
private:
	template<typename FiberOrCb>
	bool scheduleNoLock(FiberOrCb fc, int thread)
	{
		bool need_tickle = m_fibers.empty();
		FiberAndThread ft(fc, thread);
		if(ft.fiber || ft.cb)
			m_fibers.push_back(ft);
		return need_tickle;
	}
private:
	struct FiberAndThread
	{
		Fiber::ptr fiber;
		std::function<void()> cb;
		int thread;

		FiberAndThread(Fiber::ptr f, int thr)
			: fiber(f), thread(thr) {}

		FiberAndThread(Fiber::ptr *f, int thr)
			: thread(thr)
		{
			swap(fiber, *f);
		}

		FiberAndThread(std::function<void()> f, int thr)
			: cb(f), thread(thr) {}

		FiberAndThread(std::function<void()> *f, int thr)
			: thread(thr)
		{
			swap(cb, *f);
		}

		FiberAndThread()
			: thread(-1) {}

		void reset()
		{
			fiber = nullptr;
			cb = nullptr;
			thread = -1;
		}
	};
private:
	MutexType m_mutex;
	std::vector<Thread::ptr> m_threads; //所有线程的线程池(包括主线程)
	std::list<FiberAndThread>m_fibers; //消息队列, 可以是Fiber::ptr 也可以是cb
	Fiber::ptr m_rootFiber; //指向主线程的子协程的智能指针
	std::string m_name;		
protected:
	std::vector<int> m_threadIds; //线程id数组
	size_t m_threadCount = 0;    //初始化线程数
	std::atomic<size_t> m_activeThreadCount = {0}; //正在工作中的线程数
	std::atomic<size_t> m_idleThreadCount = {0};   //闲置的线程数
	bool m_stopping = true;    //调度器是否停止
	bool m_autoStop;     //调度器自动关闭了   
	int m_rootThread = 0; //主线程id
};

}
#endif
