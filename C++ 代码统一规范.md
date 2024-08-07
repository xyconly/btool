# C++ 代码统一规范

作者:  AChar

版权:  引用时请注明来源, 更改后可追加作者列表, 并显式标注更改项

时间: 2021-09-28

## 0. 开篇词

​    由于时间有限, 该规范基本属于想到哪写到哪的形式, 主要参考的是Google风格, 然而Google在大力发展Go等语言的同时, 明显对C++越来越不重视, 甚至有时成为了C++发展的一大阻力之一, 在命名风格上, 个人还是对其有些不同的意见, 这当然会使得很多人觉得这是一个异类, 应当挑选良辰吉日祭天, 以儆效尤. 

​    但即使如此, 我还是依旧坚持我的部分观点, 因为我始终认为, 代码, 就应该是***想当然***, 让人看一眼便能区分其含义, 传统的Google命名, 虽然对于接口十分友好, 然而Google并非首创, 也是在前人的基础上做了修改, 部分修改其实并不认同, 例如变量的命名规范中, 我更倾向于***Google风格 + 匈牙利命名法***, 我认为比Google命名法更加接近***想当然***这个定义, 让人看一眼, 即明白.

​      同时, 作为一名 coder, 也应该做到 ***顺眼***,  而在函数命名中, 不知大家是否有过这样的体验, 在使用标准库或者boost这个准标准库时, 小写的函数看着真的是十分舒心, 大写字母, 当看上去的第一眼, 我的第一反应是沉重, 而书写代码, 就应该是轻松而愉悦的, 在代码的世界里, 你可以实现任何你想的功能, 而不用受限于你的身体肌肉强度, 即使身为码农, 你也可以在一个硬盘中搭建足够宏伟的金字塔. 我不清楚看这篇文章的人, 是否经历过 `MFC` 那个年代的摧残, `MFC` 那会, 也就是***匈牙利命名大法***, 全部大写开头, 看着看着, 心情便沉入了谷底. 然而, 当我们进入`C++11`的年代以后, 整个世界突然亮了, 标准库的线程可以这样快速启动

```c++
std::thread([&]{}).detach();
```

​      而假使改成

```c++
STD::Thread([&]{}).Detach();
```

​      我不知道各位觉得哪个好,那么我们在来比对下一个

```c++
std::this_thread::sleep_for(std::chrono::milliseconds(100));// c++标准库
STD::ThisThread::SleepFor(STD::Chrono::MilliSeconds(100));  // MFC时代
Std::ThisThread::sleepFor(Std::Chrono::MilliSeconds(100));  // Qt时代
```

​    依次类推三个单词, 或者4个, 5个单词拼凑的一个类或函数时, 效果将更加明显, 驼峰给了人划分的依据, 但驼峰驼峰一坨坨, 看着还是沉重, 所以我在这里也做了一个结合, 外部接口参考Qt的风格, 内部接口参考标准库的风格, 因为内部接口必然是内部可控的, 也就不存在歧义的问题, 标准库只需严格规范`namespace` 就不会出现***想当然***上的误解, 而`namespace`本身就应该是被严格规范的, `std`本身就不应该被`using namespace`, 为了节省这几秒钟的输入我不清楚对各位有什么好处, 大部分时候我更倾向于认为: ***你该不会是抄的网上的demo吧?***

​    作用域名, 应该被严加限制, 尤其是导出类, ***必须 ! 必须! ! 必须 !!! 添加作用域名 !!!!*** 

​    当然, 沉重就一定不好吗? 并不是, 很多时候, 我么也可以利用这份沉重, 例如静态函数, 那么便全部大写开始, 例如:

```c++
static int GetStaticID(); // 这么沉重, 一看适合全局的/静态的
int  getParentId();       // 这就适合某类/结构体/C的接口
int get_thread_id();      // 适合内部工具库 或者 protected/private的接口
```

​     同时, 还可以延伸至其他命名等相关



​     ***当然, 说了这么多, 其实最重要的, 是形成统一的规范, 如果直接使用Google命名规范, 你绝对不可能会有被祭天的风险, 甚至还能在别人祭天时添砖加瓦. 如果你不清楚或者不认同, 那么我建议你直接使用Google命名规范来规范代码, 这是个万金油的选项.***

***Google命名规范中文地址: https://zh-google-styleguide.readthedocs.io/en/latest/google-cpp-styleguide/***



## 1. 文件命名规范

1. 业务文件均采用驼峰式命名, 以大写开头, 例如: `DataManager.h`/ `DataManager.cpp`
   也可采用全小写, 如 `datamanager.h`/ `datamanager.cpp`
2. 通用工具文件均采用小写命名, 以 `_` 间隔, 尽可能封装为`hpp`文件, 例如: `timer_manager.hpp`



入口文件命名区分3种形式:

1. 采用公司单例封装的进程;
2. 从main函数进入的进程;
3. 导出库;



### 1.1 采用单例封装的进程

​    对于该方案, 由于其入口已被封装, 入口文件名称与项目名保持一致.

​    例如有项目名为`DasClient`, 则其入口为`DasClient.h`/`DasClient.cpp`

### 1.2 从main函数进入的进程

​     对于该方案, 其入口文件名称始终为`main.cpp`

### 1.3 导出库

​    对于该方案,  入口文件名称与项目名保持一致.

​    例如有项目名为`EncodeBinary`, 则其入口为`EncodeBinary.h`/`EncodeBinary.cpp`



## 2. 头文件

### 2.1 引用顺序

​    引用顺序应遵从可修改等级依次引用

​    首先是本地头文件, 如`#include "EncodeBinary.h"`  <u>*(它也可以划归入末尾的 本地其他业务文件 的行列)*</u>

​    其后包含标准库文件, 如`#include <string>`

​    其后包含三方库文件, 如`#include <boost/asio.hpp>`

​    其后包含本地通用工具文件, 如`#include "utility/timer_manager.hpp"`

​    其后包含本地其他业务文件, 如`#include "other.h"`



### 2.2 注意事项

1. 必须使用` #pragma once` 或者 `#ifdef XXX` 进行包裹; 
   define格式如下: `<PROJECT>_<PATH>_<FILE>_H_`
   例如: 项目 `foo` 中 `src/bar/baz.h  `

   ```c++
   #ifndef FOO_BAR_BAZ_H_
   #define FOO_BAR_BAZ_H_
   ...
   #endif // FOO_BAR_BAZ_H_
   ```

2. 尽可能避免前置声明;

3. 只有当函数只有 10 行甚至更少时才将其定义为内联函数(注意:滥用内联将导致程序变得更慢, 析构函数中的操作也需考虑进去. 而`-O3`优化下编译器会自动扩展为内联), 包含循环或switch的函数不得定义为内联;

4. **文件路径严格禁止过于依赖隐藏路径**;
    例如: 引用头文件,  `google-awesome-project/src/base/logging.h`
    
    ```c++
    #include "base/logging.h"
    ```
    
    
    尽量避免使用"./"或者"../",  **确定的通用工具库**  的除外



## 3. 命名空间

​    鼓励在 `.cpp` 文件内使用匿名命名空间或 `static` 声明, 但禁止在`.h`文件中无意义的使用. 使用具名的命名空间时, 其名称可基于项目名或相对路径.



### 3.1 注意事项

1. **导出库必须声明命名空间**

2. 头文件中任何地方**不得使用**以下形式

   ```c++
   using namespace std; 
   namespace baz = ::foo::bar::baz;
   ```

   

3. 禁止用内联命名空间(inline namespace)

4. 不要在命名空间 `std` 内声明任何东西, 包括标准库的类前置声

5. 在**非通用工具**文件中, 例如进程的业务文件, 单纯为了封装静态函数应采用

   ```c++
   namespace myproject {
     namespace foo_bar {
       void Function1();
       void Function2();
     }  // namespace foo_bar
   }  // namespace myproject
   ```

   而不要采用

   ```c++
   namespace myproject {
     class FooBar {
      public:
       static void Function1();
       static void Function2();
     };
   }  // namespace myproject
   ```



## 4 结构体与类

### 4.1 命名

   1.  首字母大写, 不包含下划线: 如:`MyExcitingClass`, `MyExcitingEnum`, `MyExcitingStruct`.
   2.  模板类中除`Args`参数外, `typename`以`T`开始, 如:`template<typename TType, typename.... Args>`.



### 4.2 注意事项

1. 结构体中可不定义默认构造函数,通过`{0}`来进行初始化;

   ```c++
   struct St{
       int i_;
       int j_;
   }
   ...
   St tmp = {0};
   ```

2. 数据成员应均设置为`private`, 即使是例如`GlobalConfigManager`这样的全局配置,也应该尽可能改为私有, 而外部使用时采用`get`/ `set`的形式获取;

   ```c++
   class GlobalConfigManager {
   public:
       // 获取命名
       inline const std::string& getName() const {
           std::unique_lock<std::mutex> lock(m_mtx);
           return m_name;
       }
       // 设置命名
       inline void setName(const std::string& name) {
           std::unique_lock<std::mutex> lock(m_mtx);
           m_name = name;
       }
       
   protected:
       // 获取当前ID
       inline int getId() const {
           return m_id;
       }
       // 设置当前ID
       inline void setId(int id) {
           m_id = id;
       }
       
   private:
       // 数据安全锁
       mutable std::mutex m_mtx;
       // 命名
       std::string        m_name;
       // 当前ID
       int                m_id;
   }
   ```

3. 声明顺序通常以`public` -> `protected` -> `private`的顺序,当然这也并非严格顺序, 当出现内部类等情况下,通常内部类在上层,例如:

   ```c++
   class A{
   protected:
       struct inner_st_a{};
       struct inner_st_b{};
   public:
       A(){}
       ...
   }
   ```

4. 已弃用对象,但为了不影响原接口,必须包含弃用说明

   ```c++
   // DEPRECATED(AChar): 已弃用,改为使用XXX
   struct [[deprecated]] S{};
   struct S{[[deprecated]] int n;};
   enum [[deprecated]] E {};
   enum { A [[deprecated]], B [[deprecated]] = 42 };
   ```

   



## 5. 函数

### 5.1 命名

1. 静态函数均首字母大写;

   ```c++
   static int GetId();
   ```

   

2. 类/结构体函数首字母小写, `get`/`set`成对出现, 注意添加`const`;

   ```c++
   std::string getName() const;
   void setName(const std::string& name);
   ```

   通用工具文件可均采用 小写+下划线 的形式,使得更贴近标准库风格

   ```c++
   std::string get_name() const;
   void set_name(const std::string& name);
   ```

   


### 5.2 注意事项

1. 函数功能需要尽可能单一;

2. 重载操作符时需要注意尽量避免出现异常行为;

3. 除`helper`这些辅助操作以外,应尽量减少隐式转换的重载;

4. 返回值若为结构体/类内部对象的引用, 务必加上`const`, 模块内的参数修改不得抛出给外界随意修改, 如果需要修改, 应通过`set`函数去修改, 性能问题请务必不要在这里考虑, 代码量少的情况下, `O3`时默认就给你改成内敛了, 即使你没有添加`inline`标志, 切记! 类即边界, 类内的参数, 其边界就在类内部, 不得跨越处理, 除非是一个各模块间通用的结构体, 那么此时请务必使用`shared_ptr`去使用, 对其数值存在多线程修改的, 请务必使用`handler`类对该结构体进行封装, 模块间传递的为`shared_ptr<handler>`对象!!

   ```c++
   #include <iostream>
   #include <memory>
   
   struct St {
       std::string name_;
       // ... 其余参数
   };
   
   class StHandler {
   public:
       const std::string& get_name() const {
           return m_st.name_;
       }
       void set_name(const std::string& name) {
           m_st.name_ = name;
       }
   private:
       St m_st;
   };
   
   class ModelA {
   public:
       ModelA(const std::shared_ptr<StHandler>& handler):m_handler(handler){}
   private:
       std::shared_ptr<StHandler> m_handler;
   };
   class ModelB {
   public:
       ModelB(const std::shared_ptr<StHandler>& handler) :m_handler(handler) {}
   private:
       std::shared_ptr<StHandler> m_handler;
   };
   
   int main() {
       auto handler = std::make_shared<StHandler>();
       ModelA a(handler);
       ModelA b(handler);
       return 0;
   }
   ```

   千万不要为了贪图编写的一时之快,  否则 **写时一时爽,  事后火葬场**

5. 已弃用函数,但为了不影响原接口,必须包含弃用说明

   ```c++
   // DEPRECATED(AChar): 已弃用,改为使用getUserId
   [[deprecated]] int getId();
   ```

   

## 6. 变量

### 6.1 命名

   1. 结构体中,应以 `_` 结尾

      ```c++
      struct St{
          int i_;
          int j_;
      }
      ```

2. 类成员变量中,应以 `m_` 开始

   ```c++
   class A{
   private:
       int m_i;
       int m_j;
   }
   ```

3. 全局变量应以 `g_` 开始

   ```c++
   const int g_count = 0;
   ```

4. 常量变量应以 `c_` 开始

5. 静态变量应以 `s_` 开始

6. 临时变量采用 `小写` 或  `小写` + `_`  形式

   ```c++
   void func(){
       auto tablename = getName();
       auto table_name = getName();
       ...
   }
   ```



### 6.2 注意事项

1. 已弃用参数,但为了不影响原接口,必须包含弃用说明

   ```c++
   // DEPRECATED(AChar): 已弃用,改为使用y
   [[deprecated]] int x;
   ```





## 7. 其他

### 7.1 引用参数

1. 参数传递中, 若为返回值应尽可能采用引用,而非指针,指针则意味着必须进行判断,而引用则更加安全,通常情况下,在跨语言导出库等这些特殊情况下,才会传入传入`const T*`, 正常情况下,即使需要指针,也是类似`shared_ptr`这样的对象进行传递, 若用 `const T*` 说明输入另有处理. 应有理有据, 否则会害得读者误解. 例如:

   ```c++
   bool add(int a, int b, int& resault);
   ```

   同时,在大部分情况下,我们可以返回`tuple`来达到多参数返回的目的,例如:

   ```c++
   std::tuple<bool/*ok*/, std::string/*errmsg*/, int /*resault*/> add(int a, int b);
   ...
   bool ok;std::string errmsg;int resault;
   std::tie(ok, errmsg, resault) = add(1, 2);
   auto [ok2, _, resault2] = add(1, 2);
   ```
   
   不过值得注意的是, `std::forward_as_tuple`并不会严格执行`forward`,而是需要显示调用`std::forward`来实现

2. 大部分情况下, 参数增加`const`并不会带来很大的开销,但是若参数为`int`/`char`等编译器参数类型,则无需追加`const`标志,因为其开销可以忽略, 加入`const`的开销甚至大于`int`拷贝操作本身, 例如:

   ```c++
   void func(int a, const std::string& s);
   ```



### 7.2 右值引用

1. 右值引用的引入, 首先需要明确一个定位: **为了解决可读性与便利性, 提供深浅拷贝自主选择时机**

   没有经历过`VC6`摧残的各位, 请思考一个问题, 在有且仅有一个拷贝构造的情况下, 如何实现有可能深拷贝, 有可能浅拷贝, 例如:

   ```c++
   #include <iostream>
   #include <vector>
   class A {
   public:
       A(std::vector<int> vec) :m_vec(vec) {
           std::cout << "1" << std::endl;
       }
       A(const A& rhs) {
           std::cout << "2" << std::endl;
           // 在这里自定义实现深拷贝或者浅拷贝, 通常都是深拷贝, 在以前浅拷贝的均禁止拷贝
           m_vec = rhs.m_vec;
       }
   private:
       std::vector<int> m_vec;
   };
   
   int main() {
       std::vector<int> vec(10, 1);
       A a1(vec);
       A a2 = a1;//  深拷贝, vector 2次memcpy
       
       std::cout << "-------" << std::endl;
   
       A* a3 = new A(vec);
       A* a4 = a3;// 浅拷贝, vector 1次memcpy
       return 0;
   }
   ```

   在C++11之前, 这个时候是没有移动构造的, `stl`容器的默认也都是深拷贝, 也就是说`vector`的拷贝构造后会存在2份`vector`内存对象, 想要只保留一份, 就只能使用指针, 而指针就存在使用的各种繁琐判断, 至少要判空吧, 而且存在析构时机等各种小细节.那个时候编译器还无法自行优化返回, 所以深浅拷贝的选择只能通过返回的是对象还是指针的形式来实现;

   那么为了更加优雅的实现, 我们只需对应问题对应解决就好了, 问题的核心在于无法区分深拷贝还是浅拷贝, 那么我们就再加个构造类型, 名曰: 移动构造, 其区分便是右值引用.

   ```c++
   class A {
   public:
       A(const A& rhs) {
           std::cout << "2" << std::endl;
           m_vec = rhs.m_vec;
       }
       A(A&& rhs) {
           std::cout << "3" << std::endl;
           m_vec = std::move(rhs.m_vec);
       }
   private:
       std::vector<int> m_vec;
   };
   ```

   **注意: **

   ​     **`&`是引用, 只能引用左值 **

   ​     **`&&`是右值引用, 可以引用左值也能引用右值, 引用左值时, 相当于左值**

   ​     **`const &`是万能引用, 可以是引用, 也能是右值引用**

   ​     **`std::move`是函数, `std::move`的作用是将左值或右值转化为右值, **

   而左值与右值的区别在于是否具名, 或者叫是否可寻址, 例如:
        int i = 22; 
        此时有个名字叫i的变量, i有名字(可以寻址), 能够被赋值, 就是左值.
        22在编译时没有名字, 你无法改变22的数值, 就是右值.
   右值引用, 则表示引用了一个不可赋值的右值

   ```c++
   int x = 6; // x是左值, 6是右值
   int &y = x; // 左值引用, y引用x
   
   int &z1 = x * 6; // 错误, x*6是一个右值
   const int &z2 =  x * 6; // 正确, 可以将一个const引用绑定到一个右值
   
   int &&z3 = x * 6; // 正确, 右值引用, 但是右值又被赋予了z3这个名字, 所以z3是右值引用的左值化
   int &&z4 = x; // 错误, x是一个左值
   ```

   搞清楚了左值与右值, 我们才能搞清楚他什么时候是进入左值什么时候进入右值.最简单的就是
           有名字, 左值
           没名字, 右值
   我们在来分析下, 下面的代码:

   ```c++
   #include <iostream>
   #include <thread>
   
   struct B {
       static int count_;
       int j_;
       B() {
           j_ = ++count_;
           std::cout << "B()" << "  " << j_ << std::endl;
       };
       B(B&& rhs) {
           j_ = ++count_;
           std::cout << "B&&" << "  " << j_ << std::endl;
       };
       B(const B& rhs) {
           j_ = ++count_;
           std::cout << "const B&" << "  " << j_ << std::endl;
       }
       ~B() {
           std::cout << "~B()" << "  " << j_ << std::endl;
       }
   };
   int  B::count_ = 0;
   
   void func2() {
       B b1;                       // 1) 生成左值对象b1
       std::thread([](B&& b2){     // 3) 传入thead内部对象备份的右值(备份的原语义为右值), 在进入此处时
                                   //    将右值对象具名,进行左值化, 名为b2, b2从有名字开始即为左值
                                   //    之后想要使用b2的右值引用, 需要std::forward转义获取
                                   //    move是强制转为右值, forward则是获取原语义
           std::this_thread::sleep_for(std::chrono::seconds(1));
           std::cout << "use B:" << b2.j_ << std::endl;
       }, std::move(b1)).detach(); // 2) 首先进入move,将左值b1转为右值引用
                                   //    然而, thead内部存在转换, 存储至临时变量中, 所以
                                   //    会进入移动构造中, 创建新的对象备份, 同时赋予其名字左值化
       std::cout << "!!!!!!!!" << std::endl;
   }
   int main() {
       func2();
       std::this_thread::sleep_for(std::chrono::seconds(2));
       return 0;
   }
   ```

   所以输出结果为:

   ```c++
   B()  1
   B&&  2
   !!!!!!!!
   ~B()  1
   use B:2
   ~B()  2
   ```

   那么我们可以看到, `move`就是一个引用, 甚至他的开销还比普通的引用大, 通常而言, 
       **`const &` 开销  <  `rvo`开销  <  `&&` 开销 **

   那为什么大家还在说, `std::move`可以节省开销呢? 我们在来回想一下右值引用出现的目的, 区分于传统`const &`, 给大家一个浅拷贝的选择入口, 那么对于`string` / `vector`等等为代表的的, 内部包含内部内存管理的对象/容器而言, 右值就是一个很好的选择, 在`std::move`右值引用出现之前, 你会发现有大量的禁止拷贝操作, 为什么? 防止内存不受管控, 两个对象管控同一块区域, 那他析构时还要不要释放?

   所以, 右值给了大家一个主动选择浅拷贝的快速入口, 可以这么去写:

   ```c++
   #include <iostream>
   struct MemoryManager {
       MemoryManager(const std::string& s) {
           m_array = new char[s.length()+1];
           memcpy(m_array, s.c_str(), s.length() + 1);
       }
       ~MemoryManager() {
           if (m_array)
               delete[] m_array;
       }
       MemoryManager(const MemoryManager& rhs){
           m_array = new char[strlen(rhs.m_array) + 1];
           memcpy(m_array, rhs.m_array, strlen(rhs.m_array) + 1);
       }
       MemoryManager(MemoryManager&& rhs){
           m_array = rhs.m_array;
           rhs.m_array = nullptr;
       }
   
       char* m_array;
   };
   
   int main() {
       std::string ss = "123456";
       MemoryManager a(ss);
       std::cout << "a 地址: " << &a.m_array << "; 内容: " << a.m_array << std::endl << std::endl;
       MemoryManager b = a;
       std::cout << "b 拷贝构造a之后: " << std::endl;
       std::cout << "a 地址: " << &a.m_array << "; 内容: " << a.m_array << std::endl;
       std::cout << "b 地址: " << &b.m_array << "; 内容: " << b.m_array << std::endl << std::endl;
       MemoryManager c = std::move(a);
       std::cout << "c 移动构造a之后: " << std::endl;
       std::cout << "a 地址: " << &a.m_array << "; 内容: " << (a.m_array == nullptr ? "null" : a.m_array) << std::endl;
       std::cout << "b 地址: " << &b.m_array << "; 内容: " << b.m_array << std::endl;
       std::cout << "c 地址: " << &c.m_array << "; 内容: " << c.m_array << std::endl << std::endl;
       return 0;
   }
   ```

   输出结果为

   ```shell
   a 地址: 0000000A8BAFF7C8; 内容: 123456
   
   b 拷贝构造a之后:
   a 地址: 0000000A8BAFF7C8; 内容: 123456
   b 地址: 0000000A8BAFF7E8; 内容: 123456
   
   c 移动构造a之后:
   a 地址: 0000000A8BAFF7C8; 内容: null
   b 地址: 0000000A8BAFF7E8; 内容: 123456
   c 地址: 0000000A8BAFF808; 内容: 123456
   ```

   是的, 右值引用当且仅当

   1. **`trivial`对象**    或     **非`trivial`对象, 且提供了自定义的移动构造函数(若未提供则依旧左值化, 进入拷贝构造函数中进行构造)**
   2. 你的移动构造中可以节省部分`memcpy`操作, 仅仅只是将内存的控制权转移(内含`string`/`vector`等变量进行`move`时, 系统默认提供的是存在优化的);

   只有满足上述两个条件的情况下, 才会起到优化作用, 否则, 其开销甚至大于`const &`
   左值化的`abi`映射参考:https://zhuanlan.zhihu.com/p/355397487

   

2. 第二个使用场景, 则是唯一性对象的作用域切换, 部分唯一性对象存在不可拷贝的情况, 那么此时也可使用 `std::move` 去切换,例如: 

   ```c++
   void func(std::unique_ptr<A> u2){
   	//...
   }
   std::unique_ptr<A> u1;
   func(std::move(u1));
   ```

3. 第三则是临时对象的内存返回, 延长生命周期

   ```c++
   #include <iostream>
   #include <vector>
   
   struct A {
       int len_;
       int* vec_;
   };
   
   std::tuple<A, std::vector<int>> create_A() {
       std::vector<int> vec;
       vec.emplace_back(1);
       A a;
       a.len_ = vec.size();
       a.vec_ = &vec[0];
       std::cout << &vec[0] << std::endl;
       return std::forward_as_tuple(std::move(a), std::move(vec));// 注意forward_as_tuple依旧需要指定std::move
   }
   
   int main() {
       auto [a, vec] = create_A();
       std::cout << &vec[0] << std::endl;
       return 0;
   }
   ```

   


### 7.3 vector

1. reserve
   如果`vector`中要添加的元素数量已知, 那么在添加元素前使用reserve函数预分配`vector`需要的内存.
   这样可以避免由于`vector`内部发生数据迁移而带来的不必要的元素拷贝开销/创建新内存开销, 和释放旧内存开销. **提前reserve带来的性能优化是显著的, 仅次于folly的自实现small_vector**

   ```c++
   #include <vector>
   constexpr int g_count = 10000;
   
   struct Test {};
   
   void poor(std::vector<Test>& data) {
       for (int i = 0; i < g_count; i++) {
           data.emplace_back(Test());
       }
   }
   
   void better(std::vector<Test>& data) {
       data.reserve(g_count);
       for (int i = 0; i < g_count; i++) {
           data.emplace_back(Test());
       }
   }
   
   int main()
   {
       std::vector<Test> test_1;
       poor(test_1);
   
       std::vector<Test> test_2;
       better(test_2);
       return 0;
   }
   ```
   
2. resize(0)
   `vector`的`resize(0)`在大数据量下优于`clear()`, 不过区别不大

3. 批量删除其中某类子项时采用std::remove_if, 而不要使用迭代器一个一个删除

4. 批量插入使用`insert`而不要使用`emplace_back`一个一个插入

5. 迭代器访问速度 > 下标访问速度 > `at`访问速度, 任何时候不需要使用`at`



### 7.4 友元

1. 之前提过, 类的成员变量通常均应该声明为 `private`, 然而, 有时我们又不可避免的会存在一些联动类或结构体之间的访问, 虽然外界依旧不会访问, 或提供外界类似 `helper` 的操作对象, 那么此时我们可以使用友元来实现该功能, 但是切忌将友元抛出至第二个文件内, 友元必须严格限制在单文件内, 否则后续的维护成本将成指数性放大. 通常而言, 若无特殊需求, 也应该限制在类自身内部, 例如:

   ```c++
   class A{
   public:
       class inner_helper{
       public:
           inner_helper(A* parent):parent_(parent){}
           void do(){ parent_->do(); }
       private:
           A*  parent_;
       };
       friend class inner_helper;
       
   private:
       void do(){
           // ...
       }
   }；
   ```



### 7.5 异常

​    *我不建议在 非与三方对接时(因为第三方不可控) 使用 C++ 异常, 异常不应该出现在完全自主可控的逻辑代码中*

​    异常虽然确保了程序的稳健, 然而其带来的开销, 以及被隐藏起来的隐患确是更为致命的. 

​    我们应确保程序中的错误均为已知错误,  任何未知的错误即使将其捕获, 亦会造成不可估量的后果, 同时会造成间隔非常长的时间后才发现问题, 例如,2021.1.1日程序发生未知错误, 然而该异常被捕获. 程序一直以未知异常状态持续运行, 然后可能是1个月, 可能是半年, 突然发现了某个数据异常, 此时, 你将面对的是海量的日志数据查询, 以及无穷无尽的BUG修复, 更有可能由于无法定位问题而出现错误修改, 例如一些对代码不熟悉的新人, 在接到修复任务时, 面对海量数据而无从下手, 此时只能随意更改猜测段代码来完成交付, 而这种异常通常又难以浮现, 等到下次出现时, 可能也早已忘记上次的错误更改的地方了, 或者又引入新的BUG.

​    所以, 通常而言, 我不建议代码中引入异常, 这一点Go语言也一直在避免, 只能在上层的recover中捕获, 而不是如JAVA那般. 对于 `new` 操作失败的可能, 我建议直接忽略, 大内存的数据块应尽量避免, 无法避免应通过封装使其主动返回错误,  对于内存资源占用的特别大的服务进程, 应加装一层监管告警服务, 当出现过高的内存占用时, 应通过高可用,负载均衡等各项手段避免其出现占满的情况.

​     然而, 不建议并不代表绝对不使用, 例如某些三方库, 其错误就是以异常的形式返回, 此时我们必须将其捕获异常, 来确保程序的正常运行.

​    而在确定没有异常的情况下, 我也建议在编译期加上 `-fno-exceptions` 标志, 来减少代码大小.

​    

### 7.6 运行时类型识别

​    *我们禁止在生产代码中使用 `RTTI`.*

​    `RTTI`建议仅在测试`demo`中输出返回, 该功能现已被元编程完全替代, 特化偏特化即可满足大部分需求, 而在C++17中的`constexpr`更是得到进一步的加强;  

​    备注: `constexpr`是真正的常量, `constexpr`修饰的`double`常量也是真.常量

​    而实际代码中, 更多接触的其实是`static_cast` 或者 `static_pointer_cast` 或者 `reinterpret_cast` 来指针类型的转化;

​     **注意**: 代码中禁止使用`dynamic_cast`与`dynamic_pointer_cast`, 虽然看起来似乎更安全, 但一旦出现返回为空也意味着你的程序本身就存在缺陷了, 而`dynamic_cast`与`dynamic_pointer_cast`这两个转换的性能却是直线下降, 它是以字符串逐个比对来实现的, 而一旦你使用了它, 则意味着假设你新增一个继承类的时候,必须`review`之前使用的地方, 是否会造成影响. 所以, 我们应该通过代码本身设计来规避转换的问题, 而不是使用这个性能极低的所谓安全函数来实现. 

​    大部分时候, 可以通过工厂方式, 虚函数等形式规避掉`dynamic_cast`与`dynamic_pointer_cast`的使用



### 7.7 注释

​    注释有很多中方案, 但是尽可能选取一种通用方案



1. 文件注释

   ```c++
   /*
   * Copyright (c) ArchForce Corp. 2021-2031. All Rights Reserved
   * Authors: AChar, 2021.09.23
   * Version: 1.0.0.0
   * Purpose: 提供统一文件头说明
   * Note:    这个是样板,以下参考了btool的atomic_switch.hpp文件的说明  
   * Example: 
      例如:只希望启动一次,但可能存在启动延时的情况;此时通过判断m_init的情况即可避免重复进入;
                  AtomicSwitch judge;
                  if (!judge.init())
                      return false;
                  // 其他初始化操作
                  if (!judge.start())
                      return false;
      例如:只希望停止一次,但可能存在终止延时的情况;此时优先将m_bstop置位,其次待释放结束后修改m_init标识防止重复进入;
                  AtomicSwitch judge;
                  if (!judge.stop())
                      return false;
                  // 其他释放操作, 下次启动必须调用init
                  judge.reset();
                  return true;
   */
   ```

   

2. 注释`//`

   ```c++
   // Google风格中存在Example
   // 但我认为Example不要放在类中,有的Example小百行,
   // 就算是VS他也显示不了那么多,统一放到文件说明中去
   // 除非是同一个.h中含有多个类, 且每个类的Example都十分简短
   // 也不要使用///去表示, 那个是C#或者doxygen的风格, 胡乱使用在C++中会显得特别暗
   // 事实上即使是doxygen的风格中, 我也建议采用//!这种方案
   class Test {
     ...
   };
   ```

   

3. 注释`/* */`

   ```c++
   /**
   * Google风格中存在Example
   * 但我认为Example不要放在类中,有的Example小百行,
   * 就算是VS他也显示不了那么多,统一放到文件说明中去
   * 除非是同一个.h中含有多个类, 且每个类的Example都十分简短
   * 也不要使用///去表示, 那个是JAVA或者C#或者doxygen的风格, 胡乱使用在C++中会显得特别暗
   * 事实上即使是doxygen的风格中, 我也建议采用//!这种方案
   */
   class Test {
     ...
   };
   ```

   

4. `doxygen`风格(详细参考 https://www.doxygen.nl/manual/docblocks.html#cppblock )
   4.1 文件注释

   ```c++
   /**
    * @file Test.h
    * @brief 测试头文件
    * @details 这个是测试Doxygen
    * @mainpage 工程概览
    * @author AChar
    * @email AChar@af.local
    * @version 1.0.0
    * @date 2021-09-23
    * @license ArchForce
    */
   ```

   4.2 类定义

   ```c++
   /**
    * @brief 测试类
    * 主要用来演示Doxygen类的注释方式
    */
    class Test{
    };
   ```

   4.3 变量定义

   ```c++
   //! 缓存大小
   #define BUFSIZ 1024*4
   //!< 缓存大小
   #define BUFSIZ 1024*4
   /** 以该点结束的简要说明. 详情如下
    * 详细说明从这里开始. 
    */
   #define BUFSIZ 1024*4
   
   #define BUFSIZ 1024*4 //! 缓存大小
   #define BUFSIZ 1024*4 //!< 缓存大小
   ```
   
   4.4 函数注释
   
   ```c++
   /**
   * @brief 主函数
   * @details 程序唯一入口
   *
   * @param argc 命令参数个数
   * @param argv 命令参数指针数组
   * @return 程序执行成功与否
   *     @retval 0 程序执行成功
   *     @retval 1 程序执行失败
   * @note 注解,这里只是一个简单的例子
   * @attention 注意,这里是注意
   * @warning 警告,这里是警告
   * @exception 异常,这里是异常
   */
   int main(int argc, char* argv[]){
       
   }
   ```
   
   4.5 其他
   
   |   **命令**    | **生成字段名** |            **说明**            |
   | :-----------: | :------------: | :----------------------------: |
   |    `@see`     |      参考      |                                |
   |   `@class`    |     引用类     |        用于文档生成连接        |
   |    `@var`     |    引用变量    |        用于文档生成连接        |
   |    `@enum`    |    引用枚举    |        用于文档生成连接        |
   |    `@code`    |   代码块开始   |      与`@endcode`成对使用      |
   |  `@endcode`   |   代码块结束   |       与`@code`成对使用        |
   |    `@bug`     |      缺陷      |  链接到所有缺陷汇总的缺陷列表  |
   |    `@todo`    |      TODO      | 链接到所有TODO 汇总的TODO 列表 |
   |  `@example`   |  使用例子说明  |                                |
   |  `@remarks`   |    备注说明    |                                |
   |    `@pre`     |  函数前置条件  |                                |
   | `@deprecated` |  函数过时说明  |                                |
   
5. 弃用注释

   ```c++
   // DEPRECATED(AChar): 已弃用,改为使用XXX
   struct [[deprecated]] S{};
   struct S{[[deprecated]] int n;};
   enum [[deprecated]] E {};
   enum { A [[deprecated]], B [[deprecated]] = 42 };
   
   // DEPRECATED(AChar): 已弃用,改为使用y
   [[deprecated]] int x;
   
   // DEPRECATED(AChar): 已弃用,改为使用getUserId
   [[deprecated]] int getId();
   ```

### 7.9 列表初始化

cheap talk

```c++
struct Foo{
    Foo(int i) { std::cout << i << std::endl; }
};
Foo foo(1.2);
```

众所周知, `i`肯定会将浮点数`1.2`给截断, 那么这个过程就是类型收窄的过程;

通常情况包括

1. 从一个浮点数隐式转换为一个整型数, 如 `int i=2.2`
2. 从高精度浮点数隐式转换为低精度浮点数, 如从 `long double` 隐式转换为 `double` 或`float`
3. 从一个整型数隐式转换为一个浮点数, 并且超出了浮点数的表示范围, 如 `float x=(unsigned long long)-1`
4. 从一个整型数隐式转换为一个长度较短的整型数, 并且超出了长度较短的整型数的表示范围,如 `char x=65536`

使用列表初始化则可以直接在编译阶段发现这些问题

```c++
int a = 1.1; // OK
int b = { 1.1 }; // error

float fa = 1e40; // OK
float fb = { 1e40 }; // error

float fc = (unsigned long long)-1; // OK
float fd = { (unsigned long long)-1 }; // error
float fe = (unsigned long long)1; // OK
float ff = { (unsigned long long)1 }; // OK

const int x = 1024, y = 1;
char c = x; // OK
char d = { x }; // error
char e = y; // OK
char f = { y }; // OK
```

其中需要注意的是` x/ y` 被定义成了 `const int`. 如果去掉 `const` 限定符, 那么最后一个变量 `f` 也会因为类型收窄而报错. 

灵活运用`std::initializer_list`可以在很多时候更加优美安全的实现我们的功能, 例如:

```c++
#include <vector>
#include <map>

class FooVector {
public:
    FooVector(const std::initializer_list<int>& list) {
        m_vec.insert(m_vec.end(), list.begin(), list.end());
    }
private:
    std::vector<int> m_vec;
};

class FooMap {
    using pair_t = std::map<int, int>::value_type;
public:
    FooMap(const std::initializer_list<pair_t>& list) {
        for (auto iter = list.begin(); iter != list.end(); ++iter){
            m_map.emplace(*iter);
        }
    }
private:
    std::map<int, int> m_map;
};

int main() {
    FooVector foo_1 = { 1, 2, 3, 4, 5 };
    FooMap foo_2 = { { 1, 2 }, { 3, 4 }, { 5, 6 } };
    return 0;
}
```

不过需要注意的是, 并非任何时候均可使用:

```c++
struct A {
    int x;
    int y;
} a = { 123, 321 };         // a.x = 123, a.y = 321
struct B {
    int x;
    int y;
    B(int, int) : x(0), y(0) {}
} b = { 123, 321 };         // b.x = 0, b.y = 0
struct St1 {
    int x;
    double y;
    int z;
    St1(int, int) {}
};
//St1 st1{ 1, 2.5, 1 };     // 存在用户自定义构造函数, 且无法匹配, 错误
struct St2 {
    int x;
    int y;
    int z;
    St2(int, int, int) {}
};
St2 st2{ 1, 2, 3 };         // st2.x / st2.y / st2.z 未定义, 实际调用的是自定义构造函数St2(int, int, int)
struct St3 {
    int x;
    int y;
    int z;
};
St3 st3{ 1, 2, 3 };         // st3.x = 1, st3.y = 2, st3.z = 3
struct St4 {
    int x;
    double y;
protected:
    int z;
};
//St4 st4{ 1, 2.5, 1 };     // 存在protected/private参数, 错误
struct St5 {
    int x;
    double y;
protected:
    static int z;
};
St5 st5{ 1, 2.5 };          // protected参数为静态参数, 静态参数再静态区即生成, 所以没有问题
//St5 st5_2{ 1, 2.5, 1 };   // protected参数无法在此处初始化, 错误
struct St6 {
    int x;
    double y;
    virtual void F() {}
};
//St6 st6{ 1, 2.5 };        // 存在虚函数,错误

struct Base {};
struct St7 : public Base {
    int x;
    double y;
};
//St7 st7{ 1, 2.5 };        // 存在基类, 错误
```



### 7.10 导出类

1. 不要在导出文件中使用`vector`等标准库容器
   标准库是会更新的, 更重要的是各大厂商实现是不一样的, 你的 `gcc` 版本中的`vector`和对方使用的版本的`vector`可能就不是一个东西, 例如`C++11`之前与之后, `C++11`之后全部改成模板实现了, 他们有相同的名字, 但却完完全全不是一个东西
   而且使用之后还必须区分`release`/`debug`, 造成无法复用, 每次发给别人都得贴心的高速他们, 只能`release`哦, 想调试你就`release`下`O0`编译嘛, 你猜你会不会打喷嚏, 所以切忌使用! 
   内部项目也不要, 哪天换个领导, 发现新版`gcc`比旧版同样代码下性能就是更高, 就顺手给你升级了, 作为一个导出类, 不要去限制别人的使用方式

2. 不要在导出文件中使用`string`/`shared_ptr`等涉及引用计数的容器
   除了上述原因外, `string`在`C++11`以后是一个智能指针, 我们看下以下代码:

   ```c++
   std::string a = "123";    // 存在"123"的引用1次, 对象是a
   std::string c = a;        // 存在"123"的引用2次, 对象是a, c
   a = "456";                // 存在"123"的引用1次, 对象是c
                             //     同时"456"的引用1次, 对象是a
   ```

   ​    `string`是发生改变时才会切换, 此处的样例由于是常量赋值的, 不存在析构问题, 假设是动态生成的, 那么即可能存在内存析构的问题, 什么时候析构, 时机由`string`控制, 而导出库的限制又使得内存的析构无法跨越, 极其容易造成误用

   ​    **注意:你的demo能跑是因为`string`/`vector`等是极少数不那么容易崩溃的`stl`容器, 但这并不意味着没有问题, 动态库内创建的内存, 任何时候不能让进程去析构, 同样道理, 进程创建的内存也不要让动态库去析构.**

   ​    如果实在无法理解, 那么请思考一个问题, 为什么编译器在你导出的时候, 会提示一个`Warning`, 为什么不提示或者不是`Error`, 千万不要对导出类的使用做任何的限制, 这将在团队协作中, 不经意间给你很多的惊吓.

3. 错误码千万千万不要复用同一错误码, 然后在通过另外的`type`标志来区分, 这样做极其恶心人, 定位问题时通常也是恶心两次.

   ```c++
   enum ErrcodeType1 : int {
       ok = 0,
       err1 = 1,
       err2 = 2
   };
   enum ErrcodeType2 : int{
       ok = 10,
       err1 = 1,
       err2 = 2
   };
   
   // 获取错误信息, 恶心之处在于每次都要两个参数
   std::string get_error_msg(int code, int type);
   ```

   为什么不直接通过其他手段直接区分呢?

   ```c++
   enum ErrcodeType : int {
       /*** 0 - 9999 为type1错误类型  ***/
       type1_ok = 0,
       type1_err1 = 1,
       type1_err2 = 2,
       
       /*** 10000 - 20000 为type2错误类型  ***/
       type2_ok = 10 * 10000,
       type2_err1 = 1 * 10000,
       type2_err2 = 2 * 10000
   };
   ```

   还有的同学说我放不下啊, 那就`int64_t`, 难道系统字段, 比你一个`type`还要复杂? 我们写代码要遵循2/8法则, 将20%的代码去处理80%的核心性能, 剩余的80%处理20%的边缘性能, 这80%的余量就是为了满足可读性, 易维护性, 扩展性等等等等, 否则直接汇编上去干他不香? 

   当然, 特殊需求, 版本兼容, 领导要求, 产品经理体格过于健壮的, 不做要求......

4. 动态库头文件之间不要互相包含
   你可以在动态库1中使用动态2, 但请不要在动态库1的头文件中, 包含动态库2的头文件, 这样......很不好......每个动态库都是独立的小天使, 请不要搞成连体婴......



### 7.11 编译

​    常见的为 `make -j8` 这样的多核编译参数, 编译线程不要过多, 通常为PC核心数一致, 编译执行时本身不要运行过于耗资源的进程

​    也可以加入`ccache`参数来加速编译, `linux`下需要提前安装一下, 类似`cmake`

```linux
sudo apt install ccache
```



```cmake
find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif(CCACHE_FOUND)
```

   

​     还可以使用 `IncrediBuild` 等进行多机集成编译, 当然如果你是在VS下, 可以使用VS来使用该工具, 在安装的时候添加 `IncrediBuild`即可.

​    

### 7.12 发布

​    进入2021年以后的各位, 请发布docker!

​    当个现代猿吧, 别再做古人猿了!

​    编译环境这些全部可以做成docker镜像,  先build一个编译环境的docker,

​    然后再在编译docker基础上, build一个发布docker,

​    还是觉得麻烦? 那就用以下脚本直接拿过去参考吧! 

​    [docker-auto-build.tar.gz](docker-auto-build.tar.gz) 

​    这个脚本会自动生成对应日期版本以及`lastest`版本, 按照对应日期即可, 默认采用的是阿里云的仓库, 可依据实际情况自行更改即可.



## 8 内存

### 8.1 虚拟内存和内存分页

**在开始零拷贝/低时延之前, 我们需要先了解下虚拟内存和内存分页:**

在内存刚造出来那会,  由于内存有限, 请思考一个场景:

你现在有个`2G`的内存, 当你创建一个进程, 这个时候你在内存中开辟了`1.5G`的内存, 然后不考虑任何其他情况下, 只剩下`0.5G`了, 恰好你的进程又在连续内存的中间释放了`1G`, 然后你是不是拥有不连续的总共`1.5G`的空闲内存;

这个时候来了进程2, 他想要申请`1.5G`内存, 让不让申请? 肯定得通过吧, 毕竟还有, 而且内存那么贵, 利用率得高才好, 对吧. 那这不连续啊, 我要`new char[1.5G]`, 你怎么能不让呢?

为了让大家更好的利用, 也为了让大家更好的管理, 各大操作系统就做了一个中间件, `MMU(Memory Management Unit)`内存管理单元, 把你想创建的内存(`你想要的`)和实际的物理内存(`他能给的`)分开, 然后内部关联起来;

你说你要`new char[1.5G]`, 可以! 我给你返回, 然后返回地址是`0x00001000`, 你从这之后就是你的连续内存地址了啊. 转头在和`CPU`沟通, 老哥, 开两块内存, 一个`1G`, 一个`0.5G`. 最后自己维护好这个映射表就好了(通过[9.8.1 `mmap`](#9.8.1 `mmap`)完成磁盘文件的映射, 通过`flush`刷新同步). 这样即使进程崩溃了, 他也可以将该进程申请的资源给安全的释放了.

这个就是**虚拟内存**, 并非在真实物理内存上开辟空间, 而是通过磁盘文件的映射形式, 完成模仿物理内存的方式

然后这个时候, 中间商`MMU`又发现了, 很多人的内存虽然要在那放着, 但是他不经常用啊, 十天半个月用一次, 反正使用者都是...也不针对哪一位, 我告诉他啥他就信啥, 那我在磁盘上开一张表, 用来存放那些不常用的, 不常用的也别占着物理内存这么贵的资源了, 就放磁盘文件里, 啥时候要用啥时候去读吧. 业务量这不就上来了吗?那这个就是**换页**

好吧, 那既然可以这么玩, 作为~~黑心中间商~~, 平台供应商的`MMU`, 不压榨完剩余价值怎么行? 那我是不是可以同时开多张, 开一张够本, 开两张赚翻, 老子给他开出行业天花板, 可怜的物理内存中只放最关键的常用的`8G`数据, 那这个就是**分页**

综上:

- 为了提高内存的使用率, 通过`MMU`用磁盘文件对实际物理内存碎片化管理, 以此防止进程碎片化的内存申请造成内存浪费;
- 同时开辟多张虚拟内存的映射文件来对进程内存分页, 以此达到扩展虚拟内存的效果;
- 通过内存换页, 将热点数据按使用率依次存放, 冷数据往后放, 来达到更高的缓存命中率

 到底是怎么匹配的? 请移步: https://blog.csdn.net/u012999985/article/details/107398986

### 8.2 内存分页带来`Page Cache`的命中率问题

由于内存存在分页换页, 那么当进程需要提取冷数据时, 就会存在需要二次查询的情况, 这就是我们说的`Page Cache`的命中率的问题

那么知道了原理我们就可以做一些对应的操作来提升命中率, 如:

- 预读：在读取文件的时候, 将文件预读到`Page Cache`中, 以便在下次读取同样的数据时, 可以直接从`Page Cache`中获取, 减少磁盘`I/O`操作. 可以使用`madvise()`系统调用或者`posix_fadvise()`来进行预读操作.
- 合理设置缓存大小：`Page Cache`的大小可以通过`/proc/sys/vm/dirty_background_bytes`(脏页面的最小字节数), `/proc/sys/vm/dirty_bytes`(脏页面的最大字节数), `/proc/sys/vm/dirty_expire_centisecs`(内核写回脏页面的时间间隔)进行设置, 调整缓存大小可以根据应用的内存使用情况来确定.
- 合理使用文件系统缓存：在读取文件时, 可以考虑使用文件系统缓存来提高`Page Cache`的命中率. 例如, 在使用`fread()`等文件读取函数时, 可以设置`_IOFBF`标志来启用文件系统缓存.
- 合理使用页回收机制：当`Page Cache`的命中率低于预期时, 可以考虑使用页回收机制, 将不常用的页面交换出去以腾出更多的内存空间.
- 避免重复映射同一文件：如果一个文件被重复映射到多个进程的虚拟内存空间中, 那么每个进程都会有自己的`Page Cache`. 为了提高Page Cache的命中率, 可以尝试将同一文件只映射到一个进程的虚拟内存空间中.
- 避免频繁的文件操作：频繁的文件操作会导致`Page Cache`的命中率降低, 因此可以尝试减少文件操作的次数, 或者使用一些优化技术, 如缓存文件描述符等来提高文件操作的效率.
- 增加内存大小：如果系统内存不足, 操作系统会将`Page Cache`中的数据块替换出内存, 以便为系统运行提供足够的内存空间, 这样也可能会导致`Page Cache`的命中率降低.
- 对于热点数据改用内存池等大数据块：由于`Page Cache`的缓存粒度通常比小数据块要大, 所以每次只有部分数据块能够被缓存到`Page Cache`中, 而剩余的部分则仍需要从磁盘中读取, 缺页的概率会增加. 当然操作系统为了提高磁盘IO的效率, 可能会按照一定的规则将其合并成一个较大的数据块, 从而减少磁盘IO的次数。但即使如此依旧可能存在碎片化热点数据的情况, 若主动申请开辟内存池, 则可将热点数据统一加载, 从而提升命中率. 

### 8.3 CPU 与内存通讯

首先我们需要了解为什么需要分多级缓存

众所周知, `CPU`缓存分级, `L1`最快但最小, 内存反倒在最后,  查找等级也是`L1`->`L2`->`L3`->内存.

`Cache`快也是因为其分布

​     一个双核心处理器([10.3.2 内存屏障(`Memory barrier`)](#10.3.2 内存屏障(`Memory barrier`)))里通常有2个core, 这2个core要有自己的小单间吧, 于是每个core自己的小单间就是`L1`(位于CPU核心内部)

​     那他两是好兄弟, 都是一个集成芯片下的,  那这个`CPU`家庭也得有自己的公共小仓库吧, 就是`L2`(位于CPU核心和主存之间)

​     那好几个`CPU`家庭组成了社区, 那不得有个菜鸟驿站? 就是`L3`(通常位于CPU芯片上)

​     菜鸟驿站在东方明珠的地盘下, 2万平米的共享床垫他放不下, 那就在郊区找个厂子吧, 就是`内存`

所以一个core要用数据的时候, 就先看自己单间(`L1`)里有没有, 没找到就仓库(`L2`)里瞅瞅, 还是找不到就去问菜鸟驿站(`L3`)要, 都找不到, 就穿上996上班服去郊区厂里(`内存`)报道吧.......

那么, 为什么要设立这么多层级? 我们有什么可以利用? 

设想一下, 在`Core`建造之初, 没有多级缓存, `linux`为了数据的高效性, 所有数据的存取都是凑满一条`cache line`的长度来进行传递, 当进程获取内存时, 首先将数据从屋里内存通过`bus`总线搬运获取后交给 寄存器->执行单元, 其后该`cache line`用不到了, 自然要被丢弃, 但实际中, 对于热点数据我们必然会多次使用, 此时我们是否可以加一个缓存, 每次获取到数据以后就记录下上次使用时间, 超过时间阈值再丢弃, 不超过那么就可以不需要重新通过`bus`总线搬运数据, 而是直接返回, 这样是不是就更加高效了;

而随着技术的发展, 这样的缓存有3个(`intel`据说在准备4个, 目前尚未发布), 这三个缓存对应的就是`L3`->`L2`->`L1`, 数字越小越接近`core`, 容量越小(`L1`: 几十KB, `L2`: 几百KB到几MB, `L3`: 几MB到几十MB), 数据也越热.

但多级缓存必然带来数据同步的问题, 这也很好理解, 所以CPU用一个`MESI`协议来确保数据一致性

**CPU `MESI`协议说明地址:https://blog.csdn.net/Sword52888/article/details/125994201**

**内存过程动态演示地址: https://www.scss.tcd.ie/Jeremy.Jones/VivioJS/caches/MESIHelp.htm**

### 8.3 `cpu`缓存

了解了CPU的缓存原理以后 ,那么我们就可以在此基础上做出针对性的优化

#### 8.3.1 典型案例5: 伪共享

先看代码

```c++
#include <iostream>
#include <thread>

// 作者主机cache_line为64, sizeof(long) = 4
#define CACHELINE_SIZE 64

struct FalseShardingPointer {
    volatile long x_;
    volatile long y_;
};

int64_t test_false_sharding(int count) {
    auto pointer = new FalseShardingPointer;
    //std::cout << "---------------------test_false_sharding-----------------------" << std::endl;
    int64_t start = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::thread t1([&]{
        for (int i = 0; i < count; i++) {
            pointer->x_++;
        }
    });
    std::thread t2([&]{
        for (int i = 0; i < count; i++) {
            pointer->y_++;
        }
    });
    t1.join();
    t2.join();
    int64_t end = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    //std::cout << "time cost:" << end - start << std::endl;
    delete pointer;
    return end - start;
}

struct ShardingPointer {
    volatile long x_;
    char pad1[CACHELINE_SIZE - sizeof(long)];
    volatile long y_;
    char pad2[CACHELINE_SIZE - sizeof(long)];
} __attribute__((__aligned__(CACHELINE_SIZE)));

int64_t test_sharding(int count) {
    auto pointer = new ShardingPointer;
    //std::cout << "---------------------test_sharding-----------------------" << std::endl;
    int64_t start = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::thread t1([&]{
        for (int i = 0; i < count; i++) {
            pointer->x_++;
        }
    });
    std::thread t2([&]{
        for (int i = 0; i < count; i++) {
            pointer->y_++;
        }
    });
    t1.join();
    t2.join();
    int64_t end = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    //std::cout << "time cost:" << end - start << std::endl;
    return end - start;
    delete pointer;
}

int main() {
    constexpr int count = 10000000;
    int64_t false_sharding_time(0), sharding_time(0);
    for (int i = 0; i < 100; i++) {
        false_sharding_time += test_false_sharding(count);
        sharding_time += test_sharding(count);
    }

    std::cout << "false_sharding_time avg time cost:" << false_sharding_time / 100 << std::endl;
    std::cout << "sharding_time       avg time cost:" << sharding_time / 100 << std::endl;
}
```

按常理, 大家肯定觉得`ShardingPointer`比`FalseShardingPointer`还要大, 那肯定更加耗时对不对, 那么我们来看下结果:

```shell
false_sharding_time avg time cost:61858942
sharding_time       avg time cost:24913901
```

其原因就是伪共享, 原理还是由于`cache line`, 推荐参考 https://zhuanlan.zhihu.com/p/124974025



#### 8.3.2 典型案例2: 数组行列获取差异

```c++
#include <iostream>
#include <chrono>

int main() {
    constexpr int64_t row_count = 500;
    constexpr int64_t col_count = 500;
    int array[row_count][col_count] = {0};

    int64_t row_time(0), col_time(0);
    int64_t start(0), end(0);
    for (int k = 0; k < 10; ++k) {
        //std::cout << "---------------------横向遍历-----------------------" << std::endl;
        start = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        for (int i = 0; i < row_count; ++i)
            for (int j = 0; j < col_count; ++j)
                ++array[i][j];
        end = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        //std::cout << "time cost:" << end - start << std::endl;
        row_time += end - start;

        //std::cout << "---------------------纵向遍历-----------------------" << std::endl;
        start = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        for (int j = 0; j < col_count; ++j)
            for (int i = 0; i < row_count; ++i)
                ++array[i][j];
        end = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        //std::cout << "time cost:" << end - start << std::endl;
        col_time += end - start;
    }

    std::cout << "横向遍历 avg time cost:" << row_time / 10 << std::endl;
    std::cout << "纵向遍历 avg time cost:" << col_time / 10 << std::endl;
    std::cout << array[0][0] << std::endl; // 防优化
    return 0;
}
```

输出结果:

```c++
横向遍历 avg time cost:67270
纵向遍历 avg time cost:215260
```

横向遍历比纵向遍历能如此迅速的原因, 便是因为获取的即连续内存,每次`CPU`获取的数据是一片一片的, 

但也并非所有跳跃式获取均存在明显差异, 我们通过下一个案例来看下影响的内在原因究竟是什么:

```c++
#include <iostream>
#include <chrono>

int main() {
    constexpr int64_t row_count = 204800;
    int array[row_count] = { 0 };

    int64_t row_time(0), col_time(0);
    int64_t start(0), end(0);
    for (int k = 0; k < 10; ++k) {
        //std::cout << "---------------------单步循环-----------------------" << std::endl;
        start = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        for (int i = 0; i < row_count; ++i)
            ++array[i];
        end = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        //std::cout << "time cost:" << end - start << std::endl;
        row_time += end - start;

        //std::cout << "---------------------跳跃循环-----------------------" << std::endl;
        start = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        for (int i = 0; i < row_count; i += 17)
            ++array[i];
        end = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        //std::cout << "time cost:" << end - start << std::endl;
        col_time += end - start;
    }

    std::cout << "单步循环 avg time cost:" << row_time / 10 << std::endl;
    std::cout << "跳跃循环 avg time cost:" << col_time / 10 << std::endl;
    std::cout << array[0] << std::endl; // 防优化
    return 0;
}
```

跳跃循环仅执行`1/17`的任务, 然后我们来看下开销

```c++
单步循环 avg time cost:50070
跳跃循环 avg time cost:33700
```

是的, 其开销却根本不是`1/17`, 我们可以一点点的微调步进长度, 来看下是否相同, 也可以直接参考结果: http://igoro.com/archive/gallery-of-processor-cache-effects/ (Example 1)

注意步长在1到16的范围内时, 循环的运行时间几乎不变. 但是从16开始, 步长每增加一倍, 其运行时间也减少一半.

其背后的原因是, 如今的CPU并不是逐个字节地访问内存. 相反, 它以 ( 通常情况下 ) 64字节的块为单位取内存(`cache lines`). 当你读取一个特定的内存地址时, 整个缓存行都被从主内存取到缓存中. 下次获取时, 若未发生跳转且依旧在当前缓存中时, 则能直接快速获取而无需从内存再次获取.

因为16个整数占用了64字节 ( 一个`cache line` ), 因此步长从1到16的for循环都必须访问相同数量的缓存行: 即数组中的所有缓存行.  但是如果步长是32, 我们只需要访问约一半的缓存行; 步长是64时, 只有四分之一.

理解缓存行`cache line`对特定类型的程序优化非常重要. 例如, 数据对齐可能会决定一个操作访问一个还是两个缓存行. 如我们上面例子中看到的, 它意味着在不对齐的情形下, 操作将慢一倍.

获取`cache_line`大小的方法:

```c++
#ifndef GET_CACHE_LINE_SIZE_H_INCLUDED
#define GET_CACHE_LINE_SIZE_H_INCLUDED

// Author: Nick Strupat
// Date: October 29, 2010
// Returns the cache line size (in bytes) of the processor, or 0 on failure

#include <stddef.h>
size_t cache_line_size();

#if defined(__APPLE__)

#include <sys/sysctl.h>
size_t cache_line_size() {
    size_t line_size = 0;
    size_t sizeof_line_size = sizeof(line_size);
    sysctlbyname("hw.cachelinesize", &line_size, &sizeof_line_size, 0, 0);
    return line_size;
}

#elif defined(_WIN32)

#include <stdlib.h>
#include <windows.h>
size_t cache_line_size() {
    size_t line_size = 0;
    DWORD buffer_size = 0;
    DWORD i = 0;
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION* buffer = 0;

    GetLogicalProcessorInformation(0, &buffer_size);
    buffer = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION*)malloc(buffer_size);
    GetLogicalProcessorInformation(&buffer[0], &buffer_size);

    for (i = 0; i != buffer_size / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION); ++i) {
        if (buffer[i].Relationship == RelationCache && buffer[i].Cache.Level == 1) {
            line_size = buffer[i].Cache.LineSize;
            break;
        }
    }

    free(buffer);
    return line_size;
}

#elif defined(linux)

#include <stdio.h>
size_t cache_line_size() {
    FILE* p = 0;
    p = fopen("/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size", "r");
    unsigned int i = 0;
    if (p) {
        fscanf(p, "%d", &i);
        fclose(p);
    }
    return i;
}

#else
#error Unrecognized platform
#endif

#endif
```



#### 8.3.3 典型案例3: 并发指令优势

当然, 也并非所有缓存行满足的情况下一定直接循环, 例如指令操作较为耗时,的情况下我们也可以使用指令级的并发执行;

*注意: 并发指令即`CPU`乱序, 详细可以结合[10.3.2 内存屏障(`Memory barrier`)](#10.3.2 内存屏障(`Memory barrier`))一起看*

```c++
#include <iostream>
#include <chrono>

int64_t test_one(double* a, int size, bool log) {
    //std::cout << "---------------------test_one-----------------------" << std::endl;
    int64_t start = std::chrono::high_resolution_clock::now().time_since_epoch().count();

    double sum = 0.0f;
    for (int i = 0; i < size; ++i)
        sum += a[i];

    int64_t end = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    //std::cout << "time cost:" << end - start << std::endl;
    if (log)
        std::cout << sum << std::endl;// 防优化
    return end - start;
}


int64_t test_four(double* a, int size, bool log) {
    //std::cout << "---------------------test_four-----------------------" << std::endl;
    int64_t start = std::chrono::high_resolution_clock::now().time_since_epoch().count();

    double sum1 = 0.0f, sum2 = 0.0f, sum3 = 0.0f, sum4 = 0.0f, sum = 0.0f;
    for (int i = 0; i < size; i+=4) {
        sum1 += a[i];
        sum2 += a[i + 1];
        sum3 += a[i + 2];
        sum4 += a[i + 3];
    }
    sum = (sum4 + sum3) + (sum1 + sum2);

    int64_t end = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    //std::cout << "time cost:" << end - start << std::endl;
    if (log)
        std::cout << sum << std::endl;// 防优化
    return end - start;
}

int main() {
    constexpr int count = 1000000;
    double* a = new double[count]();
    for (int i = 0; i < count; i++)
        a[i] = i;

    int64_t one_for_time(0), four_for_time(0);
    for (int k = 0; k < 10; ++k) {
        one_for_time += test_one(a, count, k < 0);
        four_for_time += test_four(a, count, k < 0);
    }

    std::cout << "test_one  avg time cost:" << one_for_time / 10 << std::endl;
    std::cout << "test_four avg time cost:" << four_for_time / 10 << std::endl;
    return 0;
}
```

结果为:

```c++
test_one  avg time cost:1603240
test_four avg time cost:870560
```

其原因依旧参考 http://igoro.com/archive/gallery-of-processor-cache-effects/  (Example 4)

#### 8.3.4 典型案例4: CPU分支预测器的利用

复制网上非常好的一个举例:

```
让我们回到19世纪, 那个远距离无线通信还未普及的年代. 你是铁路交叉口的扳道工. 当听到火车快来了的时候, 你无法猜测它应该朝哪个方向走. 于是你叫停了火车, 上前去问火车司机该朝哪个方向走, 以便你能正确地切换铁轨. 

要知道, 火车是非常庞大的, 切急速行驶时有巨大的惯性. 为了完成上述停车-问询-切轨的一系列动作, 火车需耗费大量时间减速, 停车, 重新开启. 

既然上述过车非常耗时, 那是否有更好的方法？当然有！当火车即将行驶过来前, 你可以猜测火车该朝哪个方向走. 

如果猜对了, 它直接通过, 继续前行. 
如果猜错了, 车头将停止, 倒回去, 你将铁轨扳至反方向, 火车重新启动, 驶过道口. 
如果你不幸每次都猜错了, 那么火车将耗费大量时间停车-倒回-重启. 
如果你很幸运, 每次都猜对了呢？火车将从不停车, 持续前行！

上述比喻可应用于处理器级别的分支跳转指令里：
原程序：
	if (data[c] >= 128)
    	sum += data[c]; 
汇编码：
	cmp edx, 128
	jl SHORT $LN3@main
	add rbx, rdx
	$LN3@main:
	
现在假设你是处理器, 当看到上述分支时, 当你并不能决定该如何往下走, 该如何做？只能暂停运行, 等待之前的指令运行结束. 然后才能继续沿着正确地路径往下走. 

要知道, 现代编译器是非常复杂的, 运行时有着非常长的pipelines,  减速和热启动将耗费巨量的时间. 

那么, 有没有好的办法可以节省这些状态切换的时间呢？你可以猜测分支的下一步走向！

如果猜错了, 处理器要flush掉pipelines, 回滚到之前的分支, 然后重新热启动, 选择另一条路径. 
如果猜对了, 处理器不需要暂停, 继续往下执行. 
如果每次都猜错了, 处理器将耗费大量时间在停止-回滚-热启动这一周期性过程里. 
如果侥幸每次都猜对了, 那么处理器将从不暂停, 一直运行至结束. 

上述过程就是分支预测(branch prediction). 虽然在现实的道口铁轨切换中, 可以通过一个小旗子作为信号来判断火车的走向, 但是处理器却无法像火车那样去预知分支的走向--除非最后一次指令运行完毕. 

那么处理器该采用怎样的策略来用最小的次数来尽量猜对指令分支的下一步走向呢？答案就是分析历史运行记录： 如果火车过去90%的时间都是走左边的铁轨, 本次轨道切换, 你就可以猜测方向为左, 反之, 则为右. 如果在某个方向上走过了3次, 接下来你也可以猜测火车将继续在这个方向上运行...

换句话说, 你试图通过历史记录, 识别出一种隐含的模式并尝试在后续铁道切换的抉择中继续应用它. 这和处理器的分支预测原理或多或少有点相似. 

大多数应用都具有状态良好的(well-behaved)分支, 所以现代化的分支预测器一般具有超过90%的命中率. 但是面对无法预测的分支, 且没有识别出可应用的的模式时, 分支预测器就无用武之地了. 
```

关于分支预测期, 可参考维基百科相关词条`"Branch predictor" article on Wikipedia`
https://en.wikipedia.org/wiki/Branch_predictor

当然这个是CPU自身的分支预测, 他本身会记录下并自身去学习, 这也是很多时候`demo`程序越跑越快的原因之一, 相当于他被训练好了. 那么这个特性我们是否可以利用呢? 当然, 最典型的便是, 先排序, 在遍历:

```c++
#include <algorithm>
#include <chrono>
#include <iostream>

int64_t test_array(const char* title, int* array, int row_count, bool log) {
    int64_t sum(0), start(0), end(0);
    //std::cout << "---------------------" << title << "-----------------------" << std::endl;
    start = std::chrono::high_resolution_clock::now().time_since_epoch().count();

    for (int64_t i = 0; i < row_count; ++i) {
        if (array[i] >= 128)
            sum += array[i];
    }

    end = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    //std::cout << "time cost:" << end - start << std::endl;
    if (log)
        std::cout << "sum = " << sum << std::endl; // 防优化
    return end - start;
}

int main() {
    constexpr int64_t row_count = 256 * 1024 * 1024;
    int* sort_array = new int[row_count]();
    int* no_sort_array = new int[row_count]();
    for (int64_t i = 0; i < row_count; ++i) {
        int tmp = std::rand() % 256;
        sort_array[i] = tmp;
        no_sort_array[i] = tmp;
    }
    std::sort(sort_array, sort_array + row_count);

    int64_t test_sort_time(0), test_no_sort_time(0);
    for (int k = 0; k < 10; ++k) {
        test_sort_time += test_array("排序", sort_array, row_count, k < 0);
        test_no_sort_time += test_array("不排序", no_sort_array, row_count, k < 0);
    }

    std::cout << "test_sort_time    avg time cost:" << test_sort_time / 10 << std::endl;
    std::cout << "test_no_sort_time avg time cost:" << test_no_sort_time / 10 << std::endl;
    return 0;
}
```

同样的两个数组, 假设先排序好然后在遍历, 则在遍历的过程中可以得到大幅度优化, 仅仅依靠CPU自身的分支预测

```c++
test_sort_time    avg time cost:186741330
test_no_sort_time avg time cost:1427915100
```



#### 8.3.5 典型案例5: `linux`下`likely` / `unlikely`优化

除了依赖CPU自身的分支预测, `linux`下也提供了主动分支预测

```c++
#include <stdio.h>
#include <stdlib.h>

#define likely(x) __builtin_expect(!!(x), 1) //gcc内置函数, 帮助编译器分支优化
#define unlikely(x) __builtin_expect(!!(x), 0)

int main(int argc, char* argv[]){
    int x = 0;
    x = atoi(argv[1]);

    if (unlikely(x == 1111111)){  //告诉编译器这个分支非常不可能为true
        x = x + 2222222;
    }
    else{
        x = x - 33333333;
    }

    printf("x=%d\n", x);
    return 0;
}
```

使用命令 `gcc -O2 unlikely.cpp -S unlikely.s`, 查看汇编 ( *各版本略有不同* )

```c++
	.file	"unlikely.cpp"
	.text
	.section	.rodata.str1.1,"aMS",@progbits,1
.LC0:
	.string	"x=%d\n"
	.section	.text.startup,"ax",@progbits
	.p2align 4,,15
	.globl	main
	.type	main, @function
main:
.LFB54:
	.cfi_startproc
	subq	$8, %rsp
	.cfi_def_cfa_offset 16
	movq	8(%rsi), %rdi
	movl	$10, %edx
	xorl	%esi, %esi
	call	strtol
	leal	-33333333(%rax), %edx   // 优先跟随 x = x - 33333333;
	cmpl	$1111111, %eax          // 其后进行判断
	movl	$3333333, %eax          // x = x + 2222222;因为x == 1111111就直接赋值了
	movl	$.LC0, %esi
	movl	$1, %edi
	cmove	%eax, %edx
	xorl	%eax, %eax
	call	__printf_chk
	xorl	%eax, %eax
	addq	$8, %rsp
	.cfi_def_cfa_offset 8
	ret
	.cfi_endproc
.LFE54:
	.size	main, .-main
	.ident	"GCC: (Ubuntu 7.5.0-3ubuntu1~16.04) 7.5.0"
	.section	.note.GNU-stack,"",@progbits
```

`linux`下`gcc`就提供了`__builtin_expect`接口, 它可以直接告诉CPU当前哪个选项更加接近当前分支, CPU本身的分支预测已经足够99%的日常使用, 而当有最极致性能的需求时, 我们还可以通过这个接口主动告知CPU使用哪个分支预测, 以此来达到加速的目的. 

然而`windows`下却没有提供, 不清楚是否是由于`windows`觉得, 反正最终还不是要CPU自己做分支预测, 就算你写了也没啥区别, 写错了还影响分支预测, 毕竟`unlikely`是静态的, 而CPU是动态且无法避免的, 至于`unlikely`翻转连续内存的防止跳转, 那么自己确定好`if{}else{}`顺序便可实现.

C++20之前的, `likely`和`unlikely`只不过是一对自定义的宏. 而C++20中正式将`likely`和`unlikely`确定为属性关键字. 

#### 8.3.6 典型案例6: 缓存关联性

即`L1,L2,L3`三级缓存, 热点数据访问更快



## 9 低时延代码优化

​    这个内容十分宽泛, 期间的奇淫巧技, 我在此强烈推荐 `Facebook` 开源的`Folly`库, 如其名字一般: ***傻瓜式 拿去用就好了***

​    而最关键的是, 其中大量的使用了各种低时延方案, 基于C++14标准, 事实上, 我也强烈建议各位, 2021年的今天, 多看看C++17吧, C++20是个不成熟的尝试, 目前还不需要, 并且以目前的情况来看C++23也不乐观, 而C++17的优化, 却可以给你带来如同当年C++11一般的小惊喜.

​    甚至可以说： **想要不更改业务逻辑,  就拥有更高性能的低延时,  又兼容扩展性,  可读性,  易维护性 , 那就使用 C++17 吧**

###     9.1 利用 `crtp` 来取代 virtual

​    `crtp`通常拿来与`virtual`一起作比较, 那么我也自然不能免俗, 我们来比较一下看看吧, 虽然, 这完全就是两个不同的物种......

​     脱离剂量谈毒性都是耍流氓, 脱离场景谈功能, 就是流氓群里耍流氓了,我们来加一个基础的场景进去:

```c++
// 网上随处可见的桥接
#include <iostream>

class IGameInterface {
public:
    virtual void execImpl() = 0;
};
class GameA : public IGameInterface {
    void execImpl() override {
        std::cout << "a" << std::endl;
    }
};
class GameB : public IGameInterface {
    void execImpl() override {
        std::cout << "b" << std::endl;
    }
};

class IPhoneInterface {
public:
    void setGame(IGameInterface* impl) {
        this->m_impl = impl;
    }
    virtual void todo() = 0;
protected:
    IGameInterface* m_impl;
};
class MyPhone : public IPhoneInterface {
public:
    void todo() override {
        m_impl->execImpl();
    }
};

int main() {
    GameA king_glory;
    GameB eat_chick;

    MyPhone huawei;
    huawei.setGame(&king_glory);
    huawei.todo();
    huawei.setGame(&eat_chick);
    huawei.todo();

    MyPhone apple;
    apple.setGame(&eat_chick);
    apple.todo();

    return 0;
}
```

​    这个没有问题, 真的, 一点毛病都没有, 扩展性强, 就是现在产品经理说别人家的代理一天代理10亿次,  代理的东西也是一个大版本一变, 但是怎么感觉怎么就是每次比我们快0.0001毫秒呢?

​    假设, 别的代码已经优化至极限, 没法改了, 然后你的领导把任务丢给了你, 告诉你, 代码就在这, 优化去吧. 那么我们根据2/8法则, 只看关键项, **一天代理10亿次  &&  一个大版本一变**  

​    那么就好办了, 虚函数存在`virtual`虚基表跳转的问题, 假设我们直接写死, 因为一个大版本内, 手机型号代理的游戏是固定的, 不会存在今天`huawei`代理`king_glory`, 明天改去代理`eat_chick`的情况, 即使有, 是要发布大版本的, 而健壮的产品经理又对性能如此之敏感, 我们只能牺牲部分扩展性来满足高性能的需求.

​    **注意: 特别要强调一下, 树挪死, 人变活, 不要沉浸在规矩的世界里, 设计模式是人定的, 语言创造出来的根本目的就是为了让别人听你的, 所以, 如何在各种纷乱矛盾冲突的规则中, 找到最适合的, 才是重要的技能, 之一**

​    那我们来改造一下, 改造方案有两个方向:

1. 尽可能少的虚继承, 直接改成编译期确定, 不要搞运行时了;
2. 保留一定的扩展性;

    ```c++
    #include <iostream>
    
    template<typename Derived>
    class CrtpGame { // 仅作为仿基类, 若没有默认函数实现, 你完全可以去除该类
    public:
        void exec() const {
            static_cast<const Derived*>(this)->execImpl();
        }
        void exec() {
            static_cast<Derived*>(this)->execImpl();
        }
    private:
        void execImpl() const {}
    };
    class KingGlory : public CrtpGame<KingGlory> {
    public:
        void execImpl() const {
            std::cout << "a" << std::endl;
        }
    };
    class EatChick : public CrtpGame<EatChick> {
    public:
        void execImpl() const {
            std::cout << "b" << std::endl;
        }
    };
    
    class Huawei {
    public:
        void todo() {
            m_a.exec();
            m_b.exec();
        }
    private:
        KingGlory  m_a;
        EatChick  m_b;
    };
    class Apple {
    public:
        void todo() {
            m_b.exec();
        }
    private:
        EatChick  m_b;
    };
    
    int main() {
        Huawei huawei;
        huawei.todo();
    
        Apple apple;
        apple.todo();
    
        return 0;
    }
    ```

​    这个改造的过程就是`crtp`,  把原先运行时动态确定的, 改成编译期二进制就确定好了, 那么有的同学说了, 这个还是很难维护啊, 刚开始只有1-2个还好, 后面100个游戏代理你写着玩吗? 又不是同一个基类!

​    这个有很多其他的方式可以实现, 在这里我个人推荐使用C++17的`std::variant`配合`std::visit`来实现,

```c++
#include <iostream>
#include <vector>
#include <variant>

template<typename Derived>
class CrtpGame { // 仅作为仿基类, 若没有默认函数实现, 你完全可以去除该类
public:
    void exec() const {
        static_cast<const Derived*>(this)->execImpl();
    }
private:
    void execImpl() const {}
};
class KingGlory : public CrtpGame<KingGlory> {
public:
    void execImpl() const {
        std::cout << "a" << std::endl;
    }
};
class EatChick : public CrtpGame<EatChick> {
public:
    void execImpl() const {
        std::cout << "b" << std::endl;
    }
};

class Huawei {
public:
    Huawei() {
        m_var.emplace_back(KingGlory());
        m_var.emplace_back(EatChick());
    }
    void todo() {
        for (auto& item : m_var) {
            std::visit([&](const auto& t) { t.exec(); }, item);
        }
    }
private:
    using myVariant = std::variant<KingGlory, EatChick>;
    std::vector<myVariant> m_var;
};
class Apple {
public:
    Apple() {
        m_var.emplace_back(EatChick());
    }
    void todo() {
        for (auto& item : m_var) {
            std::visit([&](const auto& t) { t.exec(); }, item);
        }
    }
private:
    using myVariant = std::variant<EatChick>;
    std::vector<myVariant> m_var;
};

int main() {
    Huawei huawei;
    huawei.todo();

    Apple apple;
    apple.todo();

    return 0;
}
```

​    我们来把最终的`crtp`方案与刚开始的`virtual`方案来比对一下看看性能消耗:

```c++
#include <iostream>
#include <chrono>
#include <vector>
#include <variant>
#include <atomic>
std::atomic<long long> g_crtp_count(0);
std::atomic<long long> g_virtual_count(0);

class IGameInterface {
public:
    virtual void exec() = 0;
};
class GameA : public IGameInterface {
    void exec() override {
        ++g_virtual_count;
    }
};
class GameB : public IGameInterface {
    void exec() override {
        ++g_virtual_count;
    }
};

class IPhoneInterface {
public:
    void setGame(IGameInterface* game) {
        this->m_games.emplace_back(game);
    }
    virtual void todo() = 0;
protected:
    std::vector<IGameInterface*> m_games;
};
class MyPhone : public IPhoneInterface {
public:
    void todo() override {
       for (auto& game : m_games)
          game->exec();
    }
};

template<typename Derived>
class CrtpGame { // 仅作为仿基类, 若没有默认函数实现, 你完全可以去除该类
public:
    void exec() const {
        static_cast<const Derived*>(this)->execImpl();
    }
private:
    void execImpl() const {}
};
class KingGlory : public CrtpGame<KingGlory> {
public:
    void execImpl() const {
        ++g_crtp_count;
    }
};
class EatChick : public CrtpGame<EatChick> {
public:
    void execImpl() const {
        ++g_crtp_count;
    }
};

class Huawei {
public:
    template<typename TGame>
    void setGame(TGame* game) {
        m_games.emplace_back(game);
    }
    void todo() {
        for (auto& game : m_games) {
            std::visit([&](const auto& t) { t->exec(); }, game);
        }
    }
private:
    using GameVariantPtr = std::variant<KingGlory*, EatChick*>;
    std::vector<GameVariantPtr> m_games;
};
class Apple {
public:
    template<typename TGame>
    void setGame(TGame* game) {
        m_games.emplace_back(game);
    }
    void todo() {
        for (auto& game : m_games) {
            std::visit([&](const auto& t) { t->exec(); }, game);
        }
    }
private:
    using GameVariantPtr = std::variant<EatChick*>;
    std::vector<GameVariantPtr> m_games;
};

int main() {
    constexpr int64_t count = 100000000; // 1亿
    int64_t vitrual_avg_ns = 0;
    int64_t crtp_avg_ns = 0;

    GameA king_glory;
    GameB eat_chick;
    MyPhone huawei;
    huawei.setGame(&king_glory);
    huawei.setGame(&eat_chick);
    MyPhone apple;
    apple.setGame(&eat_chick);

    KingGlory king_glory_crtp;
    EatChick eat_chick_crtp;
    Huawei huawei_crtp;
    huawei_crtp.setGame(&king_glory_crtp);
    huawei_crtp.setGame(&eat_chick_crtp);
    Apple apple_crtp;
    apple_crtp.setGame(&eat_chick_crtp);

    for (int retry = 0; retry < 10; retry++) {
        auto begin = std::chrono::high_resolution_clock::now();
        for (int64_t i = 0; i < count; i++) {
            huawei.todo();
            apple.todo();
        }
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - begin).count();
        //std::cout << ns << std::endl;
        vitrual_avg_ns += ns;

        begin = std::chrono::high_resolution_clock::now();
        for (int64_t i = 0; i < count; i++) {
            huawei_crtp.todo();
            apple_crtp.todo();
        }
        ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - begin).count();
        //std::cout << ns << std::endl << std::endl;
        crtp_avg_ns += ns;
    }
    std::cout << "virtual 平均耗时:" << vitrual_avg_ns / 10 << std::endl;
    std::cout << "crtp    平均耗时:" << crtp_avg_ns / 10 << std::endl;

    std::cout << "g_crtp_count = " << g_crtp_count.load() << std::endl;
    std::cout << "g_virtual_count =" << g_virtual_count.load() << std::endl << std::endl;

    /// 防止cpu分支预测, 交换顺序后二次校验
    g_crtp_count = 0;
    g_virtual_count = 0;
    vitrual_avg_ns = 0;
    crtp_avg_ns = 0;
    
    for (int retry = 0; retry < 10; retry++) {
        auto begin = std::chrono::high_resolution_clock::now();
        for (int64_t i = 0; i < count; i++) {
            huawei_crtp.todo();
            apple_crtp.todo();
        }
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - begin).count();
        //std::cout << ns << std::endl;
        crtp_avg_ns += ns;

        begin = std::chrono::high_resolution_clock::now();
        for (int64_t i = 0; i < count; i++) {
            huawei.todo();
            apple.todo();
        }
        ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - begin).count();
        //std::cout << ns << std::endl << std::endl;
        vitrual_avg_ns += ns;
    }
    std::cout << "virtual 平均耗时:" << vitrual_avg_ns / 10 << std::endl;
    std::cout << "crtp    平均耗时:" << crtp_avg_ns / 10 << std::endl;

    std::cout << "g_crtp_count = " << g_crtp_count.load() << std::endl;
    std::cout << "g_virtual_count =" << g_virtual_count.load() << std::endl;
    
    return 0;
}
```

最终输出结果为：

```shell
virtual 平均耗时:1717648271
crtp    平均耗时:1667254837
g_crtp_count = 300000000
g_virtual_count =300000000

virtual 平均耗时:1721142346
crtp    平均耗时:1669575153
g_crtp_count = 300000000
g_virtual_count =300000000
```

​    当然`crtp`的使用还有很多, 可以参考: https://zhuanlan.zhihu.com/p/137879448

### 9.2 `Ringbuffer`相关内存池无锁队列

​    在消息传递中, 常见的如网络通讯中, 每次倘若都使用`new`, 那么你每天接收1亿次消息, 就意味着`new` / `delete` 1亿次, 这之间的开销何其庞大, 但是倘若又不使用, 又会造成数据的无法获取, 那么我们可以考虑使用`ringbuffer`等相关的内存池技术来实现, 相关介绍网上很多, 建议使用`github`开源代码.

​    当然也可以使用`boost::asio::streambuf::mutable_buffers_type`等, 相比较`ringbuffer`的固定长度, `boost::asio::streambuf::mutable_buffers_type`可变长度更通用, 不过性能上略有损失, 且可能造成内存的过度增长,若无特殊需求, 可在通用项目中使用

### 9.3 无锁哈希表

通常而言, 无锁hash适用于初始化确定的, 其后改动次数不高的情况, 若改动次数很多则建议使用`tbb`库中的无锁hash, 该无锁不会带来过高的内存扩展问题

### 9.4 协程处理多任务下的有序执行

我们习惯于使用各类无锁队列来处理高并发的问题, 但大部分任务只是区分了任务, 但是单一任务之间依旧存在先后强顺序性, 例如:所有证券的行情信息, 存在不定个数的行情, 但是单一行情内存在严格先后顺序;

这个时候假设采用无锁队列, 最多就是一个行情新增一个无锁队列, 但是在消费时, 却无法做到并发性, 而假设使用了统一的大无锁队列, 不做无锁哈希表, 那么更是会存在消费先后顺序的问题;

此时, 我们可以使用协程来完美处理, 协程的单线程属性也确保了数据处理的先后性, 甚至无需`atomic`来等待原子时钟周期, 毕竟`atomic`依旧存在最差一个时钟周期的等待时间. 其后的顺序处理与发送, 完全可以采用单线程的顺序思维处理, 并且依旧可以压榨`CPU`, 且不存在`while`刷盘的情况, 增加硬盘使用寿命.

参考: https://github.com/xyconly/btool/blob/master/coro_task_pool.hpp

### 9.5 返回值优化

​    虽然编译器会选择性的进行 `RVO(return value optimization) ` 优化,  但是不是强制的, 当函数有多个返回语句并且返回不通名称的对象,  函数过于复杂, 返回对象没有定义拷贝构造函数时, ` rvo`优化是不会执行的, 所以当函数返回一个很大的对象时在不确定`rvo`优化会执行时, 尽量避免值传递

```c++
#include <iostream>
#include <chrono>
#include <unordered_map>

class BigObject {
public:
    BigObject() {
        std::cout << "BigObject()" << std::endl;
    }
    ~BigObject() {
        std::cout << "~BigObject()" << std::endl;
    }
    BigObject(const BigObject&) {
        std::cout << "BigObject(const BigObject&)" << std::endl;
    }
    BigObject(BigObject&&) {
        std::cout << "BigObject(BigObject&&)" << std::endl;
    }
};

struct info {
    std::string str;
    std::unordered_map<int, std::string> umap;
};

std::string get_rvo_rtn_string() { // string 存在string(string&&)移动构造, 编译器自动优化rvo
    std::string st;
    st = "ssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssss";
    return st;
}

std::string get_move_rtn_string() {
    std::string st;
    st = "ssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssss";
    return std::move(st);
}

info get_info_v1() {  // string 存在string(string&&)移动构造, 编译器自动优化rvo
    info ifo;
    ifo.str = "get_info_v1";
    ifo.umap.insert(std::make_pair<int, std::string>(6, "eggs"));
    return ifo;
}

info get_info_v2()
{
    info ifo;
    ifo.str = "get_info_v2";
    ifo.umap.insert(std::make_pair<int, std::string>(6, "eggs"));
    return std::move(ifo);
}

BigObject foo(int n) {
    BigObject localObj;
    return localObj;
}

int main() {
    auto f = foo(1); // 测试是否存在编译器自动rvo优化

    constexpr int count = 1000000;

    int64_t rvo_time(0), move_time(0), info_rvo_time(0), info_move_time(0);
    int64_t start(0), end(0);
    for (int j = 0; j < 10; j++) {

        //std::cout << "---------------------test rvo-----------------------" << std::endl;
        start = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        for (int i = 0; i < count; i++) {
            std::string d1 = get_rvo_rtn_string();
        }
        end = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        rvo_time += end - start;
        //std::cout << "time cost:" << end - start << std::endl;

        //std::cout << "---------------------test move-----------------------" << std::endl;
        start = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        for (int i = 0; i < count; i++) {
            std::string d2 = get_move_rtn_string();
        }
        end = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        move_time += end - start;
        //std::cout << "time cost:" << end - start << std::endl;

        //std::cout << "---------------------info test rvo-----------------------" << std::endl;
        start = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        for (int i = 0; i < count; i++) {
            auto d3 = get_info_v1();
        }
        end = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        info_rvo_time += end - start;
        //std::cout << "time cost:" << end - start << std::endl;

        //std::cout << "---------------------info test move-----------------------" << std::endl;
        start = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        for (int i = 0; i < count; i++) {
            auto d4 = get_info_v2();
        }
        end = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        info_move_time += end - start;
        //std::cout << "time cost:" << end - start << std::endl;
    }

    std::cout << "test      rvo  avg time cost:" << rvo_time / 10 << std::endl;
    std::cout << "test      move avg time cost:" << move_time / 10 << std::endl;
    std::cout << "test info rvo  avg time cost:" << info_rvo_time / 10 << std::endl;
    std::cout << "test info move avg time cost:" << info_move_time / 10 << std::endl;

    return 0;
}
```

返回结果

```shell
BigObject()
test      rvo  avg time cost:129075260
test      move avg time cost:136604820
test info rvo  avg time cost:332115360
test info move avg time cost:513931250
~BigObject()
```

所以记住顺序: **`const &` 优先级 > `RVO` 优先级 > `std::move` 优先级**

### 9.6 `trivial `/ `Standard Layout` / `POD`

#### 9.6.1 `trivial`类型：

1. 没有虚函数或虚基类. 
2. 由编译器生成默认的特殊成员函数, 包括默认构造函数、拷贝构造函数、移动构造函数、赋值运算符、移动赋值运算符和析构函数. 
3. 成员变量同样需要满足条件 1 和 2. 

通常具有以下特性：

- 可以使用`std::memcpy`函数进行内存拷贝；
- 对象的默认初始化可以不执行任何操作；
- 对象的销毁可以不执行任何操作；
- 可以使用C语言中的函数进行操作, 如使用C库函数处理`trivial`类型的数组等；. 

#### 9.6.2 `Standard Layout` 类型：

1. 没有虚函数或虚基类. 
2. 所有非静态数据成员都具有相同的访问说明符（`public` / `protected` / `private`）. 
3. 在继承体系中最多只有一个类中有非静态数据成员. 
4. 子类中的第一个非静态成员的类型与其基类不同. (原因参考:https://zhuanlan.zhihu.com/p/479755982)

```cpp
#include <iostream>
struct A{
    A(){}
    void todo(){}
};
struct B : A {
    A a;    // 不满足条件4
    int i;
};
struct C : A{
    int i;  // 满足条件4
    A a;
};
int main() {
    static_assert(!std::is_standard_layout<B>::value);
    static_assert(std::is_standard_layout<C>::value);
    B b;
    std::cout << "b addr = " << &b << std::endl;
    std::cout << "b.a addr = " << &b.a << std::endl;
    std::cout << "b.i addr = " << &b.i << std::endl;
    C c;
    std::cout << "c addr = " << &c << std::endl;
    std::cout << "c.i addr = " << &c.i << std::endl;
    std::cout << "c.a addr = " << &c.a << std::endl;
    return 0;
}
```

返回结果

```shell
b addr = 0x7ffffffde70
b.a addr = 0x7ffffffde71
b.i addr = 0x7fffffffde74
c addr = 0x7ffffffde78
c.i addr = 0x7ffffffde78
c.a addr = 0x7fffffffde7c
```

通常具有以下特性：

- 可以使用`std::memcpy`进行内存拷贝；
- 可以使用`offsetof`宏获取成员的偏移量；
- 可以进行类型别名转换；
- 可以进行static_cast和reinterpret_cast转换；
- 可以使用std::aligned_storage实现内存池等功能；
- 具有与C语言的兼容性. 

#### 9.6.3 `POD（Plain Old Data）` 类型：

​    是`C++03`标准中的一个概念, 它包括两种类型：`POD`结构体和`POD`联合体. `C++11`标准中取消了`POD`的概念, 而引入了`Standard Layout`的概念, 但是`POD`类型的概念仍然被广泛使用. `c++20`已完全抛弃`POD`的概念, 当一个对象既是`trivial`又是`Standard Layout`, 那么该对象即为`POD`对象

#### 9.6.4 利用`trivial`特性

**避免无用的缺省构造函数, 导出结构体中更是如此, 结构体就是结构, 业务交给类去做, 可以添加`handler`类来辅助**

在C++中, 被称为`trivial`的, 是指本身没有提供构造函数与析构函数, 同时其成员变量也没有提供构造函数与析构函数的对象, 这种对象在内存上与C结构体一致

**注意:与C结构体(`struct`)一致! 即使他是一个类!**

换言之, `trivial`对象与`int`, `double`这些一样编译器对象一样, 在调用构造函数时**没有执行期间的消耗**

如此美妙的特性, 使得我们可以放心大胆的进行使用`std::initializer_list`, 以及无需关注延迟定义:

```c++
#include <iostream>
#include <chrono>
#include <atomic>
#include <unordered_map>

std::atomic<int64_t> g_trivial_count(0);
std::atomic<int64_t> g_untrivial_count(0);

struct TrivialSt {
    int     x_[10000];
    int     y_[10000];
    int     z_[10000];
};

struct UntrivialSt {
    int     x_[10000] = { 0 };
    int     y_[10000] = { 0 };
    int     z_[10000] = { 0 };
};

bool test_trivial(bool bl) {
    TrivialSt st = { 0 };
    if (bl) {
        ++st.x_[0]; // 防编译器优化
        ++g_trivial_count;
        return st.x_[0] > 10 ? false : bl;
    }
    return bl;
}

bool test_untrivial(bool bl) {
    UntrivialSt st;
    if (bl) {
        ++st.x_[0]; // 防编译器优化
        ++g_untrivial_count;
        return st.x_[0] > 10 ? false : bl;
    }
    return bl;
}

int main() {
    static_assert(std::is_trivial<TrivialSt>::value);
    static_assert(!std::is_trivial<UntrivialSt>::value);

    constexpr int64_t count = 10000000;
    constexpr int64_t jcount = 100;
    bool bl = false;
    std::cin >> bl; // 防编译器优化

    int64_t trivial_time(0), untrivial_time(0);
    int64_t start(0), end(0);
    for (int j = 0; j < jcount; j++) {

        //std::cout << "---------------------test_trivial-----------------------" << std::endl;
        start = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        for (int64_t i = 0; i < count; i++) {
            bl = test_trivial(bl);
        }
        end = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        trivial_time += end - start;
        //std::cout << "time cost:" << end - start << std::endl;

        //std::cout << "---------------------test_untrivial-----------------------" << std::endl;
        start = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        for (int64_t i = 0; i < count; i++) {
            bl = test_untrivial(bl);
        }
        end = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        untrivial_time += end - start;
        //std::cout << "time cost:" << end - start << std::endl;
    }

    std::cout << "trivial count:" << g_trivial_count.load() << std::endl;
    std::cout << "untrivial count:" << g_untrivial_count.load() << std::endl;
    std::cout << "test trivial   avg time cost:" << trivial_time / jcount << std::endl;
    std::cout << "test untrivial avg time cost:" << untrivial_time / jcount << std::endl;
    return 0;
}
```

输入0, 输出结果为:

```shell
trivial count:0
untrivial count:0
test trivial   avg time cost:2294350
test untrivial avg time cost:3060094
```

**所以在这里, 各位应该也能理解, 为什么规范中建议大家变量延时定义了吧, 但其实这对trivial对象可有可无, 只要不执行更改, 就没有运行时开销, 并且trivial对象自带右值引用等操作, 可以极大的节省重复代码的编写**

### 9.7 内核旁路技术

1. `DPDK`
   开源的数据平面开发工具包, 可以帮助应用程序绕过操作系统内核, 直接访问网络适配器的硬件资源
   支持多队列、多核心和多协议等技术
2. `PF_RING`
   一个高性能的数据包处理框架, 它能够提供高速数据包捕获、分发和处理能力, 支持多种数据包协议和多种网络适配器
   `PF_RING`本身并不直接利用`GPU`进行数据包处理, 但是它可以与其他的`GPU`加速技术结合使用, 以提高系统的数据包处理和分发性能
   支持零拷贝、多队列、多核心和多协议等技术
3. `Snabbswitch`
   基于`Lua`语言的高性能网络数据平面框架, 通过`DPDK`等技术与网络适配器进行交互, 从而实现网络数据包的处理和转发. `Snabb Switch`还可以与Linux内核进行交互, 利用内核旁路技术（如`PF_RING`等）实现更高的性能和更灵活的网络数据处理
   可以快速实现各种不同的网络功能, 例如路由、防火墙、负载均衡和`VPN`等. 支持多核心和多进程的并发处理
4. `Netmap`
   一种高性能网络数据包处理框架, 旨在提供低延迟、高吞吐量和低CPU利用率的网络数据包处理能力. `Netmap`通过提供一套优化的用户态库和驱动程序, 实现了零拷贝和轮询机制, 可以绕过操作系统内核, 直接访问网络适配器的硬件资源

应用场景：

- `PF_RING`: 适用于高速数据包捕获和流量分析, 可用于网络流量分析、`DDoS`检测和网络性能监控等领域. 
- `Snabb Switch`: 适用于构建高性能的虚拟路由器、网络功能虚拟化、云计算网络等领域. 
- `DPDK`: 适用于构建高性能的数据包处理应用, 例如防火墙、路由器、负载均衡器、入侵检测系统等. 
- `Netmap`: 适用于高速数据包捕获和流量处理, 可用于网络流量分析、网络嗅探、网络安全等领域. 

需要注意的是, 以上仅是每种技术的一般应用场景, 实际应用还需要结合具体的业务需求进行选择. 

参考的有: https://www.jianshu.com/p/09b4b756b833

​    内核旁路技术虽然可以提供更快速的方案, 但是对于`CPU`来说依旧存在巨大的开销, 只能单机部署单应用, 所以在更多的情况下我们采用`RDMA`等技术(参考[10.3.2 `rdma`特制网卡](#10.3.2-rdma特制网卡)), 直接将数据拷贝至内存来达到零拷贝.

**注意, 零拷贝不是没有`copy`, 而是尽量可能减少`copy`, 或者`CPU`全程不参与`copy`**



### 9.8 零拷贝小技巧

参考来源: https://zhuanlan.zhihu.com/p/78335525

#### 9.8.1 `mmap`

虚拟内存的内存映射, 是通过`mmap`完成的, 那么`mmap`是什么?

**描述**

`mmap`: `Memory Mapped Files`, 可以将一个文件/一段物理内存或者其它对象映射到进程的虚拟内存地址空间, 用户通过修改内存就能修改对应内容. 

**原理**

直接利用操作系统的分页`Page`来实现文件到物理内存的直接映射.  完成映射之后你对物理内存的操作会被同步到硬盘上(操作系统在调用`flush`之后)

应用进程调用了 `mmap()` 后, `DMA`会把磁盘的数据拷贝到内核的缓冲区里. 之后直接把内核缓冲区里的数据**映射**到用户空间, 应用进程跟操作系统内核**共享**这个缓冲区. 这样, 操作系统内核与用户空间就不需要再进行任何的数据拷贝操作

*内存相关讨论可以参考: https://www.cnblogs.com/CareySon/archive/2012/04/25/2470063.html*

**注意点**

1. 不可映射大于`2G`的文件
2. `mmap`也有一个很明显的缺陷: 延时写入, 不可靠
   写到`mmap`中的数据并没有被真正的写到硬盘,  操作系统会在程序主动调用`flush`的时候才把数据真正的写到硬盘
   `kafka`提供了一个参数: `producer.type`来控制是不是主动`flush`; 如果`kafka`写入到`mmap`之后就立即`flush`然后再返回`Producer`叫同步(`sync`); 写入`mmap`之后立即返回`Producer`不调用`flush`叫异步(`async`)
3. 当`mmap`一个文件时, 如果这个文件被另一个进程所截获, 那么`write`系统调用会因为访问非法地址被`SIGBUS`信号终止, `SIGBUS`默认会杀死进程并产生一个`coredump`
   - 解决这个问题通常使用文件的租借锁: 
     - 为文件申请一个租借锁, 当其他进程想要截断这个文件时, 内核会发送一个实时的`RT_SIGNAL_LEASE`信号, 告诉当前进程有进程在试图破坏文件, 这样`write`在被`SIGBUS`杀死之前, 会被中断, 返回已经写入的字节数, 并设置`errno`为`success`
     - 在`mmap`之前加锁, 操作完之后解锁
4. 频繁修改映射区的内存, 会触发脏页写回. 意味着在读文件的时候效率高, 且`mmap`仅支持更新, 不支持追加, 如需追加需要重新开辟映射空间

#### 9.8.2 `sendfile`

 `kafka` 的第二个核心便是利用这个特性! 

第一个是`mmap`, 快速将数据写入磁盘文件. 通常而言,  `kafka`通过将分布式消息顺序存储至本地后, 顺序写入日志文本中, 由于系统顺序访问磁盘的速度可以达到`400M/s`, 所以`kafka`抛弃了随机访问的功能, 随机访问磁盘仅有十几到几百`K/s`

然后, 正常的应用程序(不使用`mmap`以及`sendfile`的), 如何发送一条磁盘数据至网络呢? 通常是

1. 先通过系统接口`read`, 通过`DMA`将数据从磁盘`copy`至内核缓存(内核态);
2. 然后`CPU`将数据从内核缓存`copy`至应用内(用户态);
3. 然后`CPU`将数据`copy`至内核`socket`缓存(内核态);
4. 通过网卡`DMA`直接发送至网卡

```sh
-------------------------------------------------------------------------------
|                                    CPU                                      |
-------------------------------------------------------------------------------
                         /|\    v   /|\        v
                          |(4)  v(5) |(8)      v(9)
                          |     v    |         v
-------------------------------------------------------------------------------
|                                    BUS                                      |
-------------------------------------------------------------------------------
     /|\   DMA copy  v   /|\    v   /|\        v    /|\     DMA copy    |
      |(1)           v(2) |(3)  v(6) |(7)      v(10) |(11)              | (12)
      |              v    |     v    |         v     |                 \|/
------------     -----------------------------------------         -------------------
|    disk  |     |               memory                  |         |   nic(网卡)      |
|          |     |  kernel  |   user     | socket_kernel |         |                 |
------------     -----------------------------------------         -------------------
```

如果此时, 我们无需对数据进行任何处理, 那么可以直接将第1步的数据, 发送至第3步, 就可以减少一次用户态的拷贝了, 而通过`linux`的`sendfile`接口, 即可实现该功能.

```
-------------------------------------------------------------------------------
|                                    BUS                                      |
-------------------------------------------------------------------------------
     /|\   DMA copy v    /|\     支持DMA则DMA  v     /|\     DMA copy    |
      |(1)          v(2)  |(3)   不支持则CPU   v(4)   |(5)               |(6)
      |             v     |                   v      |                 \|/
------------     -----------------------------------------         -------------------
|    disk  |     |               memory                  |         |   nic(网卡)      |
|          |     |  kernel  |   user     | socket_kernel |         |                 |
------------     -----------------------------------------         -------------------
```



**描述**
`sendfile`: 系统调用在两个文件描述符之间直接传递数据(完全在内核中操作), 从而避免了数据在内核缓冲区和用户缓冲区之间的拷贝. 甚至于有些时候, 部分人直接将`sendfile`等同于零拷贝.

**原理**
利用系统, 调用 `DMA` 引擎,  将文件中的数据`copy`到内核缓冲区中, 然后数据被拷贝到内核`socket`缓冲区, 接下来, `DMA`引擎将数据从内核`socket`缓冲区中拷贝到网卡发送数据.

**进阶**

那这个时候,  相信聪明的你自然发现了, 既然如此那为啥不直接再进一步, 直接在数据`copy`至内核缓冲区之后, 直接发送给网卡不久好了. 是的, 确实存在, 这就是**带有`DMA`收集拷贝功能的`sendfile`**, 利用它可以直接将数据加载至内核缓存后发送给网卡

**区别**

**1.传统`I/O`**
硬盘 --> 内核缓冲区 --> 用户缓冲区 --> 内核`socket`缓冲区—>网卡

**2.`sendfile(in,out)`**
硬盘 --> 内核缓冲区 --> 内核`socket`缓冲区 --> 网卡

**3.`sendfile`(`DMA` 收集拷贝)**
硬盘 --> 内核缓冲区  [--> 仅将用以表述缓存地址及长度的文件描述信息 放至内核`socket`缓冲区] --> 网卡

**注意点**

1.  不可传输大于`2G`的文件
2. 不支持向量化传输(诸如`Samba`和`Apache`这样的服务器不得不使用`TCP_COKR`标志来执行多个`sendfile`调用)
3. 缺少标准化的实现, 各系统之间实现差异过大, 可移植性差
4. 需要硬件以及驱动程序支持, 例如: 网络适配器支持聚合操作特性(即`DMA`)
5. 不允许进程对文件内容作进一步的加工, 比如压缩数据再发送, 需要提前做好压缩, 降低可读性



#### 9.8.3 来自`PageCache`的限制

零拷贝技术是基于`PageCache`的, `PageCache`会缓存最近访问的数据, 提升了访问缓存数据的性能, 同时, 为了解决机械硬盘寻址慢的问题, 它还协助`I/O`调度算法实现了`IO`合并与预读, 这也是顺序读比随机读性能好的原因. 这些优势, 进一步提升了零拷贝的性能. 

在了解`sendfile`和`mmap`之后, 我们发现有一个不得大于`2G`的限制, 这就是由于`PageCache`所造成的, 我们知道内存很小, 即使通过虚拟内存的`PageCache`技术来实现, 当我们将系统分页设置为`1G`时, 可以满足大部分的需求, 但, 这依旧会存在更大的数据;

而大文件难以命中`PageCache`缓存, 而且会占满`PageCache`, 后续大量的小文件访问却始终无法有机会获取, 导致**热点**文件无法充分利用, 从而增大了性能开销.

对此`nginx`的做法是:

1. 大文件通过异步`I/O`, 或者叫直接`I/O`
2. 小文件通过`DMA`零拷贝

**正常同步读取磁盘文件具体过程:**

- 当调用`read`方法时, 会阻塞着, 此时内核会向磁盘发起`I/O`请求, 磁盘收到请求后, 便会寻址, 当磁盘数据准备好后, 就会向内核发起`I/O`中断, 告知内核磁盘数据已经准备好;
- 内核收到`I/O`中断后, 就将数据从磁盘控制器缓冲区拷贝到`PageCache`里;
- 最后, 内核再把`PageCache`中的数据拷贝到用户缓冲区, 于是`read`调用就正常返回了.

**异步`I/O`具体过程:**

- 前半部分, 内核向磁盘发起读请求, 但是可以**不等待数据就位就可以返回**, 于是进程此时可以处理其他任务;
- 后半部分, 当内核将磁盘中的数据拷贝到进程缓冲区后, 进程将接收到内核的**通知**, 再去处理数据;

我们可以发现, 异步`I/O`并没有涉及到`PageCache`, 所以使用异步`I/O`就意味着要绕开`PageCache`.

绕开`PageCache`的`I/O`也叫**直接`I/O`**, 使用`PageCache`的`I/O`则叫**缓存`I/O`**.  

通常, 对于磁盘, 异步`I/O`只支持直接`I/O`.



#### 9.8.4 `splice`

`sendfile`作用于网卡发送磁盘文件, 那么假设我存在内部的数据同步呢? 这个时候我们可以使用`splice `来解决;

其原理是一致的, 只是去除了`sendfile`的作用限制, 针对不特定的, 任意的两个文件符之间的拷贝;

`splice`可以在任意两个文件描述符之间传输数据(如`socket`到`socket`), 但输入和输出文件描述符必须有一个是`pipe`. 也就是说如果你需要从一个`socket`传输数据到另外一个`socket`, 是需要使用**pipe**来做为**中介**的, 等于要调用两个`splice`才能把数据从一个`socket`移到另外一个`socket`

但由于依旧借用了`linux`的管道缓冲机制, 所以, 它的两个文件描述符参数中至少有一个必须是管道设备;

`splice`提供了一种流控制的机制, 通过预先定义的水印(`watermark`)来阻塞写请求, 有实验表明, 利用这种方法将数据从一个磁盘传输到另外一个磁盘会增加`30%-70%`的吞吐量, `CPU`压力也会减少一半.

**注意点**

1. 同样只适用于不需要用户态处理的程序, 无法对数据二次处理
2. 传输描述符至少有一个是管道设备



#### 9.8.5 `tee`

`tee` 和 `splice` 是 Linux 系统提供的两个系统调用, 它们的功能相似, 但也有一些区别. 

`tee` 系统调用可以将一个文件描述符中的数据复制到另一个文件描述符中, 并且还可以将数据复制到文件或管道中, 同时还保留原始数据的副本. `tee` 的主要用途是在管道中实现数据的多路复制, 可以使得多个进程同时处理相同的数据, 从而提高系统的性能. 

相比之下, `splice` 系统调用的功能更为灵活, 它可以将数据从一个文件描述符中移动到另一个文件描述符中, 而不是像 `tee` 那样只是简单的复制数据. 此外, `splice` 还可以将数据直接从一个文件描述符中移动到另一个文件描述符中, 而不需要将数据复制到用户空间, 从而避免了数据复制所带来的性能开销. 

总的来说, **`tee` 主要用于在管道中实现数据的多路复制, 而 `splice` 则主要用于在文件描述符之间高效地移动数据**. 它们都是非常有用的系统调用, 可以帮助开发人员在 Linux 系统中高效地处理数据. 

`tee`与`splice`的代码示例: https://blog.csdn.net/lk_wkqd/article/details/50244463

https://blog.csdn.net/dupengchuan/article/details/51184463?spm=1001.2101.3001.6650.1&utm_medium=distribute.pc_relevant.none-task-blog-2%7Edefault%7ECTRLIST%7ERate-1-51184463-blog-73522302.235%5Ev32%5Epc_relevant_default_base&depth_1-utm_source=distribute.pc_relevant.none-task-blog-2%7Edefault%7ECTRLIST%7ERate-1-51184463-blog-73522302.235%5Ev32%5Epc_relevant_default_base&utm_relevant_index=2



#### 9.8.6 小结

在发送文件至网卡的情况下:

|     拷贝方式      | `CPU`拷贝 | `DMA`拷贝 |   系统调用   | 上下文切换 |
| :---------------: | :-------: | :-------: | :----------: | :--------: |
| 传统方式读写磁盘  |     2     |     2     | `read/write` |     4      |
|  `mmap`磁盘映射   |     1     |     2     | `mmap/write` |     4      |
| `sendfile(无DMA)` |     1     |     2     |  `sendfile`  |     2      |
|  `sendfile(DMA)`  |     0     |     2     |  `sendfile`  |     2      |
|     `splice`      |     0     |     2     |   `splice`   |     2      |



#### 9.8.7 写时复制技术(`copy-on-write`, `COW`)

当父进程`fork`生成子进程时, 会复制它的所有内存页. 这至少会导致两个问题: 

1. 消耗大量内存
2. 复制操作消耗时间

特别是fork后使用exec加载新程序时, 由于会初始化内存空间, 所以复制操作几乎是多余的. 使用`cow`技术, 使得在`fork`子进程时不复制内存页, 而是共享内存页(也就是说, 子进程也指向父进程的物理空间), 只有在该子进程需要修改某一块数据, 才会将这一块数据拷贝到自己的`app buffer`中并进行修改, 那么这一块数据就属于该子进程的私有数据, 可随意访问, 修改, 复制. 

这在一定程度上实现了零复制, 即使复制了一些数据块, 也是在逐渐需要的过程进行复制的.

### 9.9 共享内存

#### 9.9.1 `mmap`实现

   `mmap`可以提供内存与磁盘文件的映射,  那么假设两个进程同时访问同一个磁盘文件, 即可实现两个进程拥有相同的内存映射, 是共享内存的实现方式之一. 

​    优点: 储存量可以很大(多于主存)

​    缺点: 进程间读取和写入速度要比主存的要慢, 以及[9.8.1 `mmap`](#9.8.1 `mmap`)中的注意项

#### 9.9.2 `shm`内存实现

  与`mmap`不同, `shm`是直接映射物理主存, 就是我们说的真的内存上. 

   优点: 进程间访问速度(读写) 比磁盘要快

   缺点: 储存量不能非常大, 最多也不能高于主存, 还要给系统留空间

### 9.10 其他小技巧

1. 提前定义减少运算的强度

   ```c++
   long factorial(int i) { // 阶乘
       if (i != 0)
           return i * factorial(i - 1);
       return 1;
   }
   ```

   改为

   ```c++
   constexpr long factorial_table[] = { 1, 1, 2, 6, 24, 120, 720  /* ... */  };
   long factorial(int i) { // 阶乘
       return factorial_table[i];
   }
   ```

   如果表很大, 不好写, 就写一个`init`函数,在启动时生成表格. 

2. 避免不必要的整数除法
   整数除法是整数运算中最慢的, 所以应该尽可能避免

   ```c++
   a = a / 4;
   // 可以改为
   a = a >> 2;
   
   d = a / b / c;
   // 可以改为
   d = a / (b * c);
   ```

3. 使用复合赋值表达式

   ```c++
   a=a+b; 
   // 可以改为
   a+=b;
   ```

4. `switch`/ `if`语句中根据发生频率来进行排序

   ```c++
   int days_in_month, short_months, normal_months, long_months; 
   switch (days_in_month) 
   { 
   　　case 31: 
   　　　　long_months++; // 31天的和30天出现频率较高
   　　　　break; 
   　　case 30: 
   　　　　normal_months++; 
   　　　　break; 
   　　case 28: 
   　　case 29: 
   　　　　short_months++; // 28或者29天出现频率低
   　　　　break; 
   　　default: 
   　　　　cout << “month has fewer than 28 or more than 31 days” << endl; 
   　　　　break; 
   }
   ```

   `C++20`中可以添加关键字`[[likely]]/[[unlikely]]`来主动声明优先级

5. 将大的switch语句转为嵌套switch语句

   ```c++
   pMsg=ReceiveMessage(); 
   switch (pMsg->type) 
   { 
   case FREQUENT_MSG1: 
       handleFrequentMsg(); 
       break; 
   case FREQUENT_MSG2: 
       handleFrequentMsg2(); 
       break;
   case FREQUENT_MSGn: 
       handleFrequentMsgn(); 
       break; 
   default: //嵌套case部分用来处理不经常发生的消息 
       switch (pMsg->type) 
       { 
       case INFREQUENT_MSG1: 
           handleInfrequentMsg1(); 
           break; 
       case INFREQUENT_MSG2: 
           handleInfrequentMsg2(); 
           break;
       case INFREQUENT_MSGm: 
           handleInfrequentMsgm(); 
           break; 
       } 
   } 
   ```

6. 如果switch中每一种情况下都有很多的工作要做,那么把整个switch语句用一个指向函数指针的表来替换会更加有效

   ```c++
   enum MsgType{Msg1, Msg2, Msg3} 
   switch (ReceiveMessage() )
   { 
   case Msg1; 
   ... 
   case Msg2; 
   ...
   case Msg3; 
   ...
   } 
   
   // 替换为
   
   //准备工作 
   int handleMsg1(void); 
   int handleMsg2(void); 
   int handleMsg3(void); 
   //创建一个函数指针数组 
   int (*MsgFunction [])()={handleMsg1, handleMsg2, handleMsg3}; // 函数指针数组 返回值为int类型, 输入类型无
   //用下面这行更有效的代码来替换switch语句 
   status=MsgFunction[ReceiveMessage()]();
   ```

7. `for`与`if`交换

   ```c++
   for ( ... ) { 
   　　if( CONSTANT0 ) { // 循环内部, 判断会执行多次
   　　　　DoWork0();     // 假设这里不改变CONSTANT0的值 
   　　}  else  { 
   　　　　DoWork1();     // 假设这里不改变CONSTANT0的值 
   　　} 
   } 
   
   // 替换为
   if( CONSTANT0 ) {
       for ( ... )
           DoWork0();
   } else {
       for ( ... )
           DoWork1();
   }
   ```

   **当然前面也提到CPU分支预测, 只是这么写可以减少对CPU分支检测的依赖, 所以替不替换都可以**

8. `for (;;) ` 替代`while(true)`

9. ANSI规范

   ```c++
   a = b / c * d;
   f = b * g / c;
   
   // 可改成
   a = b / c * d;
   f = b / c * g;
   // 当编译器满足ANSI规范时,  b/c仅被计算一次
   ```

10. 尽可能使用常量`const`
    C++标准规定, 如果一个`const`声明的对象的地址不被获取, 允许编译器不对它分配储存空间.

11. 把本地函数声明为静态的`static`
    如果一个函数只在实现它的文件中被使用, 把它声明为静态的`static`以强制使用内部连接. 否则, 默认的情况下会把函数定义为外部连接. 这样可能会影响某些编译器的优化, 比如, 自动内联.

12. **当然, 最直接高效的, 便是使用更好的算法**
    但是, 事实上大部分业务都是通用算法, 我很少在同类公司中看到大家使用不同的算法, 除非这就是做算法的公司(即使如此, 基础库也都差不多)
    所以各位在代码基础薄弱的情况下, 更需要的是找到合适的工具库, 而平时能够做到对各类算法有分类概念, 能够在遇到问题后立即想到对应的算法方案即可;(觉得算法太差的, 可以刷`leetcode`每日一题)
    而假使遇到连`baidu` + `github` + `Stack Overflow`都无法解决, 又属于当前无法避免的算法问题, 那么应当向直属领导提出问题, 及时成立专门的算法团队, 由团队提供对应的支持, 来确保公司未来业务的稳健拓展.

13. `atoi`等安全转化在确保逻辑安全的情况下, 采用自定义简化函数

    ```cpp
    int unsafe_atoi(const char* str) {
        while (*str == ' ') ++str; // skip leading whitespace
        int sign = 1;
        switch (*str) { // handle leading +/-
        case '-': ++str; sign = -1; break;
        case '+': ++str; break;
        }
        int value = 0;
        while (char c = *str++) {
            if (c <= '9' && c >= '0') {
                value *= 10;
                value += c - '0';
                continue;
            }
            break;
        }
        return value * sign;
    }
    ```

    

## 10. 一些延伸讨论

### 10.1 基础入门

1. C++入门 https://robot.czxy.com/docs/cpp/
2. C++11 介绍 https://www.cnblogs.com/skyfsm/p/9038814.html
3. C++17 高性能分析
   https://github.com/PacktPublishing/Cpp-High-Performance
   https://github.com/Ewenwan/Cpp-High-Performance
   https://github.com/PacktPublishing/Cpp17-STL-Cookbook
4. 数据结构与算法动态可视化 https://visualgo.net/zh
5. 响应式编程 https://github.com/Ewenwan/CPP-Reactive-Programming
6. Template 进阶指南 https://github.com/wuye9036/CppTemplateTutorial

### 10.2 简化代码三方库

1. folly解读系列  https://www.zhihu.com/column/c_1327270420144570368
2. folly `RWSpinlock` 主动出让CPU简单介绍 https://kb.cnblogs.com/page/151459/
   同样道理, 适用于因无锁队列`while(pop)`造成的无限循环时, 让人觉得不那么舒服的资源浪费, 而且, `while`刷盘伤,机子跑几年就得换.
3. folly原文解析 https://github.com/facebook/folly/tree/master/folly/docs
4. 任务流 https://github.com/taskflow/taskflow
5. 高仿`GO`风格协程 https://github.com/yyzybb537/libgo
6. 各类`Cpp`推荐库 https://github.com/fffaraz/awesome-cpp

### 10.3 低延时物理外挂

#### 10.3.1 绑核将网卡和应用绑定在同一个`numa`中

 https://bbs.huaweicloud.com/blogs/147393

#### 10.3.2 `rdma`特制网卡

在之前 [8.9 内存旁路技术](#8.9-内核旁路技术 )中有提到, 内核旁路存在最大的问题在于, 依旧使用`CPU`拷贝, 只是独占了网卡和绑核等操作来达到快速的目的. 优势在于协议可控;

但是我们更多的场景下, 很多业务存在波峰波谷的特性, 假设独占网卡与核心, 则会导致`CPU`空转的情况, 故而大部分情况下, 我们会采用`RDMA`等技术, 当然, 在`RDMA`上, 我们依旧可以采用绑核等相关操作;

但是在了解`RDMA`之前, 我们还是需要先了解下网卡的工作原理.

在最早的时候, 网卡是外设, 外部设备要想和主机建立数据, 首先要靠`CPU`把数据缓存至内核区, 然后再靠`CPU`把数据从内核区拷贝至用户区, 这样就需要拷贝两次.

```
-----------------------------------------
|                 CPU                   |
-----------------------------------------
  |        ^    |                /|\
  |(7)     ^(6) |(3)              | (2)
 \|/       ^   \|/                |
-----------------------------------------
|                 BUS                   |
-----------------------------------------
  |        ^    |                /|\
  |(8)     ^(5) |(4)              | (1)
 \|/       ^   \|/                |
-------------------       -------------------
|     memory      |       |   nic(网卡)      |
|  user |  kernel |       |                 |
-------------------       -------------------
```

然而`CPU`的优势在于计算上, 用于拷贝就是浪费, 为了高效利用, 于是大家设计了`DMA`, 全称为`Direct Memory Access`, 即直接内存访问.意思是`I/O`外设对内存的读写过程可以不用`CPU`参与而直接进行.

有了`DMA`之后, 当网卡需要数据时, 除了一些必要的`CPU` 控制启停命令外, 外设至内核区的数据拷贝全部交给这个全新的组件来完成, 通过`BUS`总线获取内存数据, 放入`DMA`的寄存器中, 再复制到`I/O`设备的存储空间中.

**`DMA`控制器一般是和`I/O`设备在一起的, 也就是说一块网卡中既有负责数据收发的模块, 也有`DMA`模块.**

小结一下:

**1. 网卡包含`I/O`模块, 以及`DMA`模块;**

**2. `DMA`负责内存数据拷贝, `CPU`控制网卡启停, 中断的处理, 报文的封装和解析;**

**3. 发送消息时, `CPU`将 [用户内存空间数据] 拷贝至 [内核空间的缓冲区], 交由`DMA`通过`BUS`映射发送; **

**4. 接收消息时, `DMA`通过`BUS`映射回 [内核空间的缓冲区], `CPU`将数据拷贝至 [用户内存空间] 中, 供上层使用;**

**注意`TCP/UDP`的协议是在`CPU`拷贝阶段添加/解析的.**

```
--------------
|     CPU    |
--------------
 /|\       v
  |(2)     v(3)
  |        v
-----------------------------------------
|                 BUS                   |
-----------------------------------------
 /|\       v   /|\   DMA copy     |
  |(1)     v(4) |(5)              | (6)
  |        v    |                \|/
-------------------       -------------------
|     memory      |       |   nic(网卡)      |
|  user |  kernel |       |                 |
-------------------       -------------------
```

那么这个时候聪明的各位应该可以想到一个很简单的问题, 既然已经有`DMA`了, 为什么还需要`CPU`来插一脚, 我们可以直接丢掉`CPU`, 自己直接映射进内存不就好了, 包括`linux`在内的系统, 为了数据安全, 将内存区分了内核和用户两大分区, 用户不得修改内核数据, 以此确保系统稳健运行, 互联网为了安全, `DMA`的拷贝也都是进入内核区进行缓存, 交由`CPU`来统一控制, 最终将数据拷贝至用户可读写的用户区, 那么假设, 我们明确了解自己的网络情况, 是否可以直接将数据拷贝至用户区来读写呢?

**即I/O设备直接访问读写用户区内存**

当然, 业界的各位专家也自然想到了, 这就是`RDMA(Remote Direct Memory Access)`, 意为远程直接地址访问, 

```
-----------------------------------------
|                 BUS                   |
-----------------------------------------
  |            DMA copy           /|\
  |(2)                             | (1)
 \|/                               |
-------------------       -------------------
|     memory      |       |   RDMA网卡       |
|  user |  kernel |       |                 |
-------------------       -------------------
```

当然, `CPU`在这里还是要控制启停的, 但是对于数据的拷贝却全程不参与, 而`TCP/UDP`的协议解析/封装/校验等操作, 也移交至`RDMA`网卡内部校验, 最后数据依旧依靠`DMA`组件进行内存拷贝, 只是这次直接拷贝入用户内存空间中

更多零拷贝机制推荐参考: https://www.jianshu.com/p/e76e3580e356

`rdma`中的各项技术由于扩展宽泛, 如有兴趣可自行搜索

 

#### 10.3.2 内存屏障(`Memory barrier`)

由于`CPU`乱序执行, 导致`CPU`在执行期间并不一定按照代码的先后顺序完全执行

`CPU`作为最核心的中央处理单元, 他的性能直接关乎着计算机的运行性能, 最早的时候是单`CPU`单核心, 后来`AMD`提出原生多核的概念, 即每个核心之间都是完全独立的, 都拥有自己的前端总线, 不会造成冲突, 即使在高负载状况下, 每个核心都能保证自己的性能不受太大的影响. 

通俗的说, 原生多核的抗压能力强, 但是需要先进的工艺, 每扩展一个核心都需要很多的研发时间.

所以`Intel`提出了封装多核的概念, 既然多个`CPU`独立运行有困难, 那我把多个`CPU`一起运行某一段那就快速了, 其实质就是把多个单核直接封装在一起, 但多核心只能共同拥有一条前端总线, 在所有核心满载时, 核心之间会争抢前端总线, 导致性能大幅度下降, 所以早期的PD被扣上了“高频低能”的帽子.

要提高封装多核的性能, 在多任务的高压下尽量减少性能损失,  只能不断的扩大前端总线的总体大小, 来弥补多核心争抢资源带来的性能损失, 但这样做只能在一定程度上弥补性能的不足, 和原生的比起来还是差了很多, 而且后者成本比较高.  封装多核的优点在于多核心的发展要比原生快的多. 也为超频提供了基础.

封装多核是将多个`CPU`芯片集成在一个芯片里, 双核就是2个, 四核就是4个, 统一对外执行, 于是为了协调这些核心, 又新增了**多核心处理器**, 通过多核心处理器分派任务, 来达到多个统一芯片内的`CPU`并发执行任务的目的.

而同时, 一个核心能同时处理2条指令, 所以可以模拟出2个线程. 

故而双核就是: 一个拥有2个`CPU core`芯片的集成CPU, 每个`core`可以模拟2线程, 达到4线程的效果;

这也带来了问题: **多核系统中, `A`处理器修改了位置`a, b`两处的内容, 同步到`B`处理器的顺序, 是不确定的. 所以在看起来似乎是乱序的.** `A`先修改`a`, 再修改`b`, 但是在`B`处理器上, 可能先看到`b`被改变, 然后才是`a`的改变;

这种情况, 在大量通过内存进行通讯实现控制流程的代码逻辑而言, 是不可接受的. 这个时候, 就需要内存屏障来解决问题了



具体原理可以参考: https://www.kernel.org/doc/Documentation/memory-barriers.txt

文中提出, 内存与`CPU`交互的抽象原型为:

```
        +-------+   :   +--------+   :   +-------+
		| CPU 1 |<----->| Memory |<----->| CPU 2 |
		+-------+   :   +--------+   :   +-------+
		    ^       :       ^        :       ^
		    |       :       |        :       |
		    |       :       v        :       |
		    |       :   +--------+   :       |
		    +---------->| Device |<----------+
		            :   +--------+   :
```

每个 `CPU` 执行一个程序, 产生内存访问操作. 在抽象的 `CPU` 中, 内存操作的顺序是非常宽松的, 并且 `CPU` 实际上可以按照它喜欢的任何顺序执行内存操作, 前提是程序因果关系似乎保持不变. 同样, 编译器也可以按照它喜欢的任何顺序排列它发出的指令, 前提是它不影响程序的明显操作. 

因此, 在上图中, `CPU`执行的内存操作的影响会被系统的其余部分感知, 因为这些操作跨越了 `CPU` 和系统其余部分之间的接口(虚线). 例如, 请考虑以下事件序列：

```
	CPU 1			CPU 2
	===============	===============
	{ A == 1; B == 2 }
	A = 3;			x = B;
	B = 4;			y = A;
```

```
从中间的内存系统看到的访问集可以排列成24 种不同的组合：
	STORE A=3,	STORE B=4,		y=LOAD A->3,	x=LOAD B->4
	STORE A=3,	STORE B=4,		x=LOAD B->4,	y=LOAD A->3
	STORE A=3,	y=LOAD A->3,	STORE B=4,		x=LOAD B->4
	STORE A=3,	y=LOAD A->3,	x=LOAD B->2,	STORE B=4
	STORE A=3,	x=LOAD B->2,	STORE B=4,		y=LOAD A->3
	STORE A=3,	x=LOAD B->2,	y=LOAD A->3,	STORE B=4
	STORE B=4,	STORE A=3,		y=LOAD A->3,	x=LOAD B->4
	STORE B=4, ...
	...
```

```
因此可以产生四种不同的值组合：
	x == 2, y == 1
	x == 2, y == 3
	x == 4, y == 1
	x == 4, y == 3
```

```
此外, 由于一个 CPU 提交给内存系统的存储可能不会被另一个 CPU 以与提交存储相同的顺序执行的负载感知. 
再举一个例子, 考虑以下事件序列：
	CPU 1			CPU 2
	===============	===============
	{ A == 1, B == 2, C == 3, P == &A, Q == &C }
	B = 4;			Q = P;
	P = &B;			D = *Q;
这里有一个明显的数据依赖性, 因为加载到 D 中的值取决于 CPU2 从 P 检索到的地址.  在序列的末尾, 以下任何结果都是可能的：
	(Q == &A) and (D == 1)
	(Q == &B) and (D == 2)
	(Q == &B) and (D == 4)
请注意, CPU2 永远不会尝试将 C 加载到 D 中, 因为 CPU 会在发出 *Q 加载之前将 P 加载到 Q 中. 
```

```
设备操作
-----------------
一些设备将它们的控制接口呈现为存储位置的集合, 但访问控制寄存器的顺序非常重要.  例如, 假设有一组内部寄存器的以太网卡, 这些寄存器通过地址端口寄存器 (A) 和数据端口寄存器 (D) 进行访问.  要读取内部寄存器 5, 可以使用以下代码：
	*A = 5;
	x = *D;
但这可能会显示为以下两个序列之一：
	STORE *A = 5, x = LOAD *D
	x = LOAD *D, STORE *A = 5
第二个几乎肯定会导致故障, 因为它在尝试读取寄存器之后设置了地址. 
```

```
GUARANTEES
----------
CPU 可能有一些最低限度的保证：
 (*) 在任何给定的 CPU 上, 依赖内存访问将按顺序发布, 相对于它自己. 这意味着对于：
 		Q = READ_ONCE(P); D = READ_ONCE(*Q);
 	CPU 将发出以下内存操作：
   		Q = LOAD P, D = LOAD *Q
   	并且总是按照这个顺序. 但是, 在 DEC Alpha 上, READ_ONCE() 也会发出内存屏障指令, 因此 DEC Alpha CPU 将改为发出以下内存操作：
   		Q = LOAD P, MEMORY_BARRIER, D = LOAD *Q, MEMORY_BARRIER
   	无论是否在 DEC Alpha 上, READ_ONCE() 还可以防止编译器的恶作剧. 
   	
  (*) 特定 CPU 内的重叠加载和存储将在该 CPU 内进行排序. 这意味着对于：
  		a = READ_ONCE(*X); WRITE_ONCE(*X, b);
  	CPU 只会发出以下内存操作序列：
  		a = LOAD *X, STORE *X = b
  	对于：
  		WRITE_ONCE(*X, c); d = READ_ONCE(*X);
  	CPU 只会发出：
  		STORE *X = c, d = LOAD *X
  	(如果加载和存储针对重叠的内存片段, 则它们会重叠). 
  	
并且_must_或_must_not_假设了许多事情：
  (*) _must_not_ 假设编译器将对不受 READ_ONCE() 和 WRITE_ONCE() 保护的内存引用执行您想要的操作. 没有它们, 编译器就有权进行各种“创造性”转换, 这些转换在编译器屏障部分中有介绍. 
  (*) _must_not_假设独立的加载和存储将按照给定的顺序发出. 这意味着对于：
  		X = *A; Y = *B; *D = Z;
  	我们可能会得到以下任何序列：
  		X = LOAD *A,  Y = LOAD *B,  STORE *D = Z
		X = LOAD *A,  STORE *D = Z, Y = LOAD *B
		Y = LOAD *B,  X = LOAD *A,  STORE *D = Z
		Y = LOAD *B,  STORE *D = Z, X = LOAD *A
		STORE *D = Z, X = LOAD *A,  Y = LOAD *B
		STORE *D = Z, Y = LOAD *B,  X = LOAD *A
  (*)_must_假设重叠的内存访问可以合并或丢弃. 这意味着对于：
  		X = *A; Y = *(A + 4);
  	我们可能会得到以下任一序列：
  		X = LOAD *A; Y = LOAD *(A + 4);
		Y = LOAD *(A + 4); X = LOAD *A;
		{X, Y} = LOAD {*A, *(A + 4) };
	对于：
		*A = X; *(A + 4) = Y;
	我们可能会得到：
		STORE *A = X; STORE *(A + 4) = Y;
		STORE *(A + 4) = Y; STORE *A = X;
		STORE {*A, *(A + 4) } = {X, Y};
		
还有anti-guarantees：
  (*) 这些guarantees不适用于位域, 因为编译器通常会生成代码来使用non-atomic read-modify-write sequences来修改它们. 不要尝试使用位域来同步并行算法. 
	
  (*) 即使在位域受锁保护的情况下, 给定位域中的所有字段也必须受一个锁保护. 如果给定位域中的两个字段受不同锁保护, 则编译器的non-atomic read-modify-write sequences可能会导致对一个字段的更新破坏相邻字段的值. 

  (*) 这些guarantees仅适用于Properly aligned(正确对齐)和Properly sized(大小合适)的标量变量.  “Properly sized”目前意味着与“char”/“short”/“int”和“long”大小相同的变量.  “Properly aligned”表示自然对齐, 因此“char”没有限制, “short”为两字节对齐, “int”为四字节对齐, “long”为四字节或八字节对齐, 分别在 32 位和 64 位系统上. 请注意, 这些guarantees已引入 C11 标准, 因此在使用旧的 C11 之前的编译器(例如 gcc 4.6)时要小心. 包含此guarantees的标准部分是第 3.14 节, 它定义了“memory location”如下：
 	memory location
 		任何一个scalar type的对象, 或所有具有非零宽度的a maximal sequence of adjacent bit-fields
 		注 1：两个执行线程可以更新和访问单独的memory locations, 而不会相互干扰. 
 		注 2：bit-fields和相邻的non-bit-field成员位于不同的memory locations. 这同样适用于两个bit-fields, 如果一个在嵌套结构声明中声明而另一个不在, 或者如果两者由zero-length bit-field声明分隔, 或者如果它们由non-bit-field成员声明. 如果在两个bit-fields之间声明的所有成员也是bit-fields, 那么在同一结构中同时更新两个bit-fields是不安全的, 无论这些中间bit-fields的大小是多少. 
```

**解决方法及相关探讨可以参考: http://www.wowotech.net/kernel_synchronization/memory-barrier.html**

`linux kernel`的`memory barrier`相关的`API`列表如下：

| 接口名称    | 作用                                                         |
| ----------- | ------------------------------------------------------------ |
| `barrier()` | 优化屏障, 阻止编译器为了进行性能优化而进行的memory access reorder |
| `mb()`      | 内存屏障(包括读和写), 用于`SMP`和`UP`                        |
| `rmb()`     | 读内存屏障, 用于`SMP`和`UP`                                  |
| `wmb()`     | 写内存屏障, 用于`SMP`和`UP`                                  |
| `smp_mb()`  | 用于`SMP`场合的内存屏障, 对于`UP`不存在`memory order`的问题(对汇编指令), 因此, 在`UP`上就是一个优化屏障, 确保汇编和c代码的`memory order`是一致的 |
| `smp_rmb()` | 用于`SMP`场合的读内存屏障                                    |
| `smp_wmb()` | 用于`SMP`场合的写内存屏障                                    |

`barrier()`这个接口和编译器有关, 对于`gcc`而言, 其代码如下：

```
#define barrier() __asm__ __volatile__("": : :"memory")
```

同时比对一下`volatile`与`Memory Barrier`与`atomic`之间的关系, 参考: https://gaomf.cn/2020/09/11/Cpp_Volatile_Atomic_Memory_barrier/

结论是:

|                | `volatile` | `Memory Barrier` | `atomic` |
| :------------: | :--------: | :--------------: | -------- |
| 抑制编译器重排 |    Yes     |       Yes        | Yes      |
| 抑制编译器优化 |    Yes     |        No        | Yes      |
| 抑制 CPU 乱序  |     No     |       Yes        | Yes      |
| 保证访问原子性 |     No     |        No        | Yes      |













