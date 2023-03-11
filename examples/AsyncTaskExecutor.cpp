#include "AsyncTaskExecutor.h"
#include "BlockQueue.hpp"

#include <memory>
#include <atomic>

using namespace std::chrono_literals;

using GraphTask = tf::Taskflow;
using AsyncExecutor = tf::Executor;
using AsyncWaitSign = std::future<void>;
using AsyncTask = std::function<void()>;
using AsyncTaskBlockQueue = BlockQueue<AsyncTask>;

///////////////////////////////////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//	异步执行器和相关定义
//-----------------------------------------------------------------------------
class AsyncTaskExecutor
{
public:
    AsyncTaskExecutor(const AsyncTaskExecutor&) = delete;
    AsyncTaskExecutor(AsyncTaskExecutor&&) = delete;
    AsyncTaskExecutor& operator=(const AsyncTaskExecutor&) = delete;
    AsyncTaskExecutor& operator=(AsyncTaskExecutor&&) = delete;
    ~AsyncTaskExecutor();

    static AsyncTaskExecutor* GetInstance();
    static void ReleaseInstance();
    static AsyncExecutor* GetExecutor();

    static void SetWorkerSize(size_t num);
    static void SetQueueMaxSize(int num);
    static void SetWorkWaitMaxTime(std::chrono::milliseconds time);

    std::pair<AsyncWaitSign, bool> Submit(GraphTask* tf);
    std::pair<AsyncWaitSign, bool> BlockSubmitUntil(GraphTask* tf, std::chrono::milliseconds time);
    AsyncWaitSign BlockSubmit(GraphTask* tf);

private:
    AsyncTaskExecutor();

    void Start();
    void BuildBlockQueueTask(GraphTask* tf, std::pair<AsyncTask, AsyncWaitSign>& tp);

    static std::unique_ptr<AsyncTaskExecutor> sPtr_;
    static std::mutex singletonMu_;

    static size_t wkMaxSz_;
    static int queMaxSz_;
    static std::chrono::milliseconds wkWtMaxMs_;

    std::unique_ptr<AsyncExecutor> wkPtr_;
    std::unique_ptr<AsyncTaskBlockQueue> quePtr_;

    std::thread commWk_;
    std::atomic<bool> stop_;
};

std::unique_ptr<AsyncTaskExecutor> AsyncTaskExecutor::sPtr_ = nullptr;
std::mutex AsyncTaskExecutor::singletonMu_;

size_t AsyncTaskExecutor::wkMaxSz_ = 1;
int AsyncTaskExecutor::queMaxSz_ = 200;
std::chrono::milliseconds AsyncTaskExecutor::wkWtMaxMs_ = 100ms;

AsyncTaskExecutor::AsyncTaskExecutor() : stop_(true)
{
}

AsyncTaskExecutor::~AsyncTaskExecutor()
{
    stop_.store(true);
    if (commWk_.joinable()) {
        commWk_.join();
    }
}

AsyncTaskExecutor* AsyncTaskExecutor::GetInstance()
{
    if (sPtr_ == nullptr) {
        std::lock_guard<std::mutex> lock(singletonMu_);
        if (sPtr_ == nullptr) {
            sPtr_ = std::unique_ptr<AsyncTaskExecutor>(new AsyncTaskExecutor);
            sPtr_->Start();
        }
    }

    return sPtr_.get();
}

void AsyncTaskExecutor::ReleaseInstance()
{
    if (sPtr_ != nullptr) {
        std::lock_guard<std::mutex> lock(singletonMu_);
        if (sPtr_ != nullptr) {
            sPtr_.reset();
            sPtr_ = nullptr;
        }
    }
}

tf::Executor* AsyncTaskExecutor::GetExecutor()
{
    return GetInstance()->wkPtr_.get();
}

std::pair<AsyncWaitSign, bool> AsyncTaskExecutor::Submit(GraphTask* tf)
{
    std::pair<AsyncTask, AsyncWaitSign> tp;
    BuildBlockQueueTask(tf, tp);

    std::pair<std::future<void>, bool> res;
    if (!quePtr_->Offer(std::move(tp.first))) {
        res.second = false;
        return res;
    }
    else {
        res.first = std::move(tp.second);
        res.second = true;
        return res;
    }
}

std::pair<AsyncWaitSign, bool> AsyncTaskExecutor::BlockSubmitUntil(GraphTask* tf, std::chrono::milliseconds time)
{
    std::pair<AsyncTask, AsyncWaitSign> tp;
    BuildBlockQueueTask(tf, tp);

    std::pair<std::future<void>, bool> res;
    if (!quePtr_->Offer(std::move(tp.first), time)) {
        res.second = false;
        return res;
    }
    else {
        res.first = std::move(tp.second);
        res.second = true;
        return res;
    }
}

AsyncWaitSign AsyncTaskExecutor::BlockSubmit(GraphTask* tf)
{
    std::pair<AsyncTask, AsyncWaitSign> tp;
    BuildBlockQueueTask(tf, tp);
    quePtr_->Put(std::move(tp.first));
    return std::move(tp.second);
}

void AsyncTaskExecutor::Start()
{
    wkPtr_ = std::make_unique<AsyncExecutor>(wkMaxSz_);
    quePtr_ = std::make_unique<AsyncTaskBlockQueue>(queMaxSz_);
    stop_.store(false);

    commWk_ = std::thread([this] {
        while (!this->stop_.load()) {
            AsyncTask task;

            if (this->stop_.load() && this->quePtr_->Empty()) {
                return;
            }

            if (this->stop_.load() && !this->quePtr_->Empty()) {
                while (this->quePtr_->Poll(task)) {
                    task();
                }
            }

            if (this->quePtr_->Poll(task, wkWtMaxMs_)) {
                task();
            }
        }
        });
}

void AsyncTaskExecutor::BuildBlockQueueTask(GraphTask* tf, std::pair<AsyncTask, AsyncWaitSign>& tp)
{
    std::shared_ptr<std::promise<void>> task_promise(new std::promise<void>);
    std::future<void> future = task_promise->get_future();

    auto func = [this, task_promise, tf]() mutable {
        this->wkPtr_->run(*tf).wait();
        task_promise->set_value();
    };

    tp.first = std::move(func);
    tp.second = std::move(future);
}

void AsyncTaskExecutor::SetWorkerSize(size_t num)
{
    wkMaxSz_ = num;
}

void AsyncTaskExecutor::SetQueueMaxSize(int num)
{
    queMaxSz_ = num;
}

void AsyncTaskExecutor::SetWorkWaitMaxTime(std::chrono::milliseconds time)
{
    wkWtMaxMs_ = time;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//	外部相关全局函数接口定义
//-----------------------------------------------------------------------------
void SetAsyncWorkerMaxSize(size_t wkMaxSz)
{
    AsyncTaskExecutor::SetWorkerSize(wkMaxSz);
}

void SetAsyncTaskQueMaxSize(int queMaxSz)
{
    AsyncTaskExecutor::SetQueueMaxSize(queMaxSz);
}

void SetAsyncCommMaxWaitTime(std::chrono::milliseconds time)
{
    AsyncTaskExecutor::SetWorkWaitMaxTime(time);
}

void ReleaseAsyncExecutor()
{
    AsyncTaskExecutor::ReleaseInstance();
}

AsyncExecutor* GetAsyncExecutorWoker()
{
    return AsyncTaskExecutor::GetExecutor();
}

std::pair<AsyncWaitSign, bool> RunAsyncTask(tf::Taskflow* tf)
{
    return AsyncTaskExecutor::GetInstance()->Submit(tf);
}

std::pair<AsyncWaitSign, bool> RunAsyncTaskUntil(tf::Taskflow* tf, std::chrono::milliseconds time)
{
    return AsyncTaskExecutor::GetInstance()->BlockSubmitUntil(tf, time);
}

AsyncWaitSign BlockRunAsyncTask(tf::Taskflow* tf)
{
    return AsyncTaskExecutor::GetInstance()->BlockSubmit(tf);
}