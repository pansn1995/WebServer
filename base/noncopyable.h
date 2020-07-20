#ifndef NOCOPYABLE_H
#define NOCOPYABLE_H

//不可拷贝的类
class noncopyable
{
protected:
	noncopyable() {}
	~noncopyable() {}
private:
	noncopyable(const noncopyable&);
	const noncopyable& operator=(const noncopyable&);
};
#endif