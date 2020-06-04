## 模拟单线程情况下muduo库的工作情况

muduo的源代码对于一个初学者来说还是有一些复杂的，其中有很多的回调函数以及交叉的组件，下面我将追踪一次TCP连接过程中发生的事情，不会出现用户态的源码，都是库内部的运行机制。下文笔者将描述一次连接发生的过程，将Channel到加入到loop循环为止。

### 监听套接字加入loop循环的完整过程

- 首先创建一个TcpServer对象，在的创建过程中，首先new出来自己的核心组件（Acceptor,loop,connectionMap,threadPool）之后TcpServer会向Acceptor注册一个新连接到来时的Connection回调函数。loop是由用户提供的，并且在最后向Acceptor注册一个回调对象，用于处理：一个新的Client连接到来时该怎么处理。
  TcpServer向Acceptor注册的回调代码主要作用是：当一个新的连接到来时，根据Acceptor创建的可连接描述符和客户的地址，创建一个Connection对象，并且将这个对象加入到TcpServer的ConnectionMap中，由TcpServer来管理上述新建con对象。但是现在监听套接字的事件分发对象Channel还没有加入loop，就先不多提这个新的连接到到来时的处理过程。

```c++
TcpServer::TcpServer(EventLoop* loop,const InetAddress& listenAddr,const string& nameArg,Option option)
    : loop_(CHECK_NOTNULL(loop)),
      ipPort_(listenAddr.toIpPort()),name_(nameArg),acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)),
    threadPool_(new EventLoopThreadPool(loop, name_)),
    connectionCallback_(defaultConnectionCallback),
    messageCallback_(defaultMessageCallback),
    nextConnId_(1)
{//上面的loop是用户提供的loop
  acceptor_->setNewConnectionCallback(
      boost::bind(&TcpServer::newConnection, this, _1, _2));//注册给acceptor的回调
}//将在Acceptor接受新连接的时候

void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr)
{//将本函数注册个acceptor
  loop_->assertInLoopThread();//断言是否在IO线程
  EventLoop* ioLoop = threadPool_->getNextLoop();//获得线程池中的一个loop
  char buf[64];//获得线程池map中的string索引
  snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
  ++nextConnId_;
  string connName = name_ + buf;

  LOG_INFO << "TcpServer::newConnection [" << name_
           << "] - new connection [" << connName
           << "] from " << peerAddr.toIpPort();
  InetAddress localAddr(sockets::getLocalAddr(sockfd));//获得本地的地址，用于构建Connection
  // FIXME poll with zero timeout to double confirm the new connection
  // FIXME use make_shared if necessary
  TcpConnectionPtr conn(new TcpConnection(ioLoop,
                                          connName,
                                          sockfd,
                                          localAddr,
                                          peerAddr));//构建了一个connection
  connections_[connName] = conn;//将新构建的con加入server的map中
  conn->setConnectionCallback(connectionCallback_);//muduo默认的
  conn->setMessageCallback(messageCallback_);//moduo默认的
  conn->setWriteCompleteCallback(writeCompleteCallback_);//？？
  conn->setCloseCallback(
      boost::bind(&TcpServer::removeConnection, this, _1)); // FIXME: unsafe
  ioLoop->runInLoop(boost::bind(&TcpConnection::connectEstablished, conn));//在某个线程池的loop中加入这个con
}
```

- 下面接着讲述在TcpServer的构造过程中发生的事情：创建Acceptor对象。TcpServer用unique_ptr持有唯一的指向Acceptor的指针。Acceptor的构造函数完成了一些常见的选项。最后的一个向Acceptor->Channel注册一个回调函数，用于处理：listening可读时（新的连接到来），该怎么办？答案是：当新的连接到来时，创建一个已连接描述符，然后调用TcpServe注册给Acceptor的回调函数，用于处理新的连接。

```c++
Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport)
  : loop_(loop),
    acceptSocket_(sockets::createNonblockingOrDie(listenAddr.family())),
    acceptChannel_(loop, acceptSocket_.fd()),
    listenning_(false),
    idleFd_(::open("/dev/null", O_RDONLY | O_CLOEXEC))
{
  assert(idleFd_ >= 0);
  acceptSocket_.setReuseAddr(true);
  acceptSocket_.setReusePort(reuseport);
  acceptSocket_.bindAddress(listenAddr);
  acceptChannel_.setReadCallback(
      boost::bind(&Acceptor::handleRead, this));//Channel设置回调，当sockfd可读时掉用设置的回调
}

void Acceptor::handleRead()
{
  loop_->assertInLoopThread();//判断是否在IO线程
  InetAddress peerAddr;//客户的地址
  //FIXME loop until no more
  int connfd = acceptSocket_.accept(&peerAddr);//获得连接的描述符
  if (connfd >= 0)
  {
    // string hostport = peerAddr.toIpPort();
    // LOG_TRACE << "Accepts of " << hostport;
    if (newConnectionCallback_)
    {
      newConnectionCallback_(connfd, peerAddr);//TcpServer注册的，创建新的con,并且加入TcpServer的ConnectionMap中。
    }
    else
    {
      sockets::close(connfd);
    }
  }
  else
  {
    LOG_SYSERR << "in Acceptor::handleRead";
    // Read the section named "The special problem of
    // accept()ing when you can't" in libev's doc.
    // By Marc Lehmann, author of libev.
    if (errno == EMFILE)
    {
      ::close(idleFd_);
      idleFd_ = ::accept(acceptSocket_.fd(), NULL, NULL);
      ::close(idleFd_);
      idleFd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
    }
  }
}
```

- 在上述Acceptor对象的创建过程中，Acceptor会创建一个用于处理监听套接字事件的Channel对象，以下Acceptor的Channel对象的创造过程，很常规的处理过程。

```c++
Channel::Channel(EventLoop* loop, int fd__)
  : loop_(loop),
    fd_(fd__),
    events_(0),
    revents_(0),
    index_(-1),
    logHup_(true),
    tied_(false),
    eventHandling_(false),
    addedToLoop_(false)
{
}
```

- 到此，在muduo库内部的初始化过程已经基本处理完毕，然后由用户调用TcpServer的setThreadNum()和start()函数。在start()函数中会将打开Acceptor对象linten套接字。

```c++
void TcpServer::setThreadNum(int numThreads)
{//设置线程池的开始数目
  assert(0 <= numThreads);
  threadPool_->setThreadNum(numThreads);
}

void TcpServer::start()
{//TcpServer开始工作
  if (started_.getAndSet(1) == 0)//获得原子计数
  {
    threadPool_->start(threadInitCallback_);//线程池开始工作

    assert(!acceptor_->listenning());//打开accepor的监听状态
    loop_->runInLoop(
        boost::bind(&Acceptor::listen, get_pointer(acceptor_)));//打开acceptor的listening
  }
}
```

- 打开Acceptor对象的listenfd的详细过程。

```c++
void Acceptor::listen()
{
  loop_->assertInLoopThread();//判断是否在IO线程
  listenning_ = true;//进入监听模式
  acceptSocket_.listen();
  acceptChannel_.enableReading();//让监听字的channel关注可读事件
}
```

- 接着使用了Channel对象中的的enableReading()函数，让这个Channel对象关注可读事件。关键在于更新过程，应该是这个流程中最重要的操作。

```c++
void enableReading() { events_ |= kReadEvent; update(); }//将关注的事件变为可读，然后更新
```

- 使用了Channel的更新函数：update()

```c++
void Channel::update()
{
  addedToLoop_ = true;//更新channel的状态
  loop_->updateChannel(this);//调用POLLER的更新功能
}
```

- EventLoop持有唯一的Poller，也就是说，这个Poller将负责最后的更新过程。如果是新的Channel对象，则在Poller的pollfd数组中增加席位；如果不是新的Channel对象，则更新它目前所发生的事件（将目前发生的事件设置为0）。

```c++
void EventLoop::updateChannel(Channel* channel)
{
  assert(channel->ownerLoop() == this);//判断channel的LOOP是否是当前的LOOP
  assertInLoopThread();//判断是否在IO线程
  poller_->updateChannel(channel);//使用POLLER来更新channel
}
```

- 紧接着使用了Poller的updateChannel函数

```c++
void PollPoller::updateChannel(Channel* channel)
{//将channel关注的事件与pollfd同步
  Poller::assertInLoopThread();//如果不再loop线程直接退出
  LOG_TRACE << "fd = " << channel->fd() << " events = " << channel->events();
  if (channel->index() < 0)//获得channel在map中的位置
  {
    // a new one, add to pollfds_
    assert(channels_.find(channel->fd()) == channels_.end());
    struct pollfd pfd;//新建一个pfd与channel相关联
    pfd.fd = channel->fd();
    pfd.events = static_cast<short>(channel->events());//关注的事件设置为channel关注的事件
    pfd.revents = 0;//正在发生的事件为0
    pollfds_.push_back(pfd);//将设置好的pollfd加入关注事件列表
    int idx = static_cast<int>(pollfds_.size())-1;//并且获得加入的位置
    channel->set_index(idx);//channel保存自己在pollfds中的位置
    channels_[pfd.fd] = channel;//channel将自己加入到channelmap中
  }
  else
  {
    // update existing one
    assert(channels_.find(channel->fd()) != channels_.end());
    assert(channels_[channel->fd()] == channel);//判断位置是否正确
    int idx = channel->index();//获得channel在pollfd中的索引
    assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));
    struct pollfd& pfd = pollfds_[idx];//获得索引
    assert(pfd.fd == channel->fd() || pfd.fd == -channel->fd()-1);
    pfd.events = static_cast<short>(channel->events());//修改关注的事件
    pfd.revents = 0;//将当前发生的事件设置为0
    if (channel->isNoneEvent())//如果channel没有任何事件，一个暂时熄火的channel
    {
      // ignore this pollfd
      pfd.fd = -channel->fd()-1;//将索引设置为原来索引的负数
    }
  }
}
```

- 至此，调用EventLoop的loop函数，进行loop循环，开始处理事件。

```c++
void EventLoop::loop()
{
  assert(!looping_);//判断是否在LOOPING
  assertInLoopThread();//判断这个函数在LOOP线程调用
  looping_ = true;//进入LOOPING状态
  quit_ = false;  // FIXME: what if someone calls quit() before loop() ?
  LOG_TRACE << "EventLoop " << this << " start looping";

  while (!quit_)
  {
    activeChannels_.clear();//将活动线程队列置空
    pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);//获得活动文件描述符的数量，并且获得活动的channel队列
    ++iteration_;//增加Poll次数
    if (Logger::logLevel() <= Logger::TRACE)
    {
      printActiveChannels();
    }
    // TODO sort channel by priority
    eventHandling_ = true;//事件处理状态
    for (ChannelList::iterator it = activeChannels_.begin();
        it != activeChannels_.end(); ++it)
    {
      currentActiveChannel_ = *it;//获得当前活动的事件
      currentActiveChannel_->handleEvent(pollReturnTime_);//处理事件，传递一个poll的阻塞时间
    }
    currentActiveChannel_ = NULL;//将当前活动事件置为空
    eventHandling_ = false;//退出事件处理状态
    doPendingFunctors();//处理用户在其他线程注册给IO线程的事件
  }

  LOG_TRACE << "EventLoop " << this << " stop looping";
  looping_ = false;//推出LOOPING状态
}
```

一个监听套接字已经进入循环，如果此时一个新的连接到来又会发生什么事情呢？

### 一个新连接到达时的处理过程。

- 此时在loop循环中的监听套接字变得可读，然后便调用一个可读事件的处理对象。首先调用Acceptor注册的handleRead对象，完成连接套接字的创建，其次在handleRead对象的内部调用TcpServer注册给Acceptor的函数对象，用于将新建con对象加入TcpServer的ConnectionMap中去。

```c++
void Channel::handleEvent(Timestamp receiveTime)
{
  boost::shared_ptr<void> guard;
  if (tied_)
  {
    guard = tie_.lock();//提升成功说明con存在
    if (guard)//这样做比较保险
    {
      handleEventWithGuard(receiveTime);
    }
  }
  else
  {
    handleEventWithGuard(receiveTime);
  }
}

void Channel::handleEventWithGuard(Timestamp receiveTime)
{//真正的处理各种事件
  eventHandling_ = true;//处理事件状态
  LOG_TRACE << reventsToString();
  if ((revents_ & POLLHUP) && !(revents_ & POLLIN))
  {
    if (logHup_)
    {
      LOG_WARN << "fd = " << fd_ << " Channel::handle_event() POLLHUP";
    }
    if (closeCallback_) closeCallback_();
  }

  if (revents_ & POLLNVAL)
  {
    LOG_WARN << "fd = " << fd_ << " Channel::handle_event() POLLNVAL";
  }

  if (revents_ & (POLLERR | POLLNVAL))
  {
    if (errorCallback_) errorCallback_();
  }
  if (revents_ & (POLLIN | POLLPRI | POLLRDHUP))
  {
    if (readCallback_) readCallback_(receiveTime);
  }
  if (revents_ & POLLOUT)
  {
    if (writeCallback_) writeCallback_();
  }
  eventHandling_ = false;
}
```

- 此时，监听套接字处理的时可读事件，调用之前由Acceptor注册的handleRead回调函数

```c++
void Acceptor::handleRead()
{
  loop_->assertInLoopThread();//判断是否在IO线程
  InetAddress peerAddr;//客户的地址
  //FIXME loop until no more
  int connfd = acceptSocket_.accept(&peerAddr);//获得连接的描述符
  if (connfd >= 0)
  {
    // string hostport = peerAddr.toIpPort();
    // LOG_TRACE << "Accepts of " << hostport;
    if (newConnectionCallback_)
    {
      newConnectionCallback_(connfd, peerAddr);//这是个关键步骤，重点在于这个回调是谁注册的
    }
    else
    {
      sockets::close(connfd);
    }
  }
  else
  {
    LOG_SYSERR << "in Acceptor::handleRead";
    // Read the section named "The special problem of
    // accept()ing when you can't" in libev's doc.
    // By Marc Lehmann, author of libev.
    if (errno == EMFILE)
    {
      ::close(idleFd_);
      idleFd_ = ::accept(acceptSocket_.fd(), NULL, NULL);
      ::close(idleFd_);
      idleFd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
    }
  }
}
```

- 在上述函数中又调用，由TcpServer注册给Acceptor的回调函数

```c++
void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr)
{//将本函数注册个acceptor
  loop_->assertInLoopThread();//断言是否在IO线程
  EventLoop* ioLoop = threadPool_->getNextLoop();//获得线程池中的一个loop
  char buf[64];//获得线程池map中的string索引
  snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
  ++nextConnId_;
  string connName = name_ + buf;

  LOG_INFO << "TcpServer::newConnection [" << name_
           << "] - new connection [" << connName
           << "] from " << peerAddr.toIpPort();
  InetAddress localAddr(sockets::getLocalAddr(sockfd));//获得本地的地址，用于构建Connection
  // FIXME poll with zero timeout to double confirm the new connection
  // FIXME use make_shared if necessary
  TcpConnectionPtr conn(new TcpConnection(ioLoop,
                                          connName,
                                          sockfd,
                                          localAddr,
                                          peerAddr));//构建了一个connection
connections_[connName] = conn;//将新构建的con加入server的map中
  conn->setConnectionCallback(connectionCallback_);//muduo默认的
  conn->setMessageCallback(messageCallback_);//moduo默认的
  conn->setWriteCompleteCallback(writeCompleteCallback_);//？？
  conn->setCloseCallback(
      boost::bind(&TcpServer::removeConnection, this, _1)); // FIXME: unsafe
  ioLoop->runInLoop(boost::bind(&TcpConnection::connectEstablished, conn));//在某个线程池的loop中加入这个con
}
```

- 上述对象的最后一行，是调用新建的TcpConnection对象的函数，用设置新建的con对象中的channel的关注事件。

```c++
void TcpConnection::connectEstablished()
{//建立连接
  loop_->assertInLoopThread();//断言是否在IO线程
  assert(state_ == kConnecting);//正处于连接建立过程
  setState(kConnected);
  channel_->tie(shared_from_this());//使channel的tie的指向不为空
  channel_->enableReading();//将connection设置为可读的

  connectionCallback_(shared_from_this());//用户提供的回调函数，muduo有提供默认的
}
```

- 至此以后的过程与将listen->channel添加到loop中的过程一样。

```c++
void enableReading() { events_ |= kReadEvent; update(); }//将关注的事件变为可读，然后更新
```

- 使用了Channel的更新函数：update()

```c++
void Channel::update()
{
  addedToLoop_ = true;//更新channel的状态
  loop_->updateChannel(this);//调用POLLER的更新功能
}
```

- 使用了EventLoop的updateChannel()功能

```c++
void EventLoop::updateChannel(Channel* channel)
{
  assert(channel->ownerLoop() == this);//判断channel的LOOP是否是当前的LOOP
  assertInLoopThread();//判断是否在IO线程
  poller_->updateChannel(channel);//使用POLLER来更新channel
}
```

- 在poller中更新channel

```c++
void PollPoller::updateChannel(Channel* channel)
{//将channel关注的事件与pollfd同步
  Poller::assertInLoopThread();//如果不再loop线程直接退出
  LOG_TRACE << "fd = " << channel->fd() << " events = " << channel->events();
  if (channel->index() < 0)//获得channel在map中的位置
  {
    // a new one, add to pollfds_
    assert(channels_.find(channel->fd()) == channels_.end());
    struct pollfd pfd;//新建一个pfd与channel相关联
    pfd.fd = channel->fd();
    pfd.events = static_cast<short>(channel->events());//关注的事件设置为channel关注的事件
    pfd.revents = 0;//正在发生的事件为0
    pollfds_.push_back(pfd);//将设置好的pollfd加入关注事件列表
    int idx = static_cast<int>(pollfds_.size())-1;//并且获得加入的位置
    channel->set_index(idx);//channel保存自己在pollfds中的位置
    channels_[pfd.fd] = channel;//channel将自己加入到channelmap中
  }
  else
  {
    // update existing one
    assert(channels_.find(channel->fd()) != channels_.end());
    assert(channels_[channel->fd()] == channel);//判断位置是否正确
    int idx = channel->index();//获得channel在pollfd中的索引
    assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));
    struct pollfd& pfd = pollfds_[idx];//获得索引
    assert(pfd.fd == channel->fd() || pfd.fd == -channel->fd()-1);
    pfd.events = static_cast<short>(channel->events());//修改关注的事件
    pfd.revents = 0;//将当前发生的事件设置为0
    if (channel->isNoneEvent())//如果channel没有任何事件，一个暂时熄火的channel
    {
      // ignore this pollfd
      pfd.fd = -channel->fd()-1;//将索引设置为原来索引的负数
    }
  }
}
```

最后一个连接的channel加入loop循环，新的循环已经开始了。

## 模拟单线程情况下muduo库的工作情况

在上篇中，笔者追踪了Connetfd（连接套接字）和Listenfd（监听套接字）的Channel对象加入到loop循环的过程。其中包括了网络连接过程中，muduo会创建的对象。本文将会追踪Connetfd（连接套接字）和Listenfd（监听套接字）从loop循环退出并且销毁，一直到main函数终止的过程。

### 连接套接字正常情况下完整的销毁情况（read == 0）

由TcpConnection对象向自己所拥有的Channel对象注册的可读事件结束时，会出现`read == 0`的情况，此时会直接调用TcpConnection对象的handleClose函数。因为在向Channel对象注册可读事件时，使用了如下的语句：

```c++
channel_->setReadCallback(&TcpConnection::handleRead,this);
```

**this**使得Channel对象可以直接在TcpConnection向它注册的handleClose函数内部使用TcpConnetion的函数。

```c++
void TcpConnection::handleRead(Timestamp receiveTime)
{//都是向channel注册的函数
  loop_->assertInLoopThread();//断言在loop线程
  int savedErrno = 0;//在读取数据之后调用用户提供的回调函数
  ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
  if (n > 0)
  {//这个应该时用户提供的处理信息的回调函数
    messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
  }
  else if (n == 0)
  {//读到了0,直接关闭
    handleClose();
  }
  else
  {//如果有错误
    errno = savedErrno;
    LOG_SYSERR << "TcpConnection::handleRead";
    handleError();//处理关闭
  }
}
void TcpConnection::handleClose()
{//处理关闭事件
  loop_->assertInLoopThread();//断言是否在loop线程
  LOG_TRACE << "fd = " << channel_->fd() << " state = " << stateToString();
  assert(state_ == kConnected || state_ == kDisconnecting);
  // we don't close fd, leave it to dtor, so we can find leaks easily.
  setState(kDisconnected);//设置关闭状态
  channel_->disableAll();//不再关注任何事情

  TcpConnectionPtr guardThis(shared_from_this());//获得shared_ptr交由tcpsever处理
  connectionCallback_(guardThis);//这他妈就是记录一点日志
  // must be the last line
  closeCallback_(guardThis);
}
```

在以上的handleClose代码中，首先会设置TcpConnection对象的关闭状态，其次让自己Channel对象不再关注任何事情。
因为TcpConnection在创建时使用了如下语句：

```c++
class TcpConnection : boost::noncopyable, public boost::enable_shared_from_this<TcpConnection>
```

便可以使用shared_fron_this()获得指向本TcpConnection对象的shared_ptr指针，然后在后续的过程中，对指向本对象的
shared_ptr进行操作，则可以安全的将本对象从其他依赖类中安全的移除。
继续跟踪上述的最后一句，且closeCallback是由TcpServer在创建TcpConnection对象时向它注册的：

```c++
conn->setCloseCallback(boost::bind(&TcpServer::removeConnection, this, _1));
```

目的在于在TcpServer的TcpconnectionMap中移除指向指向这个TcpConnection对象的指针。

```c++
void TcpServer::removeConnection(const TcpConnectionPtr& conn)
{
  // FIXME: unsafe
  loop_->runInLoop(boost::bind(&TcpServer::removeConnectionInLoop, this, conn));//注册到loop线程中移除这个con
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn)
{
  loop_->assertInLoopThread();//断言是否在IO线程
  LOG_INFO << "TcpServer::removeConnectionInLoop [" << name_
           << "] - connection " << conn->name();
  size_t n = connections_.erase(conn->name());//删除该con
  (void)n;
  assert(n == 1);
  EventLoop* ioLoop = conn->getLoop();//获得线程Loop
  ioLoop->queueInLoop(
      boost::bind(&TcpConnection::connectDestroyed, conn));//将线程销毁动作添加到loop中去
}
```

目前的步骤还在于处理TcpConnection对象。

```c++
void TcpConnection::connectDestroyed()
{//销毁连接
  loop_->assertInLoopThread();//断言是否在loop线程
  if (state_ == kConnected)//如果此时处于连接状态
  {
    setState(kDisconnected);//将状态设置为不可连接状态
    channel_->disableAll();//channel不再关注任何事件

    connectionCallback_(shared_from_this());//记录作用，好坑的一个作用
  }
  channel_->remove();//在poller中移除channel
}
```

TcpConnection对象的声明周期随着将Channel对象移除出loop循环而结束。

```c++
void Channel::remove()
{//将channel从loop中移除
  assert(isNoneEvent());//判断此时的channel是否没有事件发生
  addedToLoop_ = false;//此时没有loop拥有此channel
  loop_->removeChannel(this);//调用POLLER的删除功能
}
```

因为EventLoop对象中的poller对象也持有Channel对象的指针，所以需要将channel对象安全的从poller对象中移除。

```c++
void EventLoop::removeChannel(Channel* channel)
{//每次间接的调用的作用就是将需要改动的东西与当前调用的类撇清关系
  assert(channel->ownerLoop() == this);
  assertInLoopThread();//如果没有在loop线程调用直接退出
  if (eventHandling_)//判断是否在事件处理状态。判断当前是否在处理这个将要删除的事件以及活动的事件表中是否有这个事件
  {
    assert(currentActiveChannel_ == channel ||
        std::find(activeChannels_.begin(), activeChannels_.end(), channel) == activeChannels_.end());
  }
  poller_->removeChannel(channel);//在POLLER中删除这个事件分发表
}
```

以下时Poller对象移除Channel对象的具体操作步骤。

```c++
void PollPoller::removeChannel(Channel* channel)
{
  Poller::assertInLoopThread();//判断是否在IO线程
  LOG_TRACE << "fd = " << channel->fd();
  assert(channels_.find(channel->fd()) != channels_.end());
  assert(channels_[channel->fd()] == channel);
  assert(channel->isNoneEvent());
  int idx = channel->index();//获得pfd位置的索引
  assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));
  const struct pollfd& pfd = pollfds_[idx]; (void)pfd;//获得pfd
  assert(pfd.fd == -channel->fd()-1 && pfd.events == channel->events());
  size_t n = channels_.erase(channel->fd());//在Map中删除channel
  assert(n == 1); (void)n;//准备删除pollfd中的关注事件
  if (implicit_cast<size_t>(idx) == pollfds_.size()-1)//获得pollfd的索引
  {
    pollfds_.pop_back();
  }
  else//想方设法的删除pollfd中与channel相关的pfd
  {
    int channelAtEnd = pollfds_.back().fd;
    iter_swap(pollfds_.begin()+idx, pollfds_.end()-1);
    if (channelAtEnd < 0)
    {
      channelAtEnd = -channelAtEnd-1;
    }
    channels_[channelAtEnd]->set_index(idx);
    pollfds_.pop_back();
  }
}
```

以上便是一个连接的销毁过程，现在依然让人迷惑的时Channel对象到底被谁持有过？以及TcpConnection对象的生命周期到底在什么时候结束？

### Channel与TcpConnection对象的创建与销毁

#### 创建

下面，在让我们进入上一篇文章，具体的看看Channel对象的生命期到底是个什么样子？

- 当新连接过来时，由TcpServer创建一个TcpConnection对象，这个对象中包括一个与此连接相关的Channel对象。
- 然后紧接着TcpServer使用创建的TcpConnection对象向Loop中注册事件。此时的控制权回到TcpConnection对象手中。它操作自己的Channel对象更新EventLoop。
- 最后由EventLoop对象去操作自己的Poller更新Poller的Channel队列。
  在上述过程中，Channel对象的创建操作有这样的顺序：

```c++
TcpServer->TcpConnection->Channel->EventLoop->Poller
```

TcpConnection对象的创建过程相比于Channel简单的多：

```c++
TcpServer->TcpConnection
```

在TcpServer中创建Connection对象，然后让TcpConnection对象去操作自己的Channel对象，将Channel加入到EventLoop中去，最后由EventLoop操作自己的Poller收尾。总而言之，Channel对象在整个过程中只由Poller和TcpConnection对象持有，销毁时也应该是如此过程。

#### 销毁

由于Channel是TcpConnection对象的一部分，所以Channel的生命周期一定会比TcpConnection的短。
Channel与TcpConnection对象的销毁基本与上述创建过程相同：

```c++
TcpConnection->TcpServer->Channel->EventLoop->Poller
```

随着，Channel对象从Poller中移除，TcpConnection的生命周期也随之结束。
TcpConnection对象在整个生命周期中只由TcpServer持有，但是TcpConnection对象中的Channel又由Poller持有，Poller又是EventLoop的唯一成员，所以造成了如此麻烦的清理与创建过程。那如果能将Channel移出TcpConnection对象，那muduo的创建与清理工作会不会轻松很多？