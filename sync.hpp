/******************************************************************************
 * File: synch.hpp
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  XXXX makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 *****************************************************************************/

#ifndef __SYNCH_H__
#define __SYNCH_H__

#include <iostream>
#include <assert.h>
using namespace std;

#ifdef _MSC_VER
//#  define _WINSOCKAPI_   // prevent inclusion of winsock.h, since we need winsock2.h

#ifndef _WIN32_WINNT		// 允许使用特定于 Windows XP 或更高版本的功能。
# define _WIN32_WINNT 0x0501	// 将此值更改为相应的值，以适用于 Windows 的其他版本。
#endif						

#ifndef _WIN32_WINDOWS		// 允许使用特定于 Windows 98 或更高版本的功能。
# define _WIN32_WINDOWS 0x0410 // 将此值更改为适当的值，以指定将 Windows Me 或更高版本作为目标。
#endif

#ifndef WIN32_LEAN_AND_MEAN
# define WIN32_LEAN_AND_MEAN		// 从 Windows 头中排除极少使用的资料
#endif
//#  define _WINSOCK2API_

#  if !defined(_WINDOWS_)
#	 include "pointerdef.h"				// 因下面的文件用到"winnt.h"
#    include <windows.h>
#    include <winbase.h>
#  endif

#elif defined(__GNUC__)
#  include <pthread.h>
#  include <unistd.h>
#  include <sys/time.h>
#  include <errno.h>
#  ifndef __bsdi__
#    include <semaphore.h>
#  endif
#else
#  error Unknown compiler!
#endif


namespace BTool
{

#ifdef _MSC_VER
	typedef HANDLE handle;
	enum {
		max_handler_array_size = MAXIMUM_WAIT_OBJECTS,		// for winnt.h
	};
#elif defined(__GNUC__)
	typedef int handle;
	enum {
		max_handler_array_size = 20,
	};
#endif

	class handler_impl
	{
	public:
		virtual ~handler_impl() {}
		virtual void sync() = 0;
		virtual operator handle() = 0;    
	};

	class handler
	{
	public:
		handler( handler_impl* impl = NULL ) : m_impl( impl ) {}
		handler( handler_impl& impl ) : m_impl( &impl ) {}
		void sync() { m_impl->sync(); }
		operator handle() { return m_impl->operator handle(); }

	private:
		handler_impl* m_impl;
	};

	///////////////////////////////////////////////////////////////////////////////
	// mutex

#ifdef _MSC_VER

	class mutex
	{
	public:
		mutex()			{ InitializeCriticalSection(&m_cs); }
		~mutex()		{ DeleteCriticalSection(&m_cs); }
		void lock()		{ EnterCriticalSection(&m_cs); }
		void unlock()	{ LeaveCriticalSection(&m_cs); }
		//#if(_WIN32_WINNT >= 0x0400)
		//    bool trylock()	{ return TryEnterCriticalSection(&m_cs) != FALSE; }
		//#endif

	protected:
		CRITICAL_SECTION m_cs;
	};

#elif defined(__GNUC__)

#if defined(_XOPEN_SOURCE) && (_XOPEN_SOURCE >= 500)
	class mutex
	{
	public:
		mutex()			
		{
			pthread_mutexattr_t attr;
			pthread_mutexattr_init( &attr );
			pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_RECURSIVE );
			pthread_mutex_init( &m_mtx, &attr ); 
			pthread_mutexattr_destroy( &attr );
		}
		~mutex()		{ pthread_mutex_destroy(&m_mtx); }
		void lock()		{ pthread_mutex_lock(&m_mtx); }
		void unlock()	{ pthread_mutex_unlock(&m_mtx); }
		bool trylock()	{ return pthread_mutex_trylock(&m_mtx) == 0; }

	protected:
		pthread_mutex_t m_mtx;
	};
#else
	class mutex
	{
	public:
		mutex() : m_count(0), m_locked(false)
		{
			pthread_mutex_init( &m_mtx, 0 );
			pthread_cond_init( &m_unlocked, 0 );
		}
		~mutex()            
		{ 
			pthread_mutex_destroy( &m_mtx ); 
			pthread_cond_destroy( &m_unlocked ); 
		}
		void lock()         
		{ 
			pthread_mutex_lock( &m_mtx ); 
			pthread_t tid = pthread_self();
			if( m_locked && pthread_equal(m_tid, tid) )
				++m_count;
			else
			{
				while( m_locked )
				{
					pthread_cond_wait( &m_unlocked, &m_mtx );
				}
				m_tid = tid;
				m_count = 1;
				m_locked = true;
			}
			pthread_mutex_unlock( &m_mtx );
		}
		void unlock()       
		{ 
			pthread_mutex_lock( &m_mtx );

			pthread_t tid = pthread_self();
			if( m_locked && !pthread_equal( m_tid, tid ) )
			{
				pthread_mutex_unlock( &m_mtx );
			}

			if( --m_count == 0 )
			{
				m_locked = false;

				pthread_cond_signal( &m_unlocked );
			}

			pthread_mutex_unlock( &m_mtx );
		}
		bool trylock()      
		{ 
			bool ret = false;
			pthread_mutex_lock( &m_mtx );
			pthread_t tid = pthread_self();
			if( m_locked && pthread_equal(m_tid, tid) )
			{
				++m_count;
				ret = true;
			}
			else if( !m_locked )
			{
				m_tid = tid;
				m_count = 1;
				m_locked = true;
				ret = true;
			}

			pthread_mutex_unlock( &m_mtx );
			return ret;
		}

	protected:
		pthread_mutex_t m_mtx;
		pthread_cond_t m_unlocked;
		pthread_t m_tid;
		int m_count;
		bool m_locked;
	};
#endif

#endif

	class scoped_lock
	{
	public:
		scoped_lock(mutex& mtx) : m_mtx(mtx)	{ m_mtx.lock(); }
		~scoped_lock()	{ m_mtx.unlock(); }

	protected:
		mutex& m_mtx;
	};

	///////////////////////////////////////////////////////////////////////////////
	// event

#ifdef _MSC_VER

	class event : public handler_impl
	{
	public:
		enum { wait_ok = 0, wait_timeout = -1, infinite = -1 };

	public:
		event(bool auto_reset = true, bool initial_state = false)
		{
			m_event = CreateEvent(NULL, !auto_reset, initial_state, NULL); 
		}
		~event()		{ CloseHandle(m_event); }
		void wait()		{ timedwait(); }
		bool timedwait(int timeout = infinite)
		{
			DWORD ret = WaitForSingleObject(m_event, timeout);
			return (ret == WAIT_OBJECT_0);
		}
		bool trywait() 	{ return timedwait(0); }
		void set()		{ SetEvent(m_event); }
		void reset()	{ ResetEvent(m_event); }
		void signal()	{ set(); }
		void sync()         { }
		operator handle() { return m_event; }

	protected:
		HANDLE m_event;
	};

#elif defined(__GNUC__)

#if 0 
	class event
	{
	public:
		enum { wait_ok = 0, wait_timeout = -1, infinite = -1 };

	public:
		event(bool auto_reset = true, bool initial_state = false) 
			: m_auto_reset(auto_reset), m_state(initial_state)
		{
			pthread_cond_init(&m_cond, NULL);
			pthread_mutex_init(&m_mtx, NULL);
			// pipe( m_fds );
		}
		~event()
		{ 
			pthread_cond_destroy(&m_cond);
			pthread_mutex_destroy(&m_mtx);
			//close( m_fds[0] );
			//close( m_fds[1] );
		}
		void wait()
		{  
			pthread_mutex_lock(&m_mtx);
			while(m_state == false)
				pthread_cond_wait(&m_cond, &m_mtx);
			if(m_auto_reset)
				m_state = false;
			pthread_mutex_unlock(&m_mtx);
		}
		bool timedwait(int timeout = infinite)
		{  
			if(timeout >= 0)
			{
				timeval tv;
				timespec ts;

				pthread_mutex_lock(&m_mtx);
				gettimeofday(&tv, NULL);
				ts.tv_sec = tv.tv_sec + timeout/1000;
				ts.tv_nsec = tv.tv_usec * 1000 + (timeout%1000) * 1000000;
				int ret = 0;
				while(m_state == false && ret != ETIMEDOUT)
					ret = pthread_cond_timedwait(&m_cond, &m_mtx, &ts);
				if(m_auto_reset) m_state = false;
				pthread_mutex_unlock(&m_mtx);
				if(ret == ETIMEDOUT) return false;			
			}
			else
			{
				wait();
			}

			return true;		
		}
		bool trywait()
		{
			//cout << "event trywait." << endl;
			bool res = false;
			pthread_mutex_lock(&m_mtx);
			//cout << "event trywait locked." << endl;
			res = m_state;
			m_state = false;
			pthread_mutex_unlock(&m_mtx);
			//cout << "event trywaited." << endl;
			return res;
		}
		void set()
		{  
			pthread_mutex_lock(&m_mtx);
			m_state = true;
			//		if(m_auto_reset)
			pthread_cond_signal(&m_cond);
			{
				//       char buf[] = "set event";
				//      write( m_fds[1], buf, sizeof(buf) );
			}
			//		else
			//			pthread_cond_broadcast(&m_cond);
			pthread_mutex_unlock(&m_mtx);
			//cout << "event seted." << endl;
		}
		void reset()
		{ 
			pthread_mutex_lock(&m_mtx);
			if( m_state )
			{
				//     char buf[] = "get event";
				//      read( m_fds[0], buf, sizeof(buf) );
			}
			m_state = false; 
			pthread_mutex_unlock(&m_mtx);
		}
		void signal()	{ set(); }
		//operator int()  { return m_fds[0]; } 

	protected:
		pthread_cond_t	m_cond;
		pthread_mutex_t	m_mtx;
		bool m_auto_reset;
		bool m_state;
		// int  m_fds[2];
	};
#else

	class event : public handler_impl
	{
	public:
		enum { wait_ok = 0, wait_timeout = -1, infinite = -1 };

	public:
		event(bool auto_reset = true, bool initial_state = false) 
			: m_auto_reset(auto_reset), m_state(initial_state)
		{
			pthread_mutex_init(&m_mtx, NULL);
			pipe( m_fds );
			if( m_state )
			{	
				m_state = false;
				set();
			}
		}
		~event()
		{ 
			pthread_mutex_destroy(&m_mtx);
			close( m_fds[0] );
			close( m_fds[1] );
		}
		void wait()
		{  
			fd_set rset;
			FD_ZERO( &rset );
			FD_SET( m_fds[0], &rset );
#ifdef SHOW_DEBUG_INFO
			cout << "event: waiting..." << endl;
#endif
			int ret = select( m_fds[0]+1, &rset, NULL, NULL, NULL);
			( ret == 1);
			if( m_auto_reset )
			{
				reset();
			}        
		}
		bool timedwait(int timeout = infinite)
		{  
			if(timeout >= 0)
			{
				timeval tv;

				tv.tv_sec = timeout/1000;
				tv.tv_usec = (timeout%1000) * 1000;

				fd_set rset;
				FD_ZERO( &rset );
				FD_SET( m_fds[0], &rset );

				int ret = select( m_fds[0]+1, &rset, NULL, NULL, &tv );
				if( ret == 1)
				{
					if( m_auto_reset ) reset();
					return true;
				}
				else
					return false;            
			}
			else
			{
				wait();
			}

			return true;		
		}
		bool trywait()
		{
			return timedwait( 0 );
		}
		void set()
		{  
			pthread_mutex_lock(&m_mtx);
			if( !m_state )
			{
				m_state = true;
				char buf[] = "event: set";
				write( m_fds[1], buf, sizeof(buf) );
			}
			pthread_mutex_unlock(&m_mtx);
#ifdef SHOW_DEBUG_INFO
			cout << "event: seted." << endl;
#endif
		}
		void reset()
		{ 
			pthread_mutex_lock(&m_mtx);
#ifdef SHOW_DEBUG_INFO
			cout << "event: reset" << endl;
#endif
			if( m_state )
			{
				char buf[] = "event: reset";
#ifdef SHOW_DEBUG_INFO
				cout << "event: reading" << endl;
#endif
				read( m_fds[0], buf, sizeof(buf) );
			}
			m_state = false; 
			pthread_mutex_unlock(&m_mtx);
		}
		void signal()	{ set(); }
		void sync()         { if( m_auto_reset ) reset(); }
		operator handle()  { return m_fds[0]; }    

	protected:
		pthread_mutex_t	m_mtx;
		int  m_fds[2];
		bool m_auto_reset;
		bool m_state;
	};

#endif



#endif

	///////////////////////////////////////////////////////////////////////////////
	// timedsem
#ifdef _MSC_VER

	class timedsem : public handler_impl
	{
	public:
		timedsem(int intial_count = 0)
		{
			m_sem = CreateSemaphore(NULL, intial_count, 65535, NULL);
		}
		~timedsem() { CloseHandle(m_sem); }
		bool wait(int timeout = INFINITE)
		{ 
			return WaitForSingleObject(m_sem, timeout) == WAIT_OBJECT_0; 
		}
		bool trywait()	{ return wait(0); }
		void release()	{ ReleaseSemaphore(m_sem, 1, NULL); }
		void signal()	{ release(); }
		void reset() { }
		void sync()         { }
		operator handle() { return m_sem; }

	protected:
		handle m_sem;
	};

#elif defined(__GNUC__)

	class timedsem : public handler_impl
	{
	public:
		timedsem(int intial_count = 0) : m_count(intial_count)
		{
			pthread_cond_init(&m_cond, NULL);
			pthread_mutex_init(&m_mtx, NULL);
			pipe( m_fds );
			if( m_count > 0)
			{
				for(int i=0; i<intial_count; i++)
				{
					char buf[] = "semaphore: signal";
					write( m_fds[1], buf, sizeof(buf) );
				}
			}
		}
		~timedsem()
		{
			pthread_cond_destroy(&m_cond);
			pthread_mutex_destroy(&m_mtx);
			close( m_fds[0] );  close( m_fds[1] );
		}
		bool wait(int timeout = -1)
		{
			//pthread_mutex_lock(&m_mtx);

			fd_set rset;
			FD_ZERO( &rset );
			FD_SET( m_fds[0], &rset );

			int ret = -1;
			if( timeout < 0 )
				ret = select( m_fds[0]+1, &rset, NULL, NULL, NULL );
			else
			{
				timeval tv;

				tv.tv_sec = timeout/1000;
				tv.tv_usec = (timeout%1000) * 1000;           

				ret = select( m_fds[0]+1, &rset, NULL, NULL, &tv );
			}

			if( ret == 1 && FD_ISSET(m_fds[0], &rset) )
			{
				if( --m_count >= 0 ); 
				{
					char buf[] = "semaphore: signal";
					read( m_fds[0], buf, sizeof(buf) );
				}
			}
			else
			{
				//pthread_mutex_unlock(&m_mtx);            
				return false;
			}

			//pthread_mutex_unlock(&m_mtx);
			return true;
		}
		bool trywait()
		{
			bool res = false;
			pthread_mutex_lock(&m_mtx);
			res = m_count > 0;
			if( m_count > 0 )
			{
				char buf[] = "semaphore: signal";
				read( m_fds[0], buf, sizeof(buf) );
				--m_count;
			}
			pthread_mutex_unlock(&m_mtx);
			return res;
		}
		void release()
		{
			//pthread_mutex_lock(&m_mtx);
			if( ++m_count > 0 )
			{
				char buf[] = "semaphore: signal";
				write( m_fds[1], buf, sizeof(buf) );
			}
			//pthread_mutex_unlock(&m_mtx);
		}
		void signal() { release(); }
		operator handle() { return m_fds[0]; }
		void sync() { trywait(); }

	protected:
		pthread_cond_t		m_cond;
		pthread_mutex_t		m_mtx;
		int					m_count;
		int     m_fds[2];
	};

#endif

	///////////////////////////////////////////////////////////////////////////////
	// semaphore

#ifdef _MSC_VER

	typedef timedsem semaphore;

#elif defined(__GNUC__)

	class semaphore
	{
	public:
		semaphore(int intial_count = 0)
		{
			if(sem_init(&m_sem, 0, intial_count) == 0) 
			{
#ifdef SHOW_DEBUG_INFO
				cout << "semaphote:init ok." << endl;
#endif
			}
		}
		~semaphore()	{ sem_destroy(&m_sem); }
		bool wait()		{ return sem_wait(&m_sem) == 0; }
		bool trywait()	{ int ret = sem_trywait(&m_sem); 
#ifdef SHOW_DEBUG_INFO
		cout << "semaphore: trywait res: "<< ret << endl;
#endif
		return ret == 0; }
		void release()	{ int ret = sem_post(&m_sem); 
#ifdef SHOW_DEBUG_INFO
		cout << "semaphore: release res: " << ret << endl;
#endif
		}
		void signal()	{ release(); }

	protected:
		sem_t m_sem;
	};

#endif


	///////////////////////////////////////////////////////////////////////////////
	// waitable_timer

#if defined(_MSC_VER)

#if (_WIN32_WINNT >= 0x0400) || (_WIN32_WINDOWS > 0x0400)
	class waitable_timer : public handler_impl
	{
	public:
		waitable_timer() : m_interval( 0 )
		{
			m_handle = CreateWaitableTimer(NULL, FALSE, NULL);
		}

		~waitable_timer()
		{
			if( m_handle != NULL) 
			{
				CancelWaitableTimer( m_handle );
				CloseHandle( m_handle );
			}
		}

		bool start(long interval, bool period = true)
		{
			if( period && interval <= 0 ) assert(0);

			LARGE_INTEGER li;
			if( period )
			{
				li.QuadPart = 0;
				m_interval = interval;
			}
			else
			{
				li.QuadPart = -interval*1000*10;
				m_interval = 0;
			}

			return SetWaitableTimer( m_handle, &li, m_interval, NULL, NULL, FALSE ) != FALSE;
		}

		void stop()
		{
			CancelWaitableTimer( m_handle );
		}

		bool wait(int timeout = -1)
		{
			return WaitForSingleObject( m_handle,  timeout) == WAIT_OBJECT_0;
		}

		bool trywait()
		{
			return wait(0);
		}

		void sync()
		{
		}

		operator handle()
		{
			return m_handle;
		}    

	protected:
		handle  m_handle;
		long    m_interval;
	};
#endif

#elif defined(__GNUC__)

	class waitable_timer : public handler_impl
	{
	public:
		waitable_timer() : m_interval(0), m_period(true), m_running(false)
		{
			pthread_mutex_init(&m_mtx, NULL);
			pipe( m_fds );
			pipe( m_exit_fds );
		}

		~waitable_timer()
		{
			stop();
			pthread_mutex_destroy(&m_mtx);
			close( m_fds[0] );  close( m_fds[1] );
			close( m_exit_fds[0] );  close( m_exit_fds[1] );
		}

		bool start(long interval, bool period = true)
		{
			if( period && interval <= 0 ) assert(0);
			if( period && interval <= 0 ) return false;

			stop();

			m_interval = interval;
			m_period = period;

			int ret = pthread_create(&m_tid, NULL, thread_func, this);
			return (ret == 0);
		}

		void stop()
		{
			if( !m_running ) return;
			char buf[] = "exit";
			write( m_exit_fds[1], buf, sizeof(buf) );
			pthread_join( m_tid, NULL );
#ifdef SHOW_DEBUG_INFO
			cout << "waitable_timer: stopped" << endl;
#endif
		}

		bool wait(int timeout = -1)
		{
			fd_set rset;
			FD_ZERO( &rset );
			FD_SET( m_fds[0], &rset );

			int ret = 0;
			if( timeout == -1)
				ret = select( m_fds[0]+1, &rset, NULL, NULL, NULL );
			else
			{
				timeval tv;        
				tv.tv_sec = timeout/1000;
				tv.tv_usec = (timeout%1000) * 1000;            

				ret = select( m_fds[0]+1, &rset, NULL, NULL, &tv );
			}

			if( ret == 1 && FD_ISSET( m_fds[0], &rset ) )
			{
				reset();
				return true;
			}
			else
				return false;            
		}

		bool trywait()
		{
			return wait(0);
		}

		void reset()
		{
			char buf[] = "get timedout";
#ifdef SHOW_DEBUG_INFO
			cout << "waitable_timer: reset" << endl;
#endif
			read( m_fds[0], buf, sizeof(buf) );        
		}

		void sync() { reset(); }

		operator handle()
		{
			return (handle)m_fds[0];
		}

	protected:
		static void* thread_func(void* param)
		{
			waitable_timer* pthis = (waitable_timer*)param;
			if( !pthis ) return NULL;

			pthis->m_running = true;

			//printf("waitable_timer: thread started.\n");
			if( !pthis->m_period && pthis->m_interval > 0 )
			{
				pthread_mutex_t mtx;
				pthread_cond_t cond;
				pthread_mutex_init(&mtx, NULL);
				pthread_cond_init(&cond, NULL);
				pthread_mutex_lock(&mtx);
				timespec ts;
				ts.tv_sec =  pthis->m_interval/1000;
				ts.tv_nsec = (pthis->m_interval%1000) * 1000000;
				int ret;
				while(1) {
					ret = pthread_cond_timedwait(&cond, &mtx, &ts);
					if( ret == ETIMEDOUT ) 
					{
						char buf[] = "timedout";
						write( pthis->m_fds[1], buf, sizeof(buf) );
						break;
					}
				}
				pthread_mutex_unlock(&mtx);
				pthread_mutex_destroy(&mtx);
				pthread_cond_destroy(&cond);
			}
			else 
			{
				if( pthis->m_interval < 0 ) pthis->m_interval *= -1;
				do {            
					timeval tv;        
					tv.tv_sec = pthis->m_interval/1000;
					tv.tv_usec = (pthis->m_interval%1000) * 1000;

					fd_set rset;
					FD_ZERO( &rset );
					FD_SET( pthis->m_exit_fds[0], &rset );

					int ret = select( pthis->m_exit_fds[0]+1, &rset, NULL, NULL, &tv ); 
#ifdef SHOW_DEBUG_INFO
					cout << "waitable_timer: select ret: " << ret << endl;
#endif

					if(ret == 0)
					{
						char buf[] = "timedout";
						write( pthis->m_fds[1], buf, sizeof(buf) );
					}
					else if( FD_ISSET(pthis->m_exit_fds[0], &rset) ) 
					{
						char buf[] = "got exit";
						read( pthis->m_exit_fds[0], buf, sizeof(buf) );
						break;
					}
					else
						break;
				} while( pthis->m_period && pthis->m_running );
			}

			pthis->m_running = false;
			//printf("waitable_timer: thread stoped.\n");
			return NULL;
		}

	protected:
		pthread_mutex_t		m_mtx;
		pthread_t           m_tid;
		long    m_interval;
		bool    m_period;
		bool    m_running;
		int     m_fds[2];
		int     m_exit_fds[2];
	};

#endif

    class wait_for_multiple_events
    {
    public:
        // wait_for_multiple_events,会自动复位事件信号
        int operator()(handler events[], int count, bool wait_all = false) {
#ifdef _MSC_VER
            try {
                handle test_events[max_handler_array_size];
                for (int i = 0; i < count; ++i)
                    test_events[i] = handle(events[i]);
            }
            catch (...) {
                return -1;
            }
            assert(count < (int)max_handler_array_size);
            handle _events[max_handler_array_size];
            for (int i = 0; i < count; ++i)
                _events[i] = handle(events[i]);
            int ret = (int)WaitForMultipleObjects(count, _events, wait_all ? TRUE : FALSE, INFINITE);
            return ret;
#elif defined(__GNUC__)
            int index = -1;
            int i;

            fd_set rset;
            FD_ZERO(&rset);
            int max_fd = 0;
            for (int i = 0; i < count; i++)
            {
                if (events[i] > max_fd) max_fd = events[i];
                FD_SET(events[i], &rset);
            }

            {
                int ret = select(max_fd + 1, &rset, NULL, NULL, NULL);
                if (ret == -1)
                    return -1;
                else if (ret == 0)
                    return -1; //WAIT_TIMEOUT
                else
                {
                    for (int i = 0; i < count; i++)
                    {
                        if (FD_ISSET(events[i], &rset))
                        {
                            events[i].sync();
                            return i;
                        }
                    }
                }

            }
            return -1;
#endif
        }
    };

    class wait_for_multiple_events_timed
    {
    public:
        // wait_for_multiple_events,会自动复位事件信号
        int operator()(handler events[], int count, int timeout = -1, bool wait_all = false) {
#ifdef _MSC_VER
            assert(count < (int)max_handler_array_size);
            handle _events[max_handler_array_size];
            for (int i = 0; i < count; ++i)
                _events[i] = handle(events[i]);
            DWORD ret = WaitForMultipleObjects((DWORD)count, _events, wait_all ? TRUE : FALSE, (DWORD)timeout);

            if (ret == WAIT_TIMEOUT)
                return count;
            return (int)ret;
#elif defined(__GNUC__)
#	error implement wait_for_multiple_events_timed(...) function!
#endif
        }
    };

} // namespace BTool


///////////////////////////////////////////////////////////////////////////////
// antisem or countersem

#endif//__SYNCH_H__

