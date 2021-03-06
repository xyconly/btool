btool
=====

basic tool 基础工具
-------------------

### 采用C++17标准与boost库，主要用于各类基础工具的快速使用，尽可能的跨平台，减少开发人员的底层逻辑，专注于业务开发

-   **C++17标准库，请使用VS2019及以上版本；**

-   **属性路径中需自己配置boost各版本include路径与lib路径；**

-   命名方式参考google风格，但是在函数命名时参考标准库形式，因为那样看起来很轻便，静态函数采用大写波峰，便于区分；

-   最后，**使用时请标注来源。**

主要功能说明
--------

-   并行执行同属性任务最新状态的线程池: LastTaskPool

-   FIFO并行执行线程池: ParallelTaskPool

-   FIFO并行执行线程池，但同属性任务同一时间内仅能在一个线程中执行: SerialTaskList

-   字符串转换工具: StringConvert

-   时间转换工具: DateTimeConvert

-   可设定循环次数时间轮定时器管理: TimerManager

-   Boost TCP服务: BoostNet::TcpServer

-   Boost TCP连接: BoostNet::TcpSession

-   Beast1.68 / beast1.71 版本Websocket服务: BoostNet1_68::WebsocketServer / BoostNet1_71::WebsocketServer

-   Beast1.68 / beast1.71 版本Websocket/websocketssl连接: BoostNet1_68::WebsocketSession/BoostNet1_68::WebsocketSslSession  /  BoostNet1_71::WebsocketSession/BoostNet1_71::WebsocketSslSession

-   Beast1.68 / beast1.71 版本http/https服务: BoostNet1_68::HttpServer/BoostNet1_68::HttpsServer  /  BoostNet1_71::HttpServer/BoostNet1_71::HttpsServer

-   Beast1.68 / beast1.71 版本http/https连接: BoostNet1_68::HttpSession/BoostNet1_68::HttpsSession  /  BoostNet1_71::HttpSession/BoostNet1_71::HttpsSession

-   读写锁统一声明: rwmutex.hpp

-   RAII资源管理: scope_guard.hpp

-   安全线程: safe_thread.hpp

-   文件读写: file_system.hpp

-   实时可控instance懒惰实现: instance.hpp

-   同步状态: sync.hpp

-   加密解密序列化: aes.hpp / des.hpp / md5.hpp

-   结合Protobuf实现的统一消息打包解包pb_message_package,结合协议代码自动生成工具可快速编写协议,使开发者仅关注一次协议制定之后,将精力集中于业务开发

-   基于可适配的简单RPC通讯: BTool::BoostRpc::RpcServer / BTool::BoostRpc::RpcClient
