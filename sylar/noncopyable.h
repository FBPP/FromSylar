#ifndef __SYLAR_NONCOPYABLE_H__
#define __SYLAR_NONCOPYABLE_H__

namespace sylar
{

class Noncopyable
{
public:
	Noncopyable() = default;
	~Noncopyable() = default;
	Noncopyable(const Noncopyable &) = default;
	Noncopyable &operator=(const Noncopyable&) = default;
};
}

#endif
