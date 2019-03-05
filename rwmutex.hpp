/*
* purpose: 实现平台无关的读写锁
*/
# pragma once

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
