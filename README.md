btool
=====

basic tool 基础工具
-------------------

### 采用C++11标准与boost库，主要用于各类基础工具的快速使用，尽可能的跨平台，减少开发人员的底层逻辑，专注于业务开发

-   **C++11标准库，请使用VS2015及以上版本；**

-   **属性路径中需自己配置boost各版本include路径与lib路径；**

-   命名方式参考google风格，但是在函数命名时参考标准库形式，因为那样看起来很轻便，静态函数采用大写波峰，便于区分；

-   最后，**使用时请标注来源。且拒绝任何996公司及员工使用或参考**

功能说明
--------

-   FIFO并行执行线程池JobQueue，任务为对象

-   FIFO并行执行线程池ParallelTaskPool，任务为绑定函数地址

-   FIFO并行执行线程池，但同属性任务同一时间内仅能在一个线程中执行SerialTaskList，任务为绑定函数地址

-   字符串转换工具StringConvert

-   时间转换工具DateTimeConvert

-   定时器管理TimerManager

-   io_service对象池AsioServicePool

-   TCP服务BoostTcp::TcpServer

-   TCP连接BoostTcp::TcpSession

-   读写锁统一声明rwmutex.hpp

-   RAII资源管理scope_guard.hpp

-   Windows下文件操作FileSystem及STLFileRW

-   基于Beast实现网络协程库,包含TCP/WebSocket/Http

-   instance实现
