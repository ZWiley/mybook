首先，介绍在多线程编程中不可缺少的同步措施--Mutex和Condition.

- Mutex

```c++
/***Mutex.h***/
class MutexLock : boost::noncopyable
{
 public:
  MutexLock()
    : holder_(0)
  {
    MCHECK(pthread_mutex_init(&mutex_, NULL));//MCHECK有什么作用？
  }

  ~MutexLock()
  {
    assert(holder_ == 0);
    MCHECK(pthread_mutex_destroy(&mutex_));
  }

  // must be called when locked, i.e. for assertion
  bool isLockedByThisThread() const//是否被当前线程锁住
  {
    return holder_ == CurrentThread::tid();//防止跨线程调用
  }

  void assertLocked() const
  {
    assert(isLockedByThisThread());
  }

  // internal usage

  void lock()
  {
    MCHECK(pthread_mutex_lock(&mutex_));//加锁
    assignHolder();//加锁时获得当前线程的线程号，即目前线程拥有这个锁
  }

  void unlock()
  {
    unassignHolder();//表示目前没有线程拥有这个锁
    MCHECK(pthread_mutex_unlock(&mutex_));//去锁
  }

  pthread_mutex_t* getPthreadMutex() /* non-const */
  {
    return &mutex_;
  }

 private:
  friend class Condition;//条件变量必须持有了锁之后才能使用

  class UnassignGuard : boost::noncopyable//这个内部类出现的莫名其妙
  {
   public:
    UnassignGuard(MutexLock& owner)
      : owner_(owner)
    {
      owner_.unassignHolder();
    }

    ~UnassignGuard()
    {
      owner_.assignHolder();
    }

   private:
    MutexLock& owner_;
  };

  void unassignHolder()
  {
    holder_ = 0;
  }

  void assignHolder()
  {
    holder_ = CurrentThread::tid();
  }

  pthread_mutex_t mutex_;
  pid_t holder_;
};

// Use as a stack variable, eg.
// int Foo::size() const
// {
//   MutexLockGuard lock(mutex_);
//   return data_.size();
// }
//该类负责管理互斥量的加锁和去锁
class MutexLockGuard : boost::noncopyable
{   
 public:
  explicit MutexLockGuard(MutexLock& mutex)
    : mutex_(mutex)
  {
    mutex_.lock();
  }

  ~MutexLockGuard()
  {
    mutex_.unlock();
  }

 private:

  MutexLock& mutex_;
};
```

有四种操作互斥锁的方式：创建，销毁，加锁，解锁。在muduo中，用一个低级的资源管理类MutexLock来实现这四种操作，再用一个较高级的资源管理类MutexLockGuard来管理MutexLock，即用RAII手法对资源进行两次封装，防止资源泄漏。
两个类都具有nocopy的属性，试想对Mutex的拷贝会在多线程程序中造成什么样的结果？有至少两个线程在同一时间拥有对一份资源的使用资格，后果不可设想。
在MutexLock中有一个好玩的私有变量：holder_. 该变量在一个线程对资源加锁时，将holder_设置为使用资源线程的索引；解锁时将holder_设置为0。初始化Mutex时将holder_设置为0；销毁时检查holder_是否为0。以上四个步骤保证了Mutex在某一个时间段内能被一个线程使用。
MutexLock与Condition是友元关系，具有很强的耦合度。

- Condition

```c++
/***Condition.h***/
class Condition : boost::noncopyable
{
 public:
  explicit Condition(MutexLock& mutex)
    : mutex_(mutex)
  {
    MCHECK(pthread_cond_init(&pcond_, NULL));
  }

  ~Condition()
  {
    MCHECK(pthread_cond_destroy(&pcond_));
  }

  void wait()
  {
    MutexLock::UnassignGuard ug(mutex_);
    MCHECK(pthread_cond_wait(&pcond_, mutex_.getPthreadMutex()));
  }

  // returns true if time out, false otherwise.
  bool waitForSeconds(double seconds);

  void notify()
  {
    MCHECK(pthread_cond_signal(&pcond_));
  }

  void notifyAll()
  {
    MCHECK(pthread_cond_broadcast(&pcond_));
  }

 private:
  MutexLock& mutex_;
  pthread_cond_t pcond_;
};
```

条件变量有五种操作方式：创建，销毁，等待，单一通知，全部通知。
在MutexLock中有一个内部类：UnassignGuard，该类的实例对象在Condition等待时创建，将holder_设置为0；当等待事件结束，又将holder_设置为原值。用MutexLock的析构函数检查等待事件是否发生在同一个线程中。
Condition类中有一个waitForSecond函数，用于实现pthread_cond_timewait的封装。
接下来，聊一聊主题--Thread。

- Thread

```c++
/***Thread.h***/
class Thread : boost::noncopyable   //禁止拷贝
{
 public:
  typedef boost::function<void ()> ThreadFunc;//仿函数对象,利用回调的方式使用线程函数

  explicit Thread(const ThreadFunc&, const string& name = string());//普通的线程构造函数
#ifdef __GXX_EXPERIMENTAL_CXX0X__
  explicit Thread(ThreadFunc&&, const string& name = string());//移动的线程构造函数，比上面的更节省资源std::move
#endif
  ~Thread();//析构函数

  void start();//启动线程
  int join(); // 类似于 pthread_join()

  bool started() const { return started_; }
  // pthread_t pthreadId() const { return pthreadId_; }
  pid_t tid() const { return *tid_; } //返回线程索引
  const string& name() const { return name_; }//返回线程名字

  static int numCreated() { return numCreated_.get(); }

 private:
  void setDefaultName();

  bool       started_;  //是否启动
  bool       joined_;   //是否终止
  pthread_t  pthreadId_;    //线程索引
  boost::shared_ptr<pid_t> tid_;    //持有一个强线程索引
  ThreadFunc func_;     //线程主体函数
  string     name_;     //线程名称标号

  static AtomicInt32 numCreated_;   //static变量在所有的线程对象中共享，为由该类产生线程排序
};
```

1. 在muduo的线程对象封装中，最精彩的是使用boost::function函数对象将线程函数以回调的方式传递进线程对象中。
   `typedef boost::function<void ()> ThreadFun;`
2. 在多线程情况下，避免在对象外操作指向对象的指针的情形，可以在一定程度上保证了线程安全。

```c++
/***Thread.cc***/
AtomicInt32 Thread::numCreated_;

//两种线程构造函数
//线程对象的可移动属性很有意思。
Thread::Thread(const ThreadFunc& func, const string& n)
  : started_(false),
    joined_(false),
    pthreadId_(0),
    tid_(new pid_t(0)),
    func_(func),
    name_(n)
{
  setDefaultName();
}

#ifdef __GXX_EXPERIMENTAL_CXX0X__
Thread::Thread(ThreadFunc&& func, const string& n)
  : started_(false),
    joined_(false),
    pthreadId_(0),
    tid_(new pid_t(0)),
    func_(std::move(func)),
    name_(n)
{
  setDefaultName();     
}

#endif

Thread::~Thread() 
{
  if (started_ && !joined_)     //将该线程设置为分离属性
  {
    pthread_detach(pthreadId_);     //线程结束将自动回收资源
  }
}

void Thread::setDefaultName()   //设置线程名字，比如Thread1,Thread2等
{
  int num = numCreated_.incrementAndGet();
  if (name_.empty())
  {
    char buf[32];   
    snprintf(buf, sizeof buf, "Thread%d", num);
    name_ = buf;
  }
}

void Thread::start()
{
  assert(!started_);    //断言线程是否已经开始运行
  started_ = true;      //断言失败则设置线程开始运行的标志
  // FIXME: move(func_)
  detail::ThreadData* data = new detail::ThreadData(func_, name_, tid_);   //获得线程运行的所需要的参数
  if (pthread_create(&pthreadId_, NULL, &detail::startThread, data))//线程开始运行并且线程的控制流停止再在此。
  {     //线程运行结束，线程自行运行结束并且自己做日志记录
    started_ = false;
    printf("blockDim.x: %d\n",blockDim.x);
    delete data; // or no delete?
    LOG_SYSFATAL << "Failed in pthread_create";
  }
}

int Thread::join()
{
  assert(started_);     //断言线程是否正在运行
  assert(!joined_);     //断言线程是否已经被终止
  joined_ = true;
  return pthread_join(pthreadId_, NULL);        //等待线程结束
}
```

在线程的析构函数中只设置线程的分离属性，即等待线程运行结束后自动回收线程资源，不强行终止线程。

```c++
struct ThreadData  //作为线程数据使用，将线程运行有关的数据保存到该结构体中，有点抽象回调的意思
{
  typedef muduo::Thread::ThreadFunc ThreadFunc;
  ThreadFunc func_;
  string name_;
  boost::weak_ptr<pid_t> wkTid_;    //使用弱引用保存线程标号

  ThreadData(const ThreadFunc& func,
             const string& name,
             const boost::shared_ptr<pid_t>& tid)
    : func_(func),
      name_(name),
      wkTid_(tid)
  { }

  void runInThread()    //核心函数
  {
    pid_t tid = muduo::CurrentThread::tid();    //得到当前的线程标志

    boost::shared_ptr<pid_t> ptid = wkTid_.lock();    //判断保存在ThreadData中的线程是否存在
    if (ptid)    //如果存在，ptid释放之前指向的线程标识
    {
      *ptid = tid;
      ptid.reset();
    }

    muduo::CurrentThread::t_threadName = name_.empty() ? "muduoThread" : name_.c_str(); //获得当前线程名称
    ::prctl(PR_SET_NAME, muduo::CurrentThread::t_threadName);
    try
    {
      func_(); //运行线程函数
      muduo::CurrentThread::t_threadName = "finished";
    }
    catch (const Exception& ex)    //异常捕捉部分
    {
      muduo::CurrentThread::t_threadName = "crashed";
      fprintf(stderr, "exception caught in Thread %s\n", name_.c_str());
      fprintf(stderr, "reason: %s\n", ex.what());
      fprintf(stderr, "stack trace: %s\n", ex.stackTrace());
      abort();
    }
    catch (const std::exception& ex)
    {
      muduo::CurrentThread::t_threadName = "crashed";
      fprintf(stderr, "exception caught in Thread %s\n", name_.c_str());
      fprintf(stderr, "reason: %s\n", ex.what());
      abort();
    }
    catch (...)
    {
      muduo::CurrentThread::t_threadName = "crashed";
      fprintf(stderr, "unknown exception caught in Thread %s\n", name_.c_str());
      throw; // rethrow
    }
  }
};

void* startThread(void* obj)    //这个函数最有意思
{
  ThreadData* data = static_cast<ThreadData*>(obj);    //强行转化为ThreadData
  data->runInThread();    //线程跑起来
  delete data;
  return NULL;
}
```

将线程中的若干数据保存到ThreadData中，然后将ThreadData作为传递给`pthread_create(...,void* arg)`中的最后一个数据参数传递给`void Thread(void* )`标准的线程启动函数。然后在标准的线程启动函数内将`void* arg`强行转化为ThreadData，然后使用ThreadData启动线程。
在使用muduo的接口时，使用bind将线程运行函数再打包，然后传递进Thread.
最后，向大家介绍muduo库中对于线程池的封装的理解。

1. 最重要的想法就是线程池将线程看为自己可执行的最小并且可随时增加的单位。
2. 整个线程池对象维持两个任务队列，threads_表示目前正在运行中的线程池，queue_表示位于存储队列中的等待线程。
3. thread_在运行的过程中使用while循环+条件变量判断当前的活动线程池中是否有空位，以及新的等待线程进入线程池。
4. 线程池从一开始就确定了自己将要运行的线程数目，不能在后面的运行中更改。

```c++
/***ThreadPool.h***/
class ThreadPool : boost::noncopyable
{
 public:
  typedef boost::function<void ()> Task;//将线程池中的线程作为可替换的任务，以线程为基本单位放在线程池中运行

  explicit ThreadPool(const string& nameArg = string("ThreadPool"));
  ~ThreadPool();

  // Must be called before start().
  // 设置线程池运行的最大的负载以及线程池中将要运行的线程
  void setMaxQueueSize(int maxSize) { maxQueueSize_ = maxSize; }//
  void setThreadInitCallback(const Task& cb)
  { threadInitCallback_ = cb; }

  void start(int numThreads);//启动一定数量的线程
  void stop();//线程池运算停止

  const string& name() const
  { return name_; }

  size_t queueSize() const;//返回正在排队等待的线程任务

  // Could block if maxQueueSize > 0
  void run(const Task& f);//将一个想要运行的线程放入线程池的任务队列
#ifdef __GXX_EXPERIMENTAL_CXX0X__
  void run(Task&& f);//C++11的移动方法，用于节省资源
#endif

 private:
  bool isFull() const;//判断线程队列是否已经满了
  void runInThread();//真正让线程跑起来的函数
  Task take();//获得任务队列的首个线程

  mutable MutexLock mutex_;//互斥锁
  Condition notEmpty_;//条件变量
  Condition notFull_;
  string name_;
  Task threadInitCallback_;//线程池中执行的线程对象
  boost::ptr_vector<muduo::Thread> threads_;//线程池
  std::deque<Task> queue_;//排队执行的线程对象队列
  size_t maxQueueSize_;//队列的最大数
  bool running_;//是否已经启动
};
```

1. 每一个加入线程池的线程都带有一个while循环，保证线程等待队列中的线程不会等待太久。即所有将加入线程池的线程都会进入线程等待队列接受检查。
2. start()：线程池启动函数保证在调用时启动一定数量的线程。
3. stop()：保证所有的正在运行的线程停止
4. queueSize()：返回此时线程等待队列中的个数，用于判断线程等待队列是否为空
5. run()：如果线程池为空，直接跑传入的线程。如果线程池等待队列满了，则当前控制流（线程）在notFull_上等待；否则将传入的线程加入线程等待队列，并且使用条件变量notEmpty_通知一条阻塞在该条件变量上的控制流（线程）。
6. take()：如果当前线程等待队列为空并且线程池正在跑，则控制流（线程）阻塞在notEmpty_条件变量上。当条件变量被激活时（有线程对象加入呆线程等待队列），判断是否可以从线程等待队列中拿出一个线程对象，如果可以，则将使用条件变量notFull_通知run()中阻塞在--想加入队列但是队列没有空余位置的变量上。
7. isFull()：返回在线程等待队列中的个数，用于判断是否可以将想要运行的线程放到线程等待队列中。
8. runInThread()：如果线程启动函数不为空，则在此将线程的控制流交给用于初始化线程池的线程对象。当此线程对象运行结束的时候，并且此时的线程池还在运行，则线程池离开初始化模式，进入线程池的循环线程补充模式。这种模式控制着线程池中的线程数量：当有新的线程对象进入线程池，则当前的线程控制流交给将要执行的线程对象。也就是说线程池中的线程对象要么主动结束自己的‘life’，然后由线程池的线程补充模式决定将要进入线程池运行的线程对象。然后在后面的take()中使用条件变量完成新的线程进入线程池的同步。

```c++
/***ThreadPool.cc***/
ThreadPool::ThreadPool(const string& nameArg)
  : mutex_(),
    notEmpty_(mutex_),
    notFull_(mutex_),
    name_(nameArg),
    maxQueueSize_(0),
    running_(false)
{
}

ThreadPool::~ThreadPool()
{
  if (running_)
  {
    stop();
  }
}

void ThreadPool::start(int numThreads)
{
  assert(threads_.empty());//首次启动，断言线程池为空
  running_ = true;
  threads_.reserve(numThreads);//预分配空间，且分配的空间不可变。
  for (int i = 0; i < numThreads; ++i)
  {
    char id[32];
    snprintf(id, sizeof id, "%d", i+1);
    threads_.push_back(new muduo::Thread(
          boost::bind(&ThreadPool::runInThread, this), name_+id));
    threads_[i].start();//直接启动线程
  }
  if (numThreads == 0 && threadInitCallback_)//只启动一条线程
  {
    threadInitCallback_();
  }
}

void ThreadPool::stop()
{
  {
  MutexLockGuard lock(mutex_);
  running_ = false;
  notEmpty_.notifyAll();
  }
  for_each(threads_.begin(),
           threads_.end(),
           boost::bind(&muduo::Thread::join, _1));
}

size_t ThreadPool::queueSize() const
{
  MutexLockGuard lock(mutex_);
  return queue_.size();
}

void ThreadPool::run(const Task& task)
{
  if (threads_.empty())//如果线程池为空，直接跑这条线程
  {
    task();
  }
  else
  {
    MutexLockGuard lock(mutex_);
    while (isFull())//如果线程池满了，在notfull条件变量上等待
    {
      notFull_.wait();
    }
    assert(!isFull());

    queue_.push_back(task);//现在线程池中有空位了
    notEmpty_.notify();//notempty条件变量通知信息
  }
}

#ifdef __GXX_EXPERIMENTAL_CXX0X__
void ThreadPool::run(Task&& task)
{
  if (threads_.empty())
  {
    task();
  }
  else
  {
    MutexLockGuard lock(mutex_);
    while (isFull())
    {
      notFull_.wait();
    }
    assert(!isFull());

    queue_.push_back(std::move(task));
    notEmpty_.notify();
  }
}
#endif

ThreadPool::Task ThreadPool::take()
{
  MutexLockGuard lock(mutex_);
  // always use a while-loop, due to spurious wakeup
  while (queue_.empty() && running_)//如果线程队列为空并且线程池正在跑
  {//在notempty条件变量上等待
    notEmpty_.wait();//当前线程停下来等待，当队列不为空了继续跑
  }//然后获得新任务
  Task task;//创建一个新的任务
  if (!queue_.empty())
  {
    task = queue_.front();//获得队列中的头任务
    queue_.pop_front();//弹出队列中的头任务
    if (maxQueueSize_ > 0)//如果队列最大长度大于0
    {
      notFull_.notify();//通知线程可以跑了
    }
  }
  return task;//返回任务
}

bool ThreadPool::isFull() const
{//用来判断线程队列是否已经
  mutex_.assertLocked();
  return maxQueueSize_ > 0 && queue_.size() >= maxQueueSize_;
}

void ThreadPool::runInThread()//生成一个threadFunc对象
{
  try
  {
    if (threadInitCallback_)//如果线程启动函数不为空，直接启动
    {
      threadInitCallback_();//此处开启新的线程，程序的运行流程在此停止；当线程运行完成则进入下面的while循环
    }
    while (running_)//该循环保证当上面的线程运行完成或者没有初始化线程，则进入线程池的循环模式
    {
      Task task(take());
      if (task)
      {
        task();
      }
    }
  }
  catch (const Exception& ex)   //异常捕捉过程
  {
    fprintf(stderr, "exception caught in ThreadPool %s\n", name_.c_str());
    fprintf(stderr, "reason: %s\n", ex.what());
    fprintf(stderr, "stack trace: %s\n", ex.stackTrace());
    abort();
  }
  catch (const std::exception& ex)
  {
    fprintf(stderr, "exception caught in ThreadPool %s\n", name_.c_str());
    fprintf(stderr, "reason: %s\n", ex.what());
    abort();
  }
  catch (...)
  {
    fprintf(stderr, "unknown exception caught in ThreadPool %s\n", name_.c_str());
    throw; // rethrow
  }
```