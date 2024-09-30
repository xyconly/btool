/*
* purpose: 实现平台无关的读写锁
  Note:
*/
# pragma once

#ifdef __NO_LOCK__
struct rwMutex{};
struct readLock{readLock(rwMutex&) {}};
struct writeLock{writeLock(rwMutex&) {}};
#else
# if defined(_HAS_CXX17) || (__cplusplus >= 201703L)
# include <shared_mutex>
# include <mutex>
    typedef std::shared_timed_mutex                     rwMutex;
    typedef std::shared_lock<std::shared_timed_mutex>   readLock;
    typedef std::unique_lock<std::shared_timed_mutex>   writeLock;
# else
    #if defined(_WIN32) || defined(_WIN64) || defined(WIN32) || defined(WIN64)
    # include <concrt.h>
        typedef Concurrency::reader_writer_lock                     rwMutex;
        typedef Concurrency::reader_writer_lock::scoped_lock_read   readLock;
        typedef Concurrency::reader_writer_lock::scoped_lock        writeLock;
    #else
    # include <boost/thread/mutex.hpp>
    # include <boost/thread/shared_mutex.hpp>
        typedef boost::shared_mutex            rwMutex;
        typedef boost::unique_lock<rwMutex>    writeLock;
        typedef boost::shared_lock<rwMutex>    readLock;
    #endif
# endif
#endif
