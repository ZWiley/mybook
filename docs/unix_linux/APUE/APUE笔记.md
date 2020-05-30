### fork, vfork

1. *fork*  

   + 用于创建子进程。

   + 在调用时，返回两次：子进程的返回值是0，父进程的返回值的新建子进程的ID。

   + 子进程是父进程的副本。子进程和父进程继续执行 *fork* 之后的指令。

     + 子进程获得父进程的 **数据空间、堆、栈的副本**
     + 共享的是：**文件描述符、mmap建立的映射区** 
     + 子进程和父进程共享的是 **代码段**，*fork* 之后各自执行。
     + 父进程和子进程的执行顺序谁先谁后是未知的，是竞争的关系。

   + *COW*  
     *COW* 即写时复制(*Copy-On-Write*)， **数据空间、堆、栈的副本**在创建子进程时并不创建副本。而是在父进程或者子进程修改这片区域时，内核为修改区域的那块内存制作一个副本，以提高效率。  

   + *fork*   
     *fork* 失败的原因：  

     + 系统中已经有太多的进程  
     + 该实际用户Id的进程数超过了系统限制

   + 案例  

     ```c++
     int globvar = 10;
     char buf[] = "a writte to stdout.\n";
     
     int main(int argv, char* argc[]){
         int var;
         pid_t pid;
     
         var = 88;
         if(write(STDOUT_FILENO, buf, sizeof(buf)-1) != sizeof(buf)-1){
             printf("write error");
             exit(1);
         }
         printf("before fork.\n");
         // 创建子进程后，后面的代码，父进程和子进程独立运行。
         if((pid=fork()) < 0){
             printf("fork() error.\n");
             exit(1);
         }
         else if(pid == 0){
             globvar++; //子进程运行不改变父进程的值
             var++;
         }
         else
             sleep(2);
         printf("pid=%ld, globvar=%d, var=%d.\n",(long)getpid() , globvar, var);
         retdurn 0;
     }
     ```
     
     
   
2. *vfork*   

   + 与 *fork* 一样都创建新的进程，他的是目的是执行一个程序。  

   + 与 *fork* 的区别在于：

     + **它并不将父进程的地址空间复制到子进程中** 。在子进程 *exec/exit* 之前，和父进程共享地址空间，提高了工作效率。但是在在子进程 *exec/exit* 之前，子进程如果修改了数据、进行函数调用、返回都会带来未知的结果。  
     + *vfork* 保证子进程比父进程先运行，在子进程 *exec/exit* 之后父进程才会运行。

   + 案例

     ```C++
     // 将上面的修改如下：
     if((pid=vfork()) < 0){
         printf("fork() error.\n");
         exit(1);
     }
     else if(pid == 0){
         globvar++; // 会改变父进程的变量值
         var++;
         _exit(0);
     }
     // 去掉sleep(2)是因为vfork能保证子进程先运行
     ```

     

   + 结果对比：
   
     ```bash
     # fork 
     szz@ubuntu:~/Study/SystemProgram/IO$ ./f
     a writte to stdout.
     before fork.
     pid=4304, globvar=11, var=89. # 子进程
     pid=4303, globvar=10, var=88. # 父进程
     
     # vfork
     szz@ubuntu:./vf
     a writte to stdout.
     before fork.
     pid=4301, globvar=11, var=89.# 父进程
     ```

+ *gdb* 调试  
  *`set follow-fork-mode child`*  : 跟踪子进程  
  *`set follow-fork-mode parent`* : 跟踪父进程
+ 进程和程序的区别   
  程序占用磁盘空间，进程占用系统资源。

### gcc 

1. 编译过程

   +  预处理:头文件展开，宏替换，注释去掉  
      *`gcc -E hello.c -o hello.i`*
   +  编译器:C文件变成汇编文件  
      *`gcc -S hello.i -o hello.s`*
   +  汇编器:把汇编文件变成二进制文件  
      *`gcc -c hello.s -o hello.o`*
   +  链接器:把函数库中的相应代码组合到目标文件中  
      *`gcc hello.o -o hello`*  

   编译过程：![编译过程](https://i.loli.net/2020/05/29/uoxDfnRX4CaNwTm.png)

2. gcc 参数

   + *`-I`* + 路径： 提供编译时所需头文件路径

### I/O操作

1. *`errno`*  
   *`errno`* 是一个全局的错误变量。  

2. *`open`*  
   open函数有两种设计：

   ```C++
   int open(const char* filename, int flags); // 打开已经存在的文件
   int open(const char* filename, int flags, mode_t mode); //可以用于创建文件
   /**
     flags: O_RDWR   // 读写
            O_RDONLY // 只读
            O_WRONLY // 只写
            配合
            "|O_CREAT"  // 文件不存在时创建
            "|O_TRUNC"  // 将打开的文件原来的内容清空
     mode: 文件的权限。文件的权限此处需要将实际文件权限(比如777)和本地掩码(umask得到0022)取反后进行按位与。
   */
   ```

3. *`read`*  
   *`read`* 设计

   ```c++
   sszie_t read(int fd, void* buff,  size_t count);
   /*
   size_t  表示无符号
   sszit_t 表示有符号
   返回值：
       -1: 读取失败
       0 : 读取完成
       >0: 读取的字节数
   */
   ```
   
4. *`write`* 和 *`read`* 类似。

5. 系统的I/O函数和C库函数的区别

   + 标准的C库函数，内部有一个系统维护的缓冲区。
   + 上面都的 *`write`* 和 *`read`*的缓冲区是由用户自己进行维护。

6. *`lseek`*   

   + 获取文件大小
   + 移动文件指针
   + 文件拓展

7. *`stat`* 

   + 命令：*`stat`*：*`stat + 文件名`*
   + 函数：  

# IPC

1. 简介  
   进程间通信的三种主要方式有:
    + 管道: 使用最简单
    + 信号：开销最小
    + 共享映射区：无血缘关系
    + 本地套接字：最稳定

### 1. mmp

+ 简介

  + `mmp`函数
  + 借组共享内存放磁盘文件，借组指针访问磁盘文件
  + 父子进程、血缘关系进程 通信
  + 匿名映射区

+ 函数

  ```c++
  void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
  ```
  
  函数返回的是：创建映射区的首地址，失败返回`MAP_FAILED`。

  + 参数
  + `addr`：直接传入 <font color=blue>`NULL`</font>
    + `length`：欲创建映射区的大小
    + `prot`：**映射区权限**：`PORT_READ、PORT_WRITE、PORT_READ|PORT_WRITE`。
    + `flags`：标志参数。是否会将映射区所作的修改反映到物理设备（磁盘）上。    
      &emsp;&emsp; a).`MAP_SHARED`：会     
      &emsp;&emsp; b). `MAP_PRIVATE`：不会  
    + `fd`：用来创建映射区的文件描述符
    + `offset`：映射文件的偏移。(4k的整倍数)
  + 注意事项  
    + 不能创建0字节大小的映射区，因此新创建出来的文件不能用于创建映射区
    + 权限  
      &emsp;&emsp; a). 创建映射区的权限，要小于等于打开文件权限  
      &emsp;&emsp; b). 创建映射区的过程中，隐含着一次对打开文件的读操作。  
    + `offset`: 必须是4k的整数倍,`mmu`创建的最小大小就是4k
    + 关闭fd，对`mmap`无影响。
  
+ 父子进程通信  
  父子进程之间的也可以通过`mmap`建立的映射区来完成数据通信。但是`mmap`函数的`flags`标志位应该设置`flags=MAP_SHARED`。

  + 父子进程共享：  

    + 打开的文件描述符  
    + `mmap`建立的映射区。

  + 匿名映射  
    由于`mmap`需要在进程之间通信时，需要借组一个文件描述符，但是对应的文件仅仅在通信期间存在，为了省略这一临时文件，产生匿名映射。  

    + Linux独有的方法：宏`MAP_ANONYMOUS/MAP_ANON`。  
      *`char* pmem = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANON, -1, 0);`*

     + 通用的方法：  

       ```c
       int fd = open("/dev/zero", O_RDWR);
       char* pmem = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);  
       ```

        使用的是一个文件`/dev/zero`完成。

+ `mmap`实现无血缘关系进程间通信  
  实际上`mmp`是内核**借助文件**帮我们创建了一个映射区。多个进程利用该映射区完成书的传递。由于内核是多进程共享，因此无血缘关系的进程间也可以使用`mmap`完成数据通信。

### makefile

```makefile
# obj=main.o ./src/add.c ./src/sub.c ./src/mul.c ./src/div.c
# makefile中提供的函数，都有返回值,使用函数来取代obj的首写输入方式
src = $(wildcard ./*.c, ./src/*.c)  # 表示取出指定目录下所有的.c文件
obj = $(patsubst ./%.c, ./%.o, $(src))
target = main
CC = gcc
CPPFLAGS = -I
$(target):$(obj)
	$(CC) $(obj) -o $(target)

%.o:%.c
	$(CC) -c $< -o $@
# 自动变量
# $<: 表示规则中的第一个依赖，比如main.o 
# $@: 表示规则中的目标，比如main
# $^: 表示规则中的所有依赖，比如main.o ./src/add.c ./src/sub.c ./src/mul.c ./src/div.c,
# 	  只能在规则中的命令中使用
# 系统变量 一般是大写。
# CC=gcc

# .PHONY:clean 表示生成伪目标，以防止和同目录结构下存在clean文件时无法编译
.PHONY:clean
clean:
#-rm 表示如果这条命令执行失败，就忽略
#-f  表示强制执行，以防止在没有*.o和main文件时，make clean失败。
	-rm -f  $(target) $(obj)
```

### exit、wait、waitpid、exec

1. *exit*  
   8种方式使进程终止，其中五种是正常终止:

   + 正常终止:  
     + 从 *main* 返回：return 0 == exit(0)
     + 调用 *exit*   
       在退出时，会调用一系列的终止处理程序(可以调用函数 *atexit* 进行注册)，关闭所有标准 *I/O*流等。
     + 调用 *_exit/_Exit*   
       Linux的 *_exit/_Exit* 与 *exit* 不同，前者是直接退出，不 *flush* 缓冲区，而后者会。*exit* 内部调用 *_exit*，且 *flush* 缓冲区。  
     + 最后一个线程从启动例程返回
     + 从最后一个线程调用 *pthread_exit*
   + 异常终止：
     + 调用 *abort* ：产生 *SIGABRT* 信号。
     + 接到一个信号
     + 最后一个线程对取消请求做出响应

   无论如何进程如何终止，最后都会执行内核中的同一段代码，关闭所有的描述符，释放所用的存储器。

2. 终止状态   
   当一个进行终止，内核向其父进程发生 ***SIGCHLD*** 信号。进程终止，其父进程通过调用 *wiat、waitpid* 来获取**正常终止**进程的的退出状态。  

   + 孤儿进程  
     父进程通过 *fork* 产生子进程，但是父进程在子进程之前终止，那么该子进程的父进程都会变成 ***init*** 进程，*init* 进程ID是1。这些父进程先终止的子进程被叫做 **孤儿进程**。
   + 僵死进程  
     如果子进程在父进程之前终止，父进程只能通过 *wiat、waitpid* 来获取终止的子进程的状态信息。这样一个已经终止、但是其父进程尚未对其进程善后处理的（获取子进程的相关信息、释放它占用的资源）进程叫做 **僵死进程**。 如果父进程没有 *wiat、waitpid* 来获取终止的子进程的状态状态，这些进程终止后都会变成 **僵死进程**。

   ` init进程的子进程不存在僵死进程：只要有一个进程终止，就会调用一个wait函数取得其终止状态。`

   + 查看僵死进程： `ps -e -o stat,ppid,pid,cmd|egrep '^[Zz]'`
   + 杀死僵死进程： `kill -9 ID`  
     [参考地址](https://blog.csdn.net/qq_37837134/article/details/82683107)

   ```bash
    s：ps命令用于获取当前系统的进程信息.
   
   -e：参数用于列出所有的进程
   
   -o：参数用于设定输出格式。这里只输出进程的stat(状态信息)、ppid(父进程pid)、pid(当前进程的pid)，cmd(即进程的可执行文件)。
   
   egrep：是linux下的正则表达式工具
   
   '^[Zz]'：这是正则表达式，^表示第一个字符的位置，[Zz]，表示z或者大写的Z字母，即表示第一个字符为Z或者z开头的进程数据，只所以这样是因为僵尸进程的状态信息以Z或者z字母开头。
   ```

3. ***wiat、waitpid***  

   ```c
   pid_t wait(int* statloc);  
   pid_t waitpid(pid_t pid, int* statloc, int options); // 
   /*
   // 返回时返回值是终止进程的id，子进程终止状态存储在statloc
   // 如果不关心终止状态，就令statloc为null，statloc详见P191
   */
   ```
   
+ 异同点
     + *wiat* 是阻塞的，*waitpid* 有个选项可以不阻塞。
     + *wiat* 等待其调用后的第一个子线程终止， *waitpid* 可以控制它等待的子线程。
     + *wait* 出错是：调用该函数的进程没有子线程，*waitpid* 出错还可能是指定的进程或者进程组不存在。
     + 都可以回收子进程资源
     + 都获取子进程退出状态（退出原因）。

4. *exec*  

   ```c++
   extern char **environ;
   
   int execl  (const char *path, const char *arg, ... /* (char  *) NULL */);
   int execv  (const char *path, char *const argv[]);
   int execve (const char *path, char *const argv[], char const* envp[]);
   int execle (const char *path, const char *arg, ... /*, (char *) NULL, char * const envp[] */);
   int execlp (const char *file, const char *arg, ... /* (char  *) NULL */);
   int execvp (const char *file, char *const argv[]);
   int execvpe(const char *file, char *const argv[], char *const envp[]);
   ```
   

只有失败时候才返回-1，成功时直接执行被执行的函数，不会返回，即 *exec* 同一作用域的后面的部分不会运行：

```c
       exec(...);
       perroe("exec call error.\n");
       exit(1);
```


### 动态库和静态库

---

目录结构：

```
.
├── include
│   ├── head.h
│   └── note.md
├── lib
│   └── libMyCal.a
├── main.c
└── src
    ├── add.c
    ├── div.c
    ├── mul.c
    └── sub.c
```

1. 静态库
   + 生成.o文件：`gcc *.c -c -I../include`
   + 制作静态库：`ar -cr libMyCal.a  *.o`
   + 使用:
     + `gcc main.c  lib/libMyCal.a  -o main -I./include`
     + `gcc main.c -I ./include -L lib -l MyCal -o main_`



2. 动态库

   + 制作： gcc -c -fPIC -I ../include *.c

   + 生成： gcc -shared -o libMyCal.so -I ../include *.o

   + 使用：

     + `gcc main.c lib/libMyCal.so -o m -I ./include`

     + `gcc main.c -I ./include -L lib -l MyCal -o main_`

       但是这么写会报错：

       ```
       ./ main_: error while loading shared libraries: 
           libMyCal.so: cannot open shared object file: No such file or directory
       ```

       解决方法是：
           

       + 将libMyCal.so放到系统的lib下，放的也都是动态链接库: `sudo cp ./lib/libMyCal.so  /lib/`

         再去执行`./main_` 就能顺利编译

         可用`ldd` 命令查看 `main_`的依赖项目：

         ```bash
         (base) szz@szz:/media/szz/Others/Self_study/Cpp/Study/sdll$ ldd main_ 
             linux-vdso.so.1 =>  (0x00007fff07e96000)                          # shared object file
             libMyCal.so => /lib/libMyCal.so (0x00007fd692a86000)              # 动态库    
             libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007fd6926bc000) # 系统的C库
             /lib64/ld-linux-x86-64.so.2 (0x00007fd692c88000)                  # 动态链接器
         ```

       + 加入搜索路径：`export LD_LIBRARY_PATH=./lib/`

         直接将动态库放到系统的动态库中，很不安全，因此可能会覆盖重名的系统库。更加安全的方式是通过`LD_LIBRARY_PATH`使得系统在搜索环境变量之前搜索这个路径。 

         ```bash
         linux-vdso.so.1 =>  (0x00007ffce2710000)
         libMyCal.so => ./lib/libMyCal.so (0x00007fee92742000)               # 与上面解决方案对应路径不同
         libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007fee92378000)
         /lib64/ld-linux-x86-64.so.2 (0x00007fee92944000)
         
         ```

         注意： 这个方案是临时的，用于测试使用，想要永久生效，**需要将上述命令中的相对路径换成绝对路径加入`.bashrc`文件中**。


       + 第四种解决方案


        + 动态库的优、缺点：
            + 体积小； 动态库更新了，不需要重新编译程序(只要函数接口没变)
            + 加载速度相对静态库慢一点

# 守护进程

Daemon(进程),是Linux中的后台服务器进程，通常独立于控制终端，并且周期性的执行某种任务或者等待处理。某些发生的事件，一般采用d结尾的名字。

Linux后台的一些系统服务进程，没有控制终端，不能直接和用户交互。不受用户登录、注销的影响，一直在运行着，他们就是守护进程。

## 创建守护进程

1. 创建子进程，父进程退出。
   使得所有工作在子进程中的任务脱离终端。
2. 在子进程中创建新`session`
   + `setsid()`函数
   + 使得子进程完全独立出来，脱离父进程的权限控制
3. 改变当前目录为根目录
   + `chdir()`函数
   + 这是为了防止占用可卸载的文件系统
4. 重新设置文件掩码
   + `umask()`函数
   + 防止继承的文件创建屏蔽字来拒绝某些权限
5. 关闭文件描述符
   + 关闭:`STDOUT_FILENO`、`STDIN_FILENO`、`STDERR_FILENO`。
   + `dup2()`函数，重定向到`dev/null`
6. 守护进程的任务。  
   完成前面的准备工作，这里开始实现任务。
7. 守护进程退出
   可使用信号机制。

## 文件权限

1. *chmod*  

   + 数字表示法

     + `chmod 权限命令 文件`  
     + `chmod -R 权限命令 文件夹`   
       加上 *-R*  表示递归的执行文件夹下面的所有文件。权限命令分为`r:4,w:2:x:1"`的组合。  
       比如: `-rwxrwxrwx 1 szz szz 560 Nov 29 03:16 wiat_.c` 前面的权限 `-rwxrwxrwx` 就是表示777。

   + 字符表示法  

     + `chmod [用户类型] [+|-|=] [权限字符] 文件名`     
     + `chmod -R [用户类型] [+|-|=] [权限字符] 文件夹`     
       ![文件权限](G:%5CSSE%5CInterview%5CBackEnd%5Cmybook%5Cdocs%5Cunix_linux%5CAPUE%5CImage%5C%25E6%2596%2587%25E4%25BB%25B6%25E6%259D%2583%25E9%2599%2590.jpg)


     比如：
    
     ```bash
     $ chmod u+rw wait.c  # 给用户 读写权限
     $ chmod u-rw wait.c  # 去掉用户的读写权限
     $ chmod g+rw wait.c  # 给组 读写权限
     $ chmod g=x wait.c   # 设置组具有执行权限
     $ chmod a=x wait.c   # 都有执行权限
     ```

+ *chown* 

  + `chown [选项]... [所有者] [:组] 文件 ...`
  + `chown -R [所有者] [:组] 文件夹 ...`

  比如:

  ```c
  $ ls -l wiat_.c 
  ---x-wxrwx 1 szz szz 560 Nov 29 03:16 wiat_.c
  
  //修改用户
  $ sudo chown root wiat_.c 
  ---xrw-rwx 1 root szz 560 Nov 29 03:16 wiat_.c
  
  // 修改组
  $ sudo chown .root wiat_.c 
  ---xrw-rwx 1 root root 560 Nov 29 03:16 wiat_.c
  
  //同时修改
  $ sudo chown szz.szz wiat_.c 
  ---xrw-rwx 1 szz szz 560 Nov 29 03:16 wiat_.c
  ```

3. 增加/删除用户  
   在指定的组里添加成员。

   + 增加组  : `groupadd groupname`
   + 增加用户: `useradd username -g groupname` 
   + 删除用户: `userdel username -g groupname` 
   + 查看 : `id username`
   + 改变用户： `sudo su - username`

4. 对于一个文件操作  
   假设当前环境是：       

   + 用户有: *lam, sz, other*
   + 组别：*family*  
     状态如下：*lam, sz* 属于同一个组 *family*。

   ```bash
   $ id lam
   uid=1001(lam) gid=1002(family) groups=1002(family)
   $ id sz
   uid=1002(sz) gid=1002(family) groups=1002(family)
   ```

   有文件：wait.cpp

   ```bash
   $ ls -l wait.cpp 
   ---xrw-r-- 1 lam family 20 Nov 29 05:09 wait.cpp
   ```

   权限解释：  

   + user : 对于用户 *lam* 具有可执行权限(*x*)
   + gropu: 对于属于组*family*的成员都具有具有rw权限
   + other: 对于其他人*other*具有*r*权限
     要是想让不同的成员获得想要的权限，就可以使用 *chowm* 命令来改变。      
   + **删除** ：删除一个文件，是看该文件所属的目录权限

5. 对于一个目录的操作

   ` 目录Process: dr-x--x--- 2 lam family 4096 Nov 29 05:14 Process `

   用户*other* 目前对于 *Process/* 目录没有任何的执行能力。

   ```
   $ cd Process/
   -su: cd: Process/: Permission denied
   
   $ ls Process/
   ls: cannot open directory 'Process/': Permission denied
   ```

   把 *`Process/`* 的权限改为 *`dr-x--x--x`* 可以执行 *`cd`*, 再改为 *`dr-x--xr-x`* 可以执行 *`ls Process/`*。 

   + 总结
     + 可读r: 表示具有浏览目录下面文件及目录的权限。即可用 *ls*。
     + 可写w: 表示具有增加、删除或者修改目录内文件名的权限
     + 可执行x: 表示居于进入目录的权限，即 *cd*。 

6. 默认权限  
   文件权限计算方法与*umask*   

   + 创建目录默认的最大权限是：777

   + 创建文件最大权限是：666  

     + 创建目录时：用 777- *umask* 即可得到所得目录权限  

     + 创建文件时：
       如果 *umask*得奇数位，用 666-*umask* 后，将 *umask* 的奇数位加1。  
       比如 *umask =303*， 那么文件权限是：666-303= 363,363+101=464，即 *-r--rw-r--*

       ```bash
       $ umask 303
       $ touch msk_303
       $ ls -l
       -r--rw-r-- 1 szz szz    0 Nov 29 06:39 msk_303
       ```

7. *uid/gid*  

+ *uid*  

  + `uid` 应用的对象是 **命令**，而不是文件。  
  + `suid` 仅该指令执行过程中有效。
  + 指令经过 *suid* 后，任意用户在执行该指令时，都可获得该指令对应的拥有者所具有的权限。   

  修改密码的指令权限如下，在用户位权限上有个 `s`，就是代表 *suid* ：

  ```bash
  $ ls -l /usr/bin/passwd
  -rwsr-xr-x 1 root root 59640 Mar 22  2019 /usr/bin/passwd
  ```

  **注意**：用户权限前三位上的x位上如果有s就表示 *`suid`*,当x位置上没有x时， *`suid`* 就是 *`S`*。

  现在有如下目录结构:  

  ```bash
  Permission/
  └── test
  ```

  权限：

  ```bash 
  drwxrwxr-x 2 root root   4096 Nov 29 19:06 Permission
  -rw-r--r-- 1 root root    0   Nov 29 19:06 test
  ```

  普通用户 `sz`想要删除文件 `test` 权限不够。  

  ```bash
  $ rm test
  rm: remove write-protected regular empty file 'test'? Y
  rm: cannot remove 'test': Permission denied
  ```

  经过给指令 `rm` 设置uid之后，即设置命令`rm`具有所属的用户权限。比如：

  ```bash
  $ which rm
  /bin/rm
  $ ls -l /bin/rm
  -rw-r-xr-x 1 root root 63704 Jan 18  2018 /bin/rm
  $ sudo chmod u+s `which rm`
  $ ls -l /bin/rm
  -rwsr-xr-x 1 root root 63704 Jan 18  2018 /bin/rm
  $ rm test  # 删除成功
  ```

  上面都设置 *`uid`* 功能就是使得 *`rm`* 命令具有其所属的 *`root`* 具有的权限。  
  **注意**：`rm`命令比较危险，需要将其命令改回去。

  ```bash
  $ sudo chmod 755 /bin/rm
  $ ls -l /bin/rm
  -rwxr-xr-x 1 root root 63704 Jan 18  2018 /bin/rm
  ```

+ *sgid* 
  *sgid* 与 *suid* 不同地方是 *sgid* 即可以对文件也可以针对目录设置。

    + 针对文件
      + *sgid* 针对二进制程序有效
      + 二进制命令或者程序需要可执行权限x
      + 指令经过 *sgid* 后，任意用户在执行该指令时，都可获得该指令对应的所属组具有的权限。
    + 对于目录
      + 用户在此目录下创建的文件和目录，都有和此目录相同的用户组。

+ *suid/sgid*的数字权限设置方法   
  *suid/sgid*位设置也是八进制。

  + *setuid* 占用的是八进制:`4000` 
  + *setgid* 占用的是`2000`
  + 粘滞位:占用的是 `1000`  
    在之前的`chmod`命令前面加上 **4/2/1** 即可。

+ [参考链接](https://www.bilibili.com/video/av57473824?p=14)

### 系统IO 函数

---

1. C库函数  
   C语言的库函数的I/O操作都是带有缓冲区的，只有当缓冲区满了才会进行相应的操作。  

   ```c
   fopen、fclose、fread、fwrite、fgets、fputs、fscanf、fprintf、fseek、fgetc、fputc、ftell、feof、flush...
   ```

   + 刷新缓冲区

     + 强制性刷新:*`flush`*

     + 缓冲区已满

     + 正常关闭文件:  

       > `fclose`  
       > `return`(main函数中)  
       > `exit`(main函数中)

   + C语言的库函数操作后组成:

     + 文件描述符: *`FILE* fp`*,索引到对应的磁盘文件
     + 文件读写指针: 读写文件过程中的实际位置
     + I/O缓冲区(默认是8K): 通过寻址找到对应的内存块

   + *`printf`*  
     *`printf`* 函数完成的工作流程如下图所示。
       ![*`printf`* 工作流程](https://i.loli.net/2020/05/29/58HCkynFwPJeVjd.png)  
        *`printf`*  调用后，会产生一个由系统维护的一个结构体，包含了

       ```cpp  
       file description 文件描述符号
       FP_POS           读写指针
       I/O buffer       缓冲区
       ```

       +  *`printf`* 会调用系统API *`write`*，系统通过 *`write`* 来操作内核，使得内核可以调用设备驱动使得输出的内容显示在显示器上。

2. **虚拟地址空间**  
   模型如图：  
       ![虚拟地址模型](https://i.loli.net/2020/05/29/UQpAGEtMiLFK1Ve.png)

   + 可执行程序(*`elf格式`*)包括：  
     + *`.text`*:代码段
     + *`.data`*:已经初始化的全局变量段
     + *`.bss`*: 未初始化的全局变量段
     + 其他段：只读数据段，符号段等  
       main函数就是在 *`.text`* 段中，从这里开始执行。

   + 栈与堆   
     栈，存放的是临时变量，生长方向是向下生长。  
     堆，存放的是 *`new/malloc`* 得到的变量，生长方向是向上生长。

   + 共享库与 *`.text`*
     + 共享库存放的是代码的偏移地址。C库函数和Linux系统库函数都是动态库中。
     + *`.text`* 存放的是静态库，或者说是存放的是代码的绝对地址。

   + 命令行参数  
     *`int main(int argc, const char* argv[])`* 中的 *`argc, argv`*。  

   + 环境变量(*`env`*)

   + Cpu使用虚拟内存地址空间与物理地址空间映射？解决了什么问题？  
     + 方便编译器和操作系统安排程序的地址分布。程序可以使用一系列相邻的虚拟地址来访问物理内存中不相邻的大内存缓冲区。
     + 方便进行之间隔离。不同进程之间使用虚拟地址彼此隔离，一个进程中代码无法更改另一个进程使用的物理内存。
     + 方便使用不多的内存。

# 信号

#### 概念

信号是由于进程产生，但是由内核调度传递给另一个进程：
![信号捕捉](G:%5CSSE%5CInterview%5CBackEnd%5Cmybook%5Cdocs%5Cunix_linux%5CAPUE%5CImage%5C%25E4%25BF%25A1%25E5%258F%25B7%25E6%258D%2595%25E6%258D%2589%25E8%25BF%2587%25E7%25A8%258B.jpg)

1. 产生信号

   + 按键产生信号:  
     + <font color=Crimson> `Ctr+c --> 2)SIGINT(终止/中断)`</font>   
     + <font color=Crimson> `Ctr+z --> 20)SIGTSTOP(终端暂停)` </font>   
     + <font color=Crimson> `Ctr+\ --> 3)SIGQUIT(退出)`</font>   
   + 系统调用产生: `kill(2), raise, abort`
   + 软件条件产生: 如定时器`alarm`
   + 硬件异常产生: 
     + 如非法访问内存(段错误): `11)SIGSEV(段错误)`
     + 除0: `8)SIGFPE`
   + 命令产生：`kill(1)`

2. 递达：产生的信号递达到了接受信号进程

3. 未决：

   + 介于产生信号和递达之间的状态，主要由于阻塞信号集导致。
   + 我们可以直接操作的是堵塞信号集，来影响未决信号集。

4. 信号的处理方式

   + 默认：但是每个信号的默认动作可能不一致
     + Term: 终止进程
     + Ign : 忽略信号（默认即时对该种信号忽略操作）
     + Core: 终止进程，生成Core文件(查验进程死亡原因，用于gbd调试)
     + Stop: 停止（暂停）进程
     + Cnt : 继续运行进程
     + ***`9)SIGKILL、19)SIGSTOP`*** 不允许忽略和捕捉，只能执行默认动作。

   + 忽略：对于该信号的处理动作就是忽略
   + 捕捉：调用用户处理函数  

   

#### signal相关函数

##### <font color=Crimson>`1.kill`</font>  

```c
    #include <sys/types.h>
    #include <signal.h>

    int kill(pid_t pid, int sig);
```

+ 第一个参数：
  + pid > 0: 发送信号给指定的进程
  + pid = 0: 发送信号给与调用进程同一组的的所有进程，**而且发送进程有权向他们发送信号**
  + pid < 0: 将信号发送对应进程组ID=|pid|，**而且发送进程有权向他们发送信号**
  + pid =-1: 发送给这样的进程：**而且发送进程有权向他们发送信号**  

#####  <font color=Crimson>`2.raise/abort`</font> 

```c
    // raise
    #include <signal.h>
    int raise(int sig);
    // abort
    #include <stdlib.h>
    void abort(void);
```

+ raise : 自己给自己发信号
+ abort : 自己给自己发终止信号

##### <font color=Crimson>`3.alarm`</font>   

```c
    #include <unistd.h>
    unsigned int alarm(unsigned int seconds);
```

+ 设置定时器，在指定seconds之后，<font face='黑体' color=red>不管进程处于什么状态，时间一到就发送信号</font>。内核给当前进程发送是`14)SIGALRM`信号，进程收到该信号，默认动作是终止进程。
+ `seconds == 0`时，即取消定时器
+ 第一次设置定时器返回值0。后面再设置定时器时，会覆盖前面的定时器，重新计数，此时返回值是前面定时器剩余的秒数。
+ <font face="黑体" color=red>每个进程有且只有一个定时器。</font>

##### <font color=Crimson>`4.setitimer`</font> 

```c++
    #include <sys/time.h>

    int getitimer(int which, struct itimerval *curr_value);
    int setitimer(int which, const struct itimerval *new_value,
                            struct itimerval *old_value);

        struct itimerval {
            struct timeval it_interval; /* Interval for periodic timer */
            struct timeval it_value;    /* Time until next expiration */
        };

        struct timeval {
            time_t      tv_sec;         /* seconds */
            suseconds_t tv_usec;        /* microseconds */
        };
```

`setitimer` 相比较 `alarm` 提供了更加精确的定时信号控制，前者是微妙级，后者是秒级。

+ which
  + `ITIMER_REAL`&emsp;&ensp;: 等同于 `alarm`，自然定时，即和进行状态无关，时间到就发送`SIGALRM`
  + `ITIMER_VIRTUAL`: 计算进程占用cpu时间，发送信号`SIGVTALRM`
  + `ITIMER_PROF` &emsp;&ensp;: 计算进程cpu调用及执行系统调用时间，发送信号`SIGPROF`
+ `new_value/old_value`  
  + 前者是需要设置的时间，后者是返回的时间。
  + `setitimer` 是一个周期定时，`new_value`设置的本次和下次定时时间。当下次定时时间为0，就仅仅定时一次。


##### `5. singal`

```c
    #include <signal.h>

    typedef void (*sighandler_t)(int);

    sighandler_t signal(int signum, sighandler_t handler);
```

通过`signal`函数可以自定义信号处理函数来应对信号。信号是由内核检测到，而函数`signal`是将信号和信号处理函数联系起来。

+ 第一个参数: `signal`是接受到的信号，传递给信号处理函数。
+ 第二个参数: 是信号处理函数，其格式:`typedef void (*sighandler_t)(int);`即返回类型是`void`，参数是`int`类型的函数，同时这个函数的接受值就是产生的信号。

##### `6. 信号集`

内核通过读取未决信号集来判断信号是否应该被处理。信号屏蔽字mask可以影响未决信号集，而信号屏蔽字需要通过数据类型`sigset_t`创建的对象`set`来设置`mask`，来达到屏蔽信号的目的，即影响未决信号集来决定信号是否将被处理。他们之间的关系如图:  
![信号集](G:%5CSSE%5CInterview%5CBackEnd%5Cmybook%5Cdocs%5Cunix_linux%5CAPUE%5CImage%5C%25E4%25BF%25A1%25E5%258F%25B7%25E9%2598%25BB%25E5%25A1%259E.jpg)

+ <a name="mask">mask操作函数</a>

  ```c
      #include <signal.h>
  
      int sigemptyset(sigset_t *set);     // 将信号集合清空
      int sigfillset(sigset_t *set);      // 将信号集全部置1
      int sigaddset(sigset_t *set, int signum);   // 将某个信号加入信号集
      int sigdelset(sigset_t *set, int signum);   // 将某个信号清出信号集
      int sigismember(const sigset_t *set, int signum);   // 判断某个信号是否在信号集
  
      #define _SIGSET_NWORDS (1024 / (8 * sizeof (unsigned long int))) // 32
      typedef struct {
          unsigned long int __val[_SIGSET_NWORDS];
      } __sigset_t;
  
      typedef __sigset_t sigset_t; 
  ```

  `sigset_t`是一个位图，通过上述函数来操作这个位图。上面的函数成功返回0，失败返回-1。

+ `sigprocmask` 
  用来获取或者改变调用进程的信号mask。也可屏蔽信号、解决屏蔽。

  ```c
      #include <signal.h>
  
      int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
  ```

  + 第一个参数：决定了函数调用的行为  
    + `SIG_BLOCK` &emsp;: &ensp;进程的屏蔽信号集是这个`set`和原来的屏蔽信号笔的并(union)。
    + `SIG_UNBLOCK`： 将`set`中的信号从当前信号集合中移除出去。
    + `SIG_SETMASK`： 直接将`set`覆盖当前信号集
  + 第二个参数：`set`即上面的`sigset_t`格式，是本次函数给阻塞信号的设定值。
  + 第三个参数：返回的当前的信号屏蔽集合。

+ `sigpending`

  ```c
      #include <signal.h>
  
      int sigpending(sigset_t *set);
  ```

  将调用进程的未决信号集存储在`set`中传出。

##### `7. sigaction`

```c
    #include <signal.h>

    int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);

        struct sigaction {
            void     (*sa_handler)(int);        
            void     (*sa_sigaction)(int, siginfo_t *, void *);
            sigset_t   sa_mask;
            int        sa_flags;
            void     (*sa_restorer)(void);
        };
```

+ `struct sigaction` 
  + `sa_handler`: 用来指定捕捉到信号时候的行为。`SIG_DFL`、`SIG_IGN`、信号处理函数。
  + `sa_sigaction`: 
    当 `sa_flags = SA_SIGINFO`时，`sa_sigaction`指定信号处理函数(之前是`sa_handler`指定)。
  + `sa_mask`：指定的是在<font face='黑体' color=red>**处理信号的过程中**</font>需要屏蔽的信号，即不捕捉该该信号，那么就会执行默认动作。而且触发该处理函数的信号再来时候会丢弃，即<font face='黑体' color=red> **多次信号只执行一次** </font>，除非`sa_flags = SA_NODEFER`。
  + `sa_flags`：指定的是用于修改信号行为的标志位，默认是0，即在信号处理函数执行期间自动屏蔽本信号。这个函数的操作还是要依赖 <a href="mask">mask的操作函数。</a>
  + `sa_restorer`：不要使用。
+ 返回值：成功是0，失败是-1.

# 信号2

#### 1. `pause`

```c
    #include <unistd.h>

    int pause(void);
```

调用`pause`函数的进程将一直堵塞到(即放弃cpu)有信号递达将其唤醒。

+ 返回值：
  + 如果信号的默认动作是终止进程，则进程终止。
  + 如果信号的默认动作是忽略，继承基于处于挂起状态，`pause`不返回。
  + 如果信号处理动作是捕捉，则信号处理函数调用结束，**`pause`返回-1, `errno`设置为`EINTR`**。
  + `pause`接受到的信号被屏蔽，`puase`就将会永远不被唤醒。
+ 实现`sleep`函数功能：`alarm() + pause() `

#### 2.`sigsuspend`

```c
    #include <signal.h>

    int sigsuspend(const sigset_t *mask); // mask是临时的，在函数执行时有效
```

+ 特点。这是一个原子操作，将**设置mask指的屏蔽字** + **使得进程休眠（pause）** 变成原子操作。可用来替代`pause`具有的时许竞争隐患。比如用`alarm() + pause() `实现的`sleep`函数功能。
+ 返回。和`pause`一样，无成功返回值。如果返回调用进程，返回-1，并且`errno=EINTR`。

```c
    alarm(secs);
    pause();
```

+ 原因：  

  如果在调用`alarm`之后，调用进程失去了cpu控制权，但是在cpu控制权回到这个进程之前`alarm`的计时时候到达，当cpu控制权回归调用进程时先处理`SIGALRM`信号，再调用`pause`函数，这就导致`pause`再也接受不到信号，因此这个进程就永远堵塞。

+ 解决方案：  

  使得`sigsuspend(sus_mask) = alarm() + pause() `成为一个原子操作。

  + 先使用`sigprocmask`设置信号屏蔽集合，使得cpu在`sigsuspend`之前不处理。
  + 比如同样发生上述调用进程失去cpu控制权的情况，cpu控制权返回时，并不会先处理`SIGALRM`信号
  + 因此到`sigsuspend`函数时，在这个函数的`sus_mask`中解除对`SIGALRM`的屏蔽，使得 **信号处理函数的执行后直接返回，唤醒cpu（这是因为pause的堵塞导致）**。

+ 关键点: **调用进程的堵塞原因**

  + 调用进程失去cpu的控制权
  + 调用`pause`导致的cpu挂起

#### 3.全局变量异步I/O

 尽量避免使用全局变量。类似于多线程中的多个线程对同一个变量进行操作，很容易造成异常。

#### 4.不可/可重入函数

 + 定义可重入函数，函数内部不能含有全局变量或者`static`变量，以及`malloc`和`free`。
 + 信号捕捉函数应该设计为可重入函数。
 + 信号处理函数可以调用的可重入函数，参考 man 7 signal
 + 其他大多是不可重入的，因为：
   + 使用静态数据结构
   + 调用`malloc/free`
   + 是标准I/O函数

#### 5.`SIGCHLD`

这是子进程向父进程发送的信号，那么可否利用这个信号来回收子进程？

+ 要求
  + 在父进程中注册对信号`SIGCHLD`的捕捉函数。
  + 循环产生十个子进程
  + 十个子进程在父进程前终止
  + 能顺利回收十个子进程

+ 信号处理函数

```c
    void exe_sigchld(int singo) {
        pid_t pid;
        int statloc;
        // 这里的while很关键
        while(pid = waitpid(0, &statloc, WNOHANG) > 0) {
            if(WIFEXITED(statloc))  
                printf("child process pid=%d, exited  status:%d.\n", pid, WEXITSTATUS(statloc));
            else if(WIFSIGNALED(statloc))
                printf("child process pid=%d, signaled stauts:%d.\n", pid, WTERMSIG(statloc)); 
        }
    }
```

上面都`while`处理很关键，不是`if`而是`while`，是因为：

+ 对于多个子进程同时发送终止信号`SINCHLD`，父进程只调用一次信号处理函数。
+ 如果是`if`那么进入一次信号处理函数，只能处理一个终止的子进程，那么对于同时终止的其他进程就无法处理，剩下的就只能成为僵死进程。
+ 选用`while`就可以一次进入信号处理函数，但是可以处理多个终止的子进程。

#### 6.信号传参

信号不能携带大量参数，实在有特殊需求时也可以。有相关的函数：

+ 发送信号传参

```c
    #include <signal.h>

    int sigqueue(pid_t pid, int sig, const union sigval value);
        
    union sigval {
        int   sival_int;
        void *sival_ptr;
    };
```

类似于`kill`,但是多了一个发送参数，可以作为数据发送。联合体`sigval`在跨进程传递数据时候不要使用指针，因此各个进程之间的虚拟地址不同，指针传参是为同一个进程准备的，跨进程传参使用的是int类型。

+ 捕捉函数传参

```c
    #include <signal.h>

    int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);

    struct sigaction {
        void     (*sa_handler)(int);        
        void     (*sa_sigaction)(int, siginfo_t *, void *);
        sigset_t   sa_mask;
        int        sa_flags;
        void     (*sa_restorer)(void);
    };
```

使用的结构体`sigaction`的第二项:`sa_sigaction`，此时`sa_flags = SA_SIGINFO`。

#### 7.中断系统调用

系统调用分为二类：慢速系统调用和其他系统调用

+ 慢速系统调用：可能会使进程永远堵塞的一类。如果在堵塞期间收到一个信号，该系统调用就会被中断，那么就不再被执行。也可以设定系统调用是否重启。这类函数诸如:`read、write、pause、wait`。
+ 其他系统调用：`getpid(), getppid(),fork()...`

慢速系统调用**被信号中断**时的行为，和`pause`类似：

+ 想中断`pause`，信号不能屏蔽
+ 信号的处理方式必须是捕捉（默认和忽略都不可）
+ 中断后返回-1，设置`errno`为`EINTR`

可修改`sa_flags`参数来设置被信号中断后系统调用是否重启。 

+ `SA_RESTART`:重启这个慢速调用函数  
  比如：
  + `read`被一个信号中断，处理完信号，`read`应该继续工作，所以需要重启。
  + `pause`被信号中断就不需要重启，因为不影响。
+ `SA_NODEFER`:不屏蔽待捕捉信号