//
// Created by Aaron on 8/20/2016.
//

#ifndef THENABLE_FWD_HPP_INCLUDED
#define THENABLE_FWD_HPP_INCLUDED

#include <future>

#ifndef DOXYGEN_DEFINE
#define THENABLE_DECLTYPE_AUTO decltype(auto)
#define THENABLE_DECLTYPE_AUTO_HINTED( hint ) THENABLE_DECLTYPE_AUTO
#else
#define THENABLE_DECLTYPE_AUTO auto
#define THENABLE_DECLTYPE_AUTO_HINTED( hint ) hint<THENABLE_DECLTYPE_AUTO>
#endif


namespace thenable {
    /*
     * The default launch policy of std::async is a combination of the std::launch flags,
     * allowing it to choose whatever policy it wants depending on the system.
     * */
#ifdef THENABLE_DEFAULT_POLICY
    constexpr std::launch default_policy = THENABLE_DEFAULT_POLICY;
#else
    constexpr std::launch default_policy = std::launch::deferred | std::launch::async;
#endif

    /*
     * This is a special launch type where it is guarenteed to spawn a new thread to run the task
     * asynchronously. It will not wait on any calls to .get() on the resulting future.
     *
     * 4 was chosen because deferred and async are usually 1 and 2, so 4 is the next power of two,
     * though it doesn't matter since this is a type-safe enum class anyway.
     * */

    enum class then_launch {
            detached = 4
    };

    //////////

    template <typename>
    class ThenableFuture;

    template <typename>
    class ThenableSharedFuture;

    template <typename>
    class ThenablePromise;

    //////////

    template <typename Functor, typename... Args>
    inline THENABLE_DECLTYPE_AUTO_HINTED( std::future ) defer( Functor &&f, Args &&... args );

    template <typename Functor, typename... Args>
    inline THENABLE_DECLTYPE_AUTO_HINTED( ThenableFuture ) defer2( Functor &&f, Args &&... args );

    //////////

    template <typename T, typename Functor>
    THENABLE_DECLTYPE_AUTO_HINTED( std::future ) then( std::future<T> &, Functor &&, std::launch = default_policy );

    template <typename T, typename Functor>
    THENABLE_DECLTYPE_AUTO_HINTED( std::future ) then( std::shared_future<T> &&, Functor &&, std::launch = default_policy );

    template <typename T, typename Functor>
    THENABLE_DECLTYPE_AUTO_HINTED( std::future ) then( std::shared_future<T>, Functor &&, std::launch = default_policy );

    template <typename T, typename Functor>
    THENABLE_DECLTYPE_AUTO_HINTED( std::future ) then( std::future<T> &&, Functor &&, std::launch = default_policy );

    template <typename T, typename Functor>
    THENABLE_DECLTYPE_AUTO_HINTED( std::future ) then( std::promise<T> &, Functor &&, std::launch = default_policy );

    //////////

    template <typename T, typename Functor>
    THENABLE_DECLTYPE_AUTO_HINTED( std::future ) then( std::future<T> &, Functor &&, then_launch );

    template <typename T, typename Functor>
    THENABLE_DECLTYPE_AUTO_HINTED( std::future ) then( std::shared_future<T> &&, Functor &&, then_launch );

    template <typename T, typename Functor>
    THENABLE_DECLTYPE_AUTO_HINTED( std::future ) then( std::shared_future<T>, Functor &&, then_launch );

    template <typename T, typename Functor>
    THENABLE_DECLTYPE_AUTO_HINTED( std::future ) then( std::future<T> &&, Functor &&, then_launch );

    template <typename T, typename Functor>
    THENABLE_DECLTYPE_AUTO_HINTED( std::future ) then( std::promise<T> &, Functor &&, then_launch );

    //////////

    template <typename FutureType, typename Functor, typename LaunchPolicy = std::launch>
    THENABLE_DECLTYPE_AUTO_HINTED( ThenableFuture ) then2( FutureType &&, Functor &&f, LaunchPolicy policy = default_policy );

    template <typename FutureType, typename Functor, typename LaunchPolicy = std::launch>
    THENABLE_DECLTYPE_AUTO_HINTED( ThenableFuture ) then2( FutureType &, Functor &&f, LaunchPolicy policy = default_policy );

    //////////

    template <typename T>
    ThenableFuture<T> to_thenable( std::future<T> && );

    template <typename T>
    ThenableSharedFuture<T> to_thenable( const std::shared_future<T> & );

    template <typename T>
    ThenableSharedFuture<T> to_thenable( std::shared_future<T> && );

    template <typename T>
    ThenablePromise<T> to_thenable( std::promise<T> && );

    //////////

    template <typename T = void, typename Functor, typename LaunchPolicy = std::launch>
    THENABLE_DECLTYPE_AUTO_HINTED( std::future ) make_promise( Functor &&, LaunchPolicy = default_policy );

    template <typename T = void, typename Functor, typename LaunchPolicy = std::launch>
    THENABLE_DECLTYPE_AUTO_HINTED( ThenableFuture ) make_promise2( Functor &&, LaunchPolicy = default_policy );
}

#endif //THENABLE_FWD_HPP_INCLUDED
