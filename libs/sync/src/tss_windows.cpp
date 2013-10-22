/*
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 *
 * (C) Copyright 2004 Michael Glassford
 * (C) Copyright 2013 Andrey Semashev
 */
/*!
 * \file   tss_windows.cpp
 *
 * \brief  This header is the Boost.Sync library implementation, see the library documentation
 *         at http://www.boost.org/doc/libs/release/libs/sync/doc/html/index.html.
 */

#include <boost/sync/detail/config.hpp>

#if defined(BOOST_SYNC_DETAIL_PLATFORM_WINAPI)

#include <cstddef>
#include <cstdlib>
#include <boost/sync/detail/interlocked.hpp>
#include <boost/sync/detail/tss.hpp>
#include "tss_manager.hpp"
#include "tss_windows_hooks.hpp"
#include <windows.h>
#include <boost/sync/detail/header.hpp>

namespace boost {

namespace sync {

namespace detail {

namespace {

#if BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_WIN6
static INIT_ONCE init_tss_once_flag = INIT_ONCE_STATIC_INIT;
#else
static long init_tss_once_flag = 0;
#endif
static tss_manager* tss_mgr = NULL;
static DWORD tss_key = TLS_OUT_OF_INDEXES;

extern "C" {

static BOOL WINAPI init_tss(void*, void*, void**)
{
    windows::tss_cleanup_implemented(); // make sure the TSS cleanup hooks are linked into the executable

    try
    {
        tss_mgr = new tss_manager();
    }
    catch (...)
    {
        std::abort();
    }

    tss_key = TlsAlloc();
    if (tss_key == TLS_OUT_OF_INDEXES)
        std::abort();

    return TRUE;
}

} // extern "C"

#if BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_WIN6

BOOST_FORCEINLINE void init_tss_once()
{
    InitOnceExecuteOnce(&init_tss_once_flag, (PINIT_ONCE_FN)&init_tss, NULL, NULL);
}

#else // BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_WIN6

BOOST_FORCEINLINE void init_tss_once()
{
    if (const_cast< long volatile& >(init_tss_once_flag) != 2)
    {
        while (true)
        {
            long old_val = BOOST_ATOMIC_INTERLOCKED_COMPARE_EXCHANGE(&init_tss_once_flag, 1, 0);
            if (old_val == 2)
                break;
            else if (old_val == 1)
                SwitchToThread();
            else
            {
                init_tss(NUll, NULL, NULL);
                BOOST_ATOMIC_INTERLOCKED_EXCHANGE(&init_tss_once_flag, 2);
                break;
            }
        }
    }
}

#endif // BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_WIN6

} // namespace

namespace windows {

void on_process_enter()
{
}

void on_process_exit()
{
    tss_manager* p = tss_mgr;
    tss_mgr = NULL;
    delete p;

    TlsFree(tss_key);
    tss_key = TLS_OUT_OF_INDEXES;
}

void on_thread_enter()
{
}

void on_thread_exit()
{
    // This callback may be called before tss manager is initialized
    init_tss_once();
    if (tss_manager::thread_context* ctx = static_cast< tss_manager::thread_context* >(TlsGetValue(tss_key)))
    {
        tss_mgr->destroy_thread_context(ctx);
    }
}

} // namespace windows

BOOST_SYNC_API void add_thread_exit_callback(at_thread_exit_callback callback, void* context)
{
    init_tss_once();
    tss_manager::thread_context* ctx = static_cast< tss_manager::thread_context* >(TlsGetValue(tss_key));

    if (!ctx)
    {
        ctx = tss_mgr->create_thread_context();
        TlsSetValue(tss_key, ctx);
    }

    ctx->add_at_exit_entry(callback, context);
}

BOOST_SYNC_API thread_specific_key new_thread_specific_key(at_thread_exit_callback callback, bool cleanup_at_delete)
{
    init_tss_once();
    return tss_mgr->new_key(callback, cleanup_at_delete);
}

BOOST_SYNC_API void delete_thread_specific_key(thread_specific_key key) BOOST_NOEXCEPT
{
    try
    {
        tss_mgr->delete_key(key);
    }
    catch (...)
    {
        std::abort();
    }
}

BOOST_SYNC_API void* get_thread_specific(thread_specific_key key)
{
    tss_manager::thread_context* ctx = static_cast< tss_manager::thread_context* >(TlsGetValue(tss_key));
    if (ctx)
        return ctx->get_value(key);
    return NULL;
}

BOOST_SYNC_API void set_thread_specific(thread_specific_key key, void* p)
{
    tss_manager::thread_context* ctx = static_cast< tss_manager::thread_context* >(TlsGetValue(tss_key));

    if (!ctx)
    {
        ctx = tss_mgr->create_thread_context();
        TlsSetValue(tss_key, ctx);
    }

    ctx->set_value(key, p);
}

} // namespace detail

} // namespace sync

} // namespace boost

#include <boost/sync/detail/footer.hpp>

#endif // defined(BOOST_SYNC_DETAIL_PLATFORM_WINAPI)
