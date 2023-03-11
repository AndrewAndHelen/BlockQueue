#ifndef ASYNC_TASK_EXECUTOR_H
#define ASYNC_TASK_EXECUTOR_H

#include <taskflow/core/executor.hpp>

//! \brief 设置异步执行器内部工作线程数量
//! \param wkMaxSz [in] 线程数量
void SetAsyncWorkerMaxSize(size_t wkMaxSz);

//! \brief 设置异步执行器内部任务队列最大长度
//! \param queMaxSz [in] 任务队列长度
void SetAsyncTaskQueMaxSize(int queMaxSz);

//! \brief 设置异步执行器内部通讯线程最大阻塞时间
//! \param queMaxSz [in] 任务队列长度
void SetAsyncCommMaxWaitTime(std::chrono::milliseconds time);

//! \brief 释放异步执行器单例
void ReleaseAsyncExecutor();

//! \brief 获取异步执行器内部工作者，调用此函数可跳过异步执行器中的阻塞队列，将taskflow插队加入执行器中
tf::Executor* GetAsyncExecutorWoker();

//! \brief 非阻塞的将图任务加入到异步执行器的阻塞队列中
//! \param tf [in] 图任务的指针
//! \return std::pair<std::future<void>, bool> 如果成功插入到阻塞队列队尾，返回任务等待future和插入成功标志符的pair
//! \note 调用该函数后*tf会在工作者运行时被移动，外部不可再次使用tf::Executor进行运行
//! \note 由于不确定什么时候*tf会被移动，所以在*tf的生命周期中一定要阻塞等待该任务被工作者完成
std::pair<std::future<void>, bool> RunAsyncTask(tf::Taskflow* tf);

//! \brief 将图任务加入到异步执行器的阻塞队列中，最长阻塞一段时间
//! \param tf [in] 图任务的指针
//! \param time [in] 最大等待时间
//! \return std::pair<std::future<void>, bool> 如果成功插入到阻塞队列队尾，返回任务等待future和插入成功标志符的pair
//! \note 同上
std::pair<std::future<void>, bool> RunAsyncTaskUntil(tf::Taskflow* tf, std::chrono::milliseconds time);

//! \brief 将图任务加入到异步执行器的阻塞队列中，将一直阻塞直到插入成功
//! \param tf [in] 图任务的指针
//! \return std::pair<std::shared_ptr<bool>, bool> 如果成功插入到阻塞队列队尾，返回任务等待future和插入成功标志符的pair
//! \note 同上
std::future<void> BlockRunAsyncTask(tf::Taskflow* tf);

#endif