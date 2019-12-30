#include "../sylar/sylar.h"

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();


static sylar::Logger::ptr logger1(new sylar::Logger);
static sylar::Logger::ptr logger2(new sylar::Logger);




void fun1()
{

	SYLAR_LOG_INFO(g_logger) << "name: " << sylar::Thread::GetName()
							 << " this.name: " << sylar::Thread::GetThis()->getName()
							 << " tid: " << sylar::GetThreadId()
							 << " this.tid:" << sylar::Thread::GetThis()->getId();
}

void fun2()
{
	while(1)
		SYLAR_LOG_INFO(g_logger) << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
}

void fun3()
{
	while(1)
		SYLAR_LOG_INFO(g_logger) << "===========================================================";
}

int main(int argc, char *argv[])
{
	SYLAR_LOG_INFO(g_logger) << "thread test begin";
	
	YAML::Node root = YAML::LoadFile("/home/pengpeng/windows_shared/sylar/bin/conf/log2.yml");
	sylar::Config::LoadFromYaml(root);

	std::vector<sylar::Thread::ptr> thrs;
	
	for(int i = 0; i < 1; ++i)
		{
			sylar::Thread::ptr thr(new sylar::Thread(&fun2, "name_" + std::to_string(i * 2)));
			sylar::Thread::ptr thr2(new sylar::Thread(&fun3, "name_" + std::to_string(i * 2 + 1)));
			thrs.push_back(thr);
			thrs.push_back(thr2);
		}
	for(size_t i = 0; i < thrs.size(); ++i)
		{
			thrs[i]->join();
		}
	SYLAR_LOG_INFO(g_logger) << "thread test end";
	return 0;
}
