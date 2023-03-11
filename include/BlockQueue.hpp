#ifndef BLOCK_QUEUE_H
#define BLCOK_QUEUE_H

#include <mutex>
#include <condition_variable>
#include <deque>
#include <chrono>

///////////////////////////////////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//	阻塞队列类和相关定义
//-----------------------------------------------------------------------------
template<typename _Ty>
class BlockQueue
{
public:
    using SizeType = typename std::deque<_Ty>::size_type;

    //! \brief 阻塞队列构造函数，当maxSize为-1时，表示无界队列
    BlockQueue(const int maxSize = -1);

    //! \brief 阻塞队列析构函数
    ~BlockQueue() {}

    //! \brief 阻塞队列禁用拷贝构造函数
    BlockQueue(const BlockQueue&) = delete;

    //! \brief 阻塞队列禁用拷贝赋值函数
    BlockQueue& operator = (const BlockQueue&) = delete;

    //! \brief 往队尾放入一个元素，当为有界队列且容量已满，则调用该函数的线程一直阻塞直到队列不满
    //! \param _Val [in] 队列元素，可以为左值，右值，或者队列元素构造函数中的值
    template<typename... _Valty>
    void Put(_Valty&&... _Val);

    //! \brief 从队首弹出一个元素，如果队列为空时，则一直阻塞直到等待队列中有元素
    //! \return _Ty 返回队首元素
    //! \note 队首元素会直接移动到返回值，所以元素类型不能禁止移动构造
    _Ty Take();

    //! \brief 不阻塞的往队尾放入一个元素
    //! \param _Val [in] 队列元素，左值
    //! \return bool 如果是有界队列且队列容量已满，则返回false，否则返回true
    bool Offer(const _Ty& t);

    //! \brief 不阻塞的往队尾放入一个元素
    //! \param _Val [in] 队列元素，右值
    //! \return bool 如果是有界队列且队列容量已满，则返回false，否则返回true
    bool Offer(_Ty&& t);

    //! \brief 往队尾放入一个元素，如果队列满了，则阻塞等待一段时间
    //! \param _Val [in] 队列元素，左值
    //! \return bool 如果是有界队列且等待超时，则返回false，否则返回true
    bool Offer(const _Ty& t, const std::chrono::milliseconds& time);

    //! \brief 往队尾放入一个元素，如果队列满了，则阻塞等待一段时间
    //! \param _Val [in] 队列元素，右值
    //! \return bool 如果是有界队列且等待超时，则返回false，否则返回true
    bool Offer(_Ty&& t, const std::chrono::milliseconds& time);

    //! \brief 不阻塞的从队首提取一个元素
    //! \param _Val [in] 队首元素的引用
    //! \return bool 队列容量为空时，返回false，否则，返回true
    //! \note 队首元素会直接移动到返回值，所以元素类型不能禁止移动构造
    bool Poll(_Ty& t);

    //! \brief 从队首提取一个元素，最大阻塞一段时间
    //! \param _Val [in] 队首元素的引用
    //! \return bool 如果队列为空，则阻塞等待一段时间，等待后仍为空，返回false，否则，返回true
    //! \note 队首元素会直接移动到返回值，所以元素类型不能禁止移动构造
    bool Poll(_Ty& t, const std::chrono::milliseconds& time);

    //! \brief 判断队列是否为空
    //! \return bool 如果队列为空，返回true，否则，返回false
    bool Empty() const;

    //! \brief 判断队列是否已满
    //! \return bool 如果队列已满，返回true，否则，返回false
    bool Full() const;

    //! \brief 返回队列长度
    //! \return SizeType 队列长度
    SizeType Size() const;

private:
    template<typename... _Valty>
    void PutInternalImpl(_Valty&&... _Val);

    std::deque<_Ty> c_;
    const int maxSz_;
    mutable std::mutex mu_;
    std::condition_variable condNonEmpty_;
    std::condition_variable condNonFull_;
};

template<typename _Ty>
BlockQueue<_Ty>::BlockQueue(const int maxSize) : maxSz_(maxSize) {}

template<typename _Ty>
template<typename... _Valty>
void BlockQueue<_Ty>::Put(_Valty&&... _Val)
{
    PutInternalImpl(std::forward<_Valty>(_Val)...);
}

template<typename _Ty>
_Ty BlockQueue<_Ty>::Take()
{
    std::unique_lock<std::mutex> lock(mu_);
    condNonEmpty_.wait(lock, [&] {return !c_.empty(); });
    auto res = std::move(c_.front());
    c_.pop_front();
    condNonFull_.notify_all();
    return res;
}

template<typename _Ty>
bool BlockQueue<_Ty>::Offer(const _Ty& t)
{
    std::lock_guard<std::mutex> lock(mu_);
    if (maxSz_ != -1 && c_.size() >= maxSz_) {
        return false;
    }
    c_.push_back(t);
    condNonEmpty_.notify_all();
    return true;
}

template<typename _Ty>
bool BlockQueue<_Ty>::Offer(_Ty&& t)
{
    std::lock_guard<std::mutex> lock(mu_);
    if (maxSz_ != -1 && c_.size() >= maxSz_) {
        return false;
    }
    c_.push_back(std::move(t));
    condNonEmpty_.notify_all();
    return true;
}

template<typename _Ty>
bool BlockQueue<_Ty>::Offer(const _Ty& t, const std::chrono::milliseconds& time)
{
    std::unique_lock<std::mutex> lock(mu_);
    if (maxSz_ != -1) {
        bool result = condNonFull_.wait_for(lock, time,
            [&] { return c_.size() < maxSz_; });
        if (!result) {
            return false;
        }
    }
    c_.push_back(t);
    condNonEmpty_.notify_all();
    return true;
}

template<typename _Ty>
bool BlockQueue<_Ty>::Offer(_Ty&& t, const std::chrono::milliseconds& time)
{
    std::unique_lock<std::mutex> lock(mu_);
    if (maxSz_ != -1) {
        bool result = condNonFull_.wait_for(lock, time,
            [&] { return c_.size() < maxSz_; });
        if (!result) {
            return false;
        }
    }
    c_.push_back(std::move(t));
    condNonEmpty_.notify_all();
    return true;
}

template<typename _Ty>
bool BlockQueue<_Ty>::Poll(_Ty& t)
{
    std::lock_guard<std::mutex> lock(mu_);
    if (c_.empty()) {
        return false;
    }
    t = std::move(c_.front());
    c_.pop_front();
    condNonFull_.notify_all();
    return true;
}

template<typename _Ty>
bool BlockQueue<_Ty>::Poll(_Ty& t, const std::chrono::milliseconds& time)
{
    std::unique_lock<std::mutex> lock(mu_);
    bool result = condNonEmpty_.wait_for(lock, time,
        [&] {return !c_.empty(); });
    if (!result) {
        return false;
    }
    t = std::move(c_.front());
    c_.pop_front();
    condNonFull_.notify_all();
    return true;
}

template<typename _Ty>
bool BlockQueue<_Ty>::Empty() const
{
    std::lock_guard<std::mutex> lock(mu_);
    return c_.empty();
}

template<typename _Ty>
bool BlockQueue<_Ty>::Full() const
{
    if (-1 == maxSz_) {
        return false;
    }
    std::lock_guard<std::mutex> lock(mu_);
    return c_.size() >= maxSz_;
}

template<typename _Ty>
typename BlockQueue<_Ty>::SizeType BlockQueue<_Ty>::Size() const
{
    std::lock_guard<std::mutex> lock(mu_);
    return c_.size();
}

template<typename _Ty>
template<typename... _Valty>
void BlockQueue<_Ty>::PutInternalImpl(_Valty&&... _Val)
{
    std::unique_lock<std::mutex> lock(mu_);
    if (maxSz_ != -1) {
        condNonFull_.wait(lock, [this] { return c_.size() < maxSz_; });
    }
    c_.emplace_back(std::forward<_Valty>(_Val)...);
    condNonEmpty_.notify_all();
}

#endif // BLOCK_QUEUE_H

