//
// Created by Aaron on 8/20/2016.
//

#ifndef THENABLE_FWD_HPP_INCLUDED
#define THENABLE_FWD_HPP_INCLUDED

#include <function_traits.hpp>

#include <future>
#include <tuple>

#ifndef DOXYGEN_DEFINE
#define THENABLE_DECLTYPE_AUTO decltype(auto)
#define THENABLE_DECLTYPE_AUTO_HINTED( hint ) THENABLE_DECLTYPE_AUTO
#define THENABLE_DECLTYPE_AUTO_HINTED_EXPLICIT( hint ) THENABLE_DECLTYPE_AUTO
#else
#define THENABLE_DECLTYPE_AUTO auto
#define THENABLE_DECLTYPE_AUTO_HINTED( hint ) hint<THENABLE_DECLTYPE_AUTO>
#define THENABLE_DECLTYPE_AUTO_HINTED_EXPLICIT( hint ) hint
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

    template <typename... Results>
    constexpr std::tuple<ThenableFuture<Results>...> to_thenable( std::tuple<std::future<Results>...> && );

    template <typename... Results>
    constexpr std::tuple<ThenableSharedFuture<Results>...> to_thenable( std::tuple<std::shared_future<Results>...> && );

    template <typename... Results>
    constexpr std::tuple<ThenablePromise<Results>...> to_thenable( std::tuple<std::promise<Results>...> && );

    //////////

    template <typename T = void, typename Functor, typename LaunchPolicy = std::launch>
    THENABLE_DECLTYPE_AUTO_HINTED( std::future ) make_promise( Functor &&, LaunchPolicy = default_policy );

    template <typename T = void, typename Functor, typename LaunchPolicy = std::launch>
    THENABLE_DECLTYPE_AUTO_HINTED( ThenableFuture ) make_promise2( Functor &&, LaunchPolicy = default_policy );

    //////////

    template <typename... Functors>
    inline std::tuple<std::future<fn_traits::fn_result_of<Functors>>...> parallel( Functors &&... fns );

    template <typename... Functors>
    inline std::tuple<ThenableFuture<fn_traits::fn_result_of<Functors>>...> parallel2( Functors &&... fns );

    template <typename... Functors>
    std::tuple<std::future<fn_traits::fn_result_of<Functors>>...> parallel_n( size_t concurrency, Functors &&... fns );

    template <typename... Functors>
    inline std::tuple<ThenableFuture<fn_traits::fn_result_of<Functors>>...> parallel2_n( size_t concurrency, Functors &&... fns );

    //////////

    template <typename... Results>
    std::future<std::tuple<Results...>> await_all( std::tuple<std::future<Results>...> &&results, std::launch policy = default_policy );

    template <typename... Results>
    std::future<std::tuple<Results...>> await_all( std::tuple<std::shared_future<Results>...> &&results, std::launch policy = default_policy );

    template <typename... Results>
    ThenableFuture<std::tuple<Results...>> await_all( std::tuple<ThenableFuture<Results>...> &&results, std::launch policy = default_policy );

    template <typename... Results>
    ThenableFuture<std::tuple<Results...>> await_all( std::tuple<ThenableSharedFuture<Results>...> &&results, std::launch policy = default_policy );

    template <typename... Results>
    std::future<std::tuple<Results...>> await_all( std::tuple<std::promise<Results>...> &&results, std::launch policy = default_policy );

    template <typename... Results>
    ThenableFuture<std::tuple<Results...>> await_all( std::tuple<ThenablePromise<Results>...> &&results, std::launch policy = default_policy );

    //////////

    template <typename... Results>
    std::future<std::tuple<Results...>> await_all( std::tuple<std::future<Results>...> &&results, then_launch policy );

    template <typename... Results>
    std::future<std::tuple<Results...>> await_all( std::tuple<std::shared_future<Results>...> &&results, then_launch policy );

    template <typename... Results>
    ThenableFuture<std::tuple<Results...>> await_all( std::tuple<ThenableFuture<Results>...> &&results, then_launch policy );

    template <typename... Results>
    ThenableFuture<std::tuple<Results...>> await_all( std::tuple<ThenableSharedFuture<Results>...> &&results, then_launch policy );

    template <typename... Results>
    std::future<std::tuple<Results...>> await_all( std::tuple<std::promise<Results>...> &&results, then_launch policy );

    template <typename... Results>
    ThenableFuture<std::tuple<Results...>> await_all( std::tuple<ThenablePromise<Results>...> &&results, then_launch policy );
}

#endif //THENABLE_FWD_HPP_INCLUDED
