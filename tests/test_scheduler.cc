#include "../sylar/sylar.h"

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();


void test_fiber()
{
	SYLAR_LOG_INFO(g_logger) << "test in fiber";
	static int s_count = 5;
	if(--s_count >= 0)
		sylar::Scheduler::GetThis()->schedule(&test_fiber, sylar::GetThreadId());
	sleep(1);
			
}

int main(int argc, char *argv[])
{
	g_logger->addAppender(sylar::FileLogAppender::ptr( new sylar::FileLogAppender("log.txt")));
	SYLAR_LOG_INFO(g_logger) << "main";
	sylar::Scheduler sc(3, false, "test");
	sc.start();
	SYLAR_LOG_INFO(g_logger) << "schedule";
	sc.schedule(&test_fiber);
	sc.stop();
	SYLAR_LOG_INFO(g_logger) << "end";
	return 0;
}