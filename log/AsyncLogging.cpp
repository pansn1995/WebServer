#include "AsyncLogging.h"
#include "LogFile.h"
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <functional>

AsyncLogging::AsyncLogging(std::string logFileName_, int flushInterval)
    : flushInterval_(flushInterval), 
    running_(false),
    basename_(logFileName_),
    thread_(std::bind(&AsyncLogging::threadFunc, this), "Logging"),
    mutex_(),
    cond_(mutex_),
    currentBuffer_(new Buffer),
    nextBuffer_(new Buffer),
    buffers_(),
    latch_(1) 
{
    assert(logFileName_.size() > 1);
    currentBuffer_->bzero();
    nextBuffer_->bzero();
    buffers_.reserve(16);
}

void AsyncLogging::append(const char *logline, int len) {
    MutexLockGuard lock(mutex_);
    if(currentBuffer_->avail() > len) {
        //当前缓冲区足够大，直接追加写
        currentBuffer_->append(logline, len);
    } else {
        //将当前缓冲区加入buffers_中
        buffers_.push_back(currentBuffer_);
        //清空缓冲区
        currentBuffer_.reset();
        if(nextBuffer_) {
            //有下一个缓冲，直接拿来用，使用移动语义
            currentBuffer_ = std::move(nextBuffer_);
            nextBuffer_.reset();
        } else {
            currentBuffer_.reset(new Buffer);
        }
        currentBuffer_->append(logline, len);
        //通知后端线程有个缓冲区写满了
        cond_.notify();
    }
}

void AsyncLogging::threadFunc() {
    assert(running_ == true);
    latch_.countDown();
    LogFile output(basename_);
    BufferPtr newBuffer1(new Buffer);//新currentBuffer_
    BufferPtr newBuffer2(new Buffer);//新nextBuffer_
    newBuffer1->bzero();
    newBuffer2->bzero();
    BufferVector buffersToWrite;
    buffersToWrite.reserve(16);
    while(running_) {
        assert(newBuffer1 && newBuffer1->length() == 0);
        assert(newBuffer2 && newBuffer2->length() == 0);
        assert(buffersToWrite.empty());
        {
            MutexLockGuard lock(mutex_);
            //不用while，是因为超时的时候，没有缓冲是满的，需要将当前没有写满的缓冲区写入日志
            if(buffers_.empty()) {
                //当前buffers_为空，没有缓冲区满，等待
                cond_.waitForSeconds(flushInterval_);
            }
            buffers_.push_back(std::move(currentBuffer_));//移动
            currentBuffer_ = std::move(newBuffer1);//移动
            buffersToWrite.swap(buffers_);//内部指针交换，清空buffers_
            if(!nextBuffer_) {
                nextBuffer_ = std::move(newBuffer2);//移动
            }
        }

        //开始写入日志
        assert(buffers_.empty());

        if(buffersToWrite.size() > 25) {//要发送的缓冲区太多了，丢弃直接截断为2
            buffersToWrite.erase(buffersToWrite.begin() + 2, buffersToWrite.end());
        }

        for(size_t i = 0,lim=buffersToWrite.size(); i < lim; i++) {
            output.append(buffersToWrite[i]->data(), buffersToWrite[i]->length());
        }

        if(buffersToWrite.size() > 2) {
            buffersToWrite.resize(2);
        }

        //如果newBuffer1和newBuffer1有空的，buffersToWrite提供两个缓冲区供给newBuffer1和newBuffer2使用

        if(!newBuffer1) {
            assert(!buffersToWrite.empty());
            newBuffer1 = buffersToWrite.back();
            buffersToWrite.pop_back();
            newBuffer1->reset();
        }

        if(!newBuffer2) {
            assert(!buffersToWrite.empty());
            newBuffer2 = buffersToWrite.back();
            buffersToWrite.pop_back();
            newBuffer2->reset();
        }   
        buffersToWrite.clear();
        output.flush();
    }   
    output.flush();
}
