在muduo的定时器系统中，一共由四个类：Timestamp，Timer，TimeId，TimerQueue组成。其中最关键的是Timer和TimerQueue两个类。此文只解释初读时让人非常迷惑的TimerQueue类，这个类是整个定时器设施的核心，其他三个类简介其作用。
其中Timestamp是一个以int64_t表示的微秒级绝对时间，而Timer则表示一个定时器的到时事件，是否具有重复唤醒的时间等，TimerId表示在在TimerQueue中对Timer的索引。

#### TimerQueue

下面是muduo定时器中最重要的TimerQueue类，是整个定时器的核心，初读时让人非常迷惑，最主要的原因还是没有搞清楚Timer类中的成员的意思。

```c
/**Timer.h**/
 private:
  const TimerCallback callback_;//定时器回调函数
  Timestamp expiration_;//绝对的时间
  const double interval_;//如果有重复属性，超时的时间间隔
  const bool repeat_;//是否有重复
  const int64_t sequence_;//定时器序号

  static AtomicInt64 s_numCreated_;//定时器计数
```

有了上述成员的意义，我们便可以介绍TimerQueue的功能了。

```c
/**TimerQueue.h**/
class TimerQueue : boost::noncopyable
{
 public:
  TimerQueue(EventLoop* loop);
  ~TimerQueue();

  ///
  /// Schedules the callback to be run at given time,
  /// repeats if @c interval > 0.0.
  ///
  /// Must be thread safe. Usually be called from other threads.
  TimerId addTimer(const TimerCallback& cb,
                   Timestamp when,
                   double interval);//往定时器队列中添加定时器
#ifdef __GXX_EXPERIMENTAL_CXX0X__
  TimerId addTimer(TimerCallback&& cb,
                   Timestamp when,
                   double interval);
#endif

  void cancel(TimerId timerId);//取消某个定时器

 private:

  // FIXME: use unique_ptr<Timer> instead of raw pointers.
  typedef std::pair<Timestamp, Timer*> Entry;//到期的时间和指向其的定时器
  typedef std::set<Entry> TimerList;
  typedef std::pair<Timer*, int64_t> ActiveTimer;//定时器和其定时器的序列号
  typedef std::set<ActiveTimer> ActiveTimerSet;

  void addTimerInLoop(Timer* timer);
  void cancelInLoop(TimerId timerId);
  // called when timerfd alarms
  void handleRead();
  // move out all expired timers
  std::vector<Entry> getExpired(Timestamp now);//返回超时的定时器列表
  void reset(const std::vector<Entry>& expired, Timestamp now);

  bool insert(Timer* timer);//在两个序列中插入定时器

  EventLoop* loop_;
  const int timerfd_;//只有一个定时器，防止同时开启多个定时器，占用多余的文件描述符
  Channel timerfdChannel_;//定时器关心的channel对象
  // Timer list sorted by expiration
  TimerList timers_;//定时器集合（有序）

  // for cancel()
  // activeTimerSet和timer_保存的是相同的数据
  // timers_是按照到期的时间排序的，activeTimerSet_是按照对象地址排序
  ActiveTimerSet activeTimers_;//保存正在活动的定时器（无序）
  bool callingExpiredTimers_; /* atomic *///是否正在处理超时事件
  ActiveTimerSet cancelingTimers_;//保存的是取消的定时器（无序）
};
```

上述代码中有三处让人感到惊喜的地方：

- 首先，整个TimerQueue之打开一个timefd，用以观察定时器队列队首的到期事件。其原因是因为set容器是一个有序队列，以<排序，就是说整个队列中，Timer的到期时间时从小到大排列的，正是因为这样，才能做到节省系统资源的目的。
- 其次，在整个TimerQueue类中有三个容器，一个表示有序的Timer队列，一个表示正在活动的，无序的定时器队列（用于与有序的定时器队列同步），还有一个表示取消的定时器队列（在重新启动一个有固定时间间隔定时器时，首先判断是否友重复属性，其次就是是否在已经取消的队列中）。第二个定时器队列是否多余？还没有想明白。
- 最后，整个定时器队列采用了muduo典型的事件分发机制，可以使的定时器的到期时间像fd一样在Loop线程中处理。

```c
/**TimerQueue.cc**/
int createTimerfd()
{//创建非阻塞timefd
  int timerfd = ::timerfd_create(CLOCK_MONOTONIC,
                                 TFD_NONBLOCK | TFD_CLOEXEC);
  if (timerfd < 0)
  {
    LOG_SYSFATAL << "Failed in timerfd_create";
  }
  return timerfd;
}

struct timespec howMuchTimeFromNow(Timestamp when)
{//现在距离超时还有多久
  int64_t microseconds = when.microSecondsSinceEpoch()
                         - Timestamp::now().microSecondsSinceEpoch();
  if (microseconds < 100)
  {
    microseconds = 100;
  }
  struct timespec ts;
  ts.tv_sec = static_cast<time_t>(
      microseconds / Timestamp::kMicroSecondsPerSecond);
  ts.tv_nsec = static_cast<long>(
      (microseconds % Timestamp::kMicroSecondsPerSecond) * 1000);
  return ts;
}

void readTimerfd(int timerfd, Timestamp now)
{//处理超时时间，超时后，timefd变为可读,howmany表示超时的次数
  uint64_t howmany;//将事件读出来，免得陷入Loop忙碌状态
  ssize_t n = ::read(timerfd, &howmany, sizeof howmany);
  LOG_TRACE << "TimerQueue::handleRead() " << howmany << " at " << now.toString();
  if (n != sizeof howmany)
  {
    LOG_ERROR << "TimerQueue::handleRead() reads " << n << " bytes instead of 8";
  }
}

void resetTimerfd(int timerfd, Timestamp expiration)
{//重新设置定时器描述符关注的定时事件
  // wake up loop by timerfd_settime()
  struct itimerspec newValue;
  struct itimerspec oldValue;
  bzero(&newValue, sizeof newValue);
  bzero(&oldValue, sizeof oldValue);
  newValue.it_value = howMuchTimeFromNow(expiration);//获得与现在的时间差值，然后设置关注事件
  int ret = ::timerfd_settime(timerfd, 0, &newValue, &oldValue);
  if (ret)
  {
    LOG_SYSERR << "timerfd_settime()";
  }
}

}
}
}

using namespace muduo;
using namespace muduo::net;
using namespace muduo::net::detail;

TimerQueue::TimerQueue(EventLoop* loop)
  : loop_(loop),
    timerfd_(createTimerfd()),
    timerfdChannel_(loop, timerfd_),
    timers_(),
    callingExpiredTimers_(false)
{
  timerfdChannel_.setReadCallback(
      boost::bind(&TimerQueue::handleRead, this));
  // we are always reading the timerfd, we disarm it with timerfd_settime.
  timerfdChannel_.enableReading();//设置Channel的常规步骤
}

TimerQueue::~TimerQueue()
{
  timerfdChannel_.disableAll();//channel不再关注任何事件
  timerfdChannel_.remove();//在三角循环中删除此Channel
  ::close(timerfd_);
  // do not remove channel, since we're in EventLoop::dtor();
  for (TimerList::iterator it = timers_.begin();
      it != timers_.end(); ++it)
  {
    delete it->second;//释放timer对象
  }
}

TimerId TimerQueue::addTimer(const TimerCallback& cb,
                             Timestamp when,
                             double interval)
{//添加新的定时器
  Timer* timer = new Timer(cb, when, interval);
  loop_->runInLoop(
      boost::bind(&TimerQueue::addTimerInLoop, this, timer));
  return TimerId(timer, timer->sequence());
}

#ifdef __GXX_EXPERIMENTAL_CXX0X__
TimerId TimerQueue::addTimer(TimerCallback&& cb,
                             Timestamp when,
                             double interval)
{
  Timer* timer = new Timer(std::move(cb), when, interval);
  loop_->runInLoop(
      boost::bind(&TimerQueue::addTimerInLoop, this, timer));
  return TimerId(timer, timer->sequence());
}
#endif

void TimerQueue::cancel(TimerId timerId)
{//取消定时器
  loop_->runInLoop(
      boost::bind(&TimerQueue::cancelInLoop, this, timerId));
}

void TimerQueue::addTimerInLoop(Timer* timer)
{
  loop_->assertInLoopThread();
  bool earliestChanged = insert(timer);//是否将timer插入set的首部

  //如果插入首部，更新timrfd关注的到期时间
  if (earliestChanged)
  {
    resetTimerfd(timerfd_, timer->expiration());//启动定时器
  }
}

void TimerQueue::cancelInLoop(TimerId timerId)
{//取消要关注的重复事件
  loop_->assertInLoopThread();
  assert(timers_.size() == activeTimers_.size());
  ActiveTimer timer(timerId.timer_, timerId.sequence_);//获得索引
  ActiveTimerSet::iterator it = activeTimers_.find(timer);
  if (it != activeTimers_.end())
  {//删除Timers_和activeTimers_中的Timer
    size_t n = timers_.erase(Entry(it->first->expiration(), it->first));
    assert(n == 1); (void)n;
    delete it->first; // FIXME: no delete please
    activeTimers_.erase(it);//删除活动的timer
  }
  else if (callingExpiredTimers_)
  {//将删除的timer加入到取消的timer队列中
    cancelingTimers_.insert(timer);//取消的定时器与重新启动定时器有冲突
  }
  assert(timers_.size() == activeTimers_.size());
}

void TimerQueue::handleRead()
{
  loop_->assertInLoopThread();
  Timestamp now(Timestamp::now());
  readTimerfd(timerfd_, now);//读timerFd,防止一直出现可读事件，造成loop忙碌

  std::vector<Entry> expired = getExpired(now);//获得超时的定时器

  callingExpiredTimers_ = true;//将目前的状态调整为处理超时状态
  cancelingTimers_.clear();//将取消的定时器清理掉
  //更新完成马上就是重置，重置时依赖已经取消的定时器的条件，所以要将取消的定时器的队列清空
  // safe to callback outside critical section
  for (std::vector<Entry>::iterator it = expired.begin();
      it != expired.end(); ++it)//逐个调用超时的定时器的回调
  {
    it->second->run();
  }
  callingExpiredTimers_ = false;//退出处理超时定时器额状态

  reset(expired, now);//把具有重复属性的定时器重新加入定时器队列中
}

std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now)
{//获得当前已经超时的timer
  assert(timers_.size() == activeTimers_.size());
  std::vector<Entry> expired;//存储超时timer的队列
  Entry sentry(now, reinterpret_cast<Timer*>(UINTPTR_MAX));
  TimerList::iterator end = timers_.lower_bound(sentry);//返回的一个大于等于now的timer，小于now的都已经超时
  assert(end == timers_.end() || now < end->first);
  std::copy(timers_.begin(), end, back_inserter(expired));//将timer_的begin到上述获得end迭代器元素添加到expired的末尾
  timers_.erase(timers_.begin(), end);//在timer_中删除刚才被添加的元素

  for (std::vector<Entry>::iterator it = expired.begin();
      it != expired.end(); ++it)
  {//在Activetimer_的同步中删除timer
    ActiveTimer timer(it->second, it->second->sequence());
    size_t n = activeTimers_.erase(timer);
    assert(n == 1); (void)n;
  }

  assert(timers_.size() == activeTimers_.size());//再次将timer_和activetimer同步
  return expired;//返回超时的timerQueue
}

void TimerQueue::reset(const std::vector<Entry>& expired, Timestamp now)
{//将具有超时属性的定时器重新加入定时器队列
  Timestamp nextExpire;

  for (std::vector<Entry>::const_iterator it = expired.begin();
      it != expired.end(); ++it)
  {
    ActiveTimer timer(it->second, it->second->sequence());
    if (it->second->repeat()
        && cancelingTimers_.find(timer) == cancelingTimers_.end())
    {//判断是否具有重复属性并且不在取消的定时器队列中
      it->second->restart(now);//重新设置定时器的到期时间，并且将重新设置后的定时器插入timer_和activeTimer_中
      insert(it->second);
    }
    else
    {
      // FIXME move to a free list
      delete it->second; // FIXME: no delete please
    }
  }

  if (!timers_.empty())
  {//如果目前的队列不为空，获得目前队首的到期时间
    nextExpire = timers_.begin()->second->expiration();
  }

  if (nextExpire.valid())
  {//如果到期时间不为0,重新设置timerfd应该关注的时间
    resetTimerfd(timerfd_, nextExpire);
  }
}

bool TimerQueue::insert(Timer* timer)
{//将Timer插入到两个同步的TimeQueue中，最关键的一个函数
  loop_->assertInLoopThread();
  assert(timers_.size() == activeTimers_.size());//判断两个Timer队列的同步bool earliestChanged = false;
  Timestamp when = timer->expiration();//获得Timer的事件
  TimerList::iterator it = timers_.begin();//得到Timer的begin
  if (it == timers_.end() || when < it->first)
  {//判断是否要将这个timer插入队首，如果是，更新timefd关注的到期事件
    earliestChanged = true;
  }

  {//将Timer中按顺序插入timer_，set是有序集合，默认关键字<排列
    std::pair<TimerList::iterator, bool> result
      = timers_.insert(Entry(when, timer));
    assert(result.second); (void)result;
  }

  {//随意插入进入activeTimer_
    std::pair<ActiveTimerSet::iterator, bool> result
      = activeTimers_.insert(ActiveTimer(timer, timer->sequence()));
    assert(result.second); (void)result;
  }

  assert(timers_.size() == activeTimers_.size());//再次同步两个Timer
  return earliestChanged;
}
```