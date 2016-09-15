//
// Created by Aaron on 8/17/2016.
//

#ifndef THENABLE_IMPLEMENTATION_HPP
#define THENABLE_IMPLEMENTATION_HPP

#include <function_traits.hpp>

#include <assert.h>
#include <future>
#include <tuple>
#include <memory>
#include <thread>

//This is defined so it can be quickly toggled if something needs debugging
#define THENABLE_NOEXCEPT noexcept

/*
 * This is here for the few situations where the result is simply too complex to express as anything other than decltype(auto),
 * but I know it will return a certain form of a value. Like std::future<auto>, where it is a future type, but the value of auto is entirely contextual based on what
 * you give the function. Especially with the waterfall function, where the value of the future is based upon recursive asynchronous functions. There is a point where
 * I just give up trying to express that.
 *
 * Even implicit_result_of is decltype based, because it doesn't know either. It was just a bit simpler to do that for one function deep.
 * */
#ifdef DOXYGEN_DEFINED
#define THENABLE_DECLTYPE_AUTO_HINTED(hint) hint<auto>
#else
#define THENABLE_DECLTYPE_AUTO_HINTED( hint ) decltype(auto)
#endif

/*
 * The `then` function implemented below is designed to be very similar to JavaScript's Promise.then in functionality.
 *
 * You provide it with a promise or future/shared_future object, and it will invoke a callback with the result of that promise or future.
 *
 * Additionally, it will resolve recursive futures. As in, if the original future resolves to another future, and so forth.
 *
 * It will also resolve futures/promises returned by the callback, so the future returned by `then` will always resolve to a non-future type.
 *
 * Basically, you can layer up whatever you want and it'll resolve them all.
 * */

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

    namespace detail {
        template <typename T>
        struct get_future_type {
        };

        template <typename T>
        struct get_future_type<std::future<T>> {
            typedef T type;
        };

        template <typename T>
        struct get_future_type<std::shared_future<T>> {
            typedef T type;
        };

        template <typename T>
        struct get_future_type<std::promise<T>> {
            typedef T type;
        };

        template <typename T>
        struct get_future_type<ThenableFuture<T>> {
            typedef T type;
        };

        template <typename T>
        struct get_future_type<ThenableSharedFuture<T>> {
            typedef T type;
        };

        template <typename T>
        struct get_future_type<ThenablePromise<T>> {
            typedef T type;
        };

        template <typename T>
        struct recursive_get_future_type {
            typedef T type;
        };

        template <typename T>
        struct recursive_get_future_type<std::future<T>> {
            typedef typename recursive_get_future_type<T>::type type;
        };

        template <typename T>
        struct recursive_get_future_type<std::shared_future<T>> {
            typedef typename recursive_get_future_type<T>::type type;
        };

        template <typename T>
        struct recursive_get_future_type<std::promise<T>> {
            typedef typename recursive_get_future_type<T>::type type;
        };

        template <typename T>
        struct recursive_get_future_type<ThenableFuture<T>> {
            typedef typename recursive_get_future_type<T>::type type;
        };

        template <typename T>
        struct recursive_get_future_type<ThenableSharedFuture<T>> {
            typedef typename recursive_get_future_type<T>::type type;
        };

        template <typename T>
        struct recursive_get_future_type<ThenablePromise<T>> {
            typedef typename recursive_get_future_type<T>::type type;
        };
    }

    template <typename Functor>
    using recursive_result_of = typename detail::recursive_get_future_type<typename std::result_of<Functor()>::type>::type;

    //////////

    template <typename T>
    constexpr ThenableFuture<T> to_thenable( std::future<T> && );

    template <typename T>
    constexpr ThenableSharedFuture<T> to_thenable( const std::shared_future<T> & );

    template <typename T>
    constexpr ThenableSharedFuture<T> to_thenable( std::shared_future<T> && );

    template <typename T>
    constexpr ThenablePromise<T> to_thenable( std::promise<T> && );

    //////////

    template <typename... Results>
    constexpr std::tuple<ThenableFuture<Results>...> to_thenable( std::tuple<std::future<Results>...> && );

    template <typename... Results>
    constexpr std::tuple<ThenableSharedFuture<Results>...> to_thenable( std::tuple<std::shared_future<Results>...> && );

    template <typename... Results>
    constexpr std::tuple<ThenablePromise<Results>...> to_thenable( std::tuple<std::promise<Results>...> && );

    //////////

    template <typename... Results>
    constexpr std::tuple<ThenableFuture<Results>...> to_thenable( std::tuple<ThenableFuture<Results>...> && );

    template <typename... Results>
    constexpr std::tuple<ThenableSharedFuture<Results>...> to_thenable( std::tuple<ThenableSharedFuture<Results>...> && );

    template <typename... Results>
    constexpr std::tuple<ThenablePromise<Results>...> to_thenable( std::tuple<ThenablePromise<Results>...> && );

    //////////

    template <typename T = void, typename Functor, typename LaunchPolicy = std::launch>
    std::future<T> make_promise( Functor &&, LaunchPolicy = default_policy );

    template <typename T = void, typename Functor, typename LaunchPolicy = std::launch>
    ThenableFuture<T> make_promise2( Functor &&, LaunchPolicy = default_policy );


    //////////

    template <typename... Functors>
    std::tuple<std::future<recursive_result_of<Functors>>...> parallel( Functors &&... fns );

    template <typename... Functors>
    std::tuple<ThenableFuture<recursive_result_of<Functors>>...> parallel2( Functors &&... fns );

    template <typename... Functors>
    std::tuple<std::future<recursive_result_of<Functors>>...> parallel_n( size_t concurrency, Functors &&... fns );

    template <typename... Functors>
    std::tuple<ThenableFuture<recursive_result_of<Functors>>...> parallel2_n( size_t concurrency, Functors &&... fns );

    //////////

    template <typename Functor, typename... Args>
    inline std::future<typename std::result_of<Functor( Args... )>::type> defer( Functor &&f, Args &&... args ) {
        return std::async( std::launch::deferred, std::forward<Functor>( f ), std::forward<Args>( args )... );
    };

    template <typename Functor, typename... Args>
    inline ThenableFuture<typename std::result_of<Functor( Args... )>::type> defer2( Functor &&f, Args &&... args ) {
        return to_thenable( defer( std::forward<Functor>( f ), std::forward<Args>( args )... ));
    };

    //////////

    namespace detail {
        using namespace fn_traits;

        template <typename Functor, typename T, std::size_t... S>
        inline decltype( auto ) invoke_helper( Functor &&func, T &&t, std::index_sequence<S...> ) {
            return func( std::get<S>( std::forward<T>( t ))... );
        }

        template <typename Functor, typename T>
        inline decltype( auto ) invoke_tuple( Functor &&func, T &&t ) {
            constexpr auto Size = std::tuple_size<typename std::decay<T>::type>::value;

            return invoke_helper( std::forward<Functor>( func ),
                                  std::forward<T>( t ),
                                  std::make_index_sequence<Size>{} );
        }

        /*
         * recursive_get:
         *
         * This will continually call "get" on any future or promise objects returned by another future,
         * recursively, basically resolving the entire chain at once
         * */

        /*
         * Forward declarations
         * */

        template <typename T>
        typename recursive_get_future_type<T>::type recursive_get( std::future<T> && );

        template <typename T>
        typename recursive_get_future_type<T>::type recursive_get( std::shared_future<T> && );

        template <typename T>
        typename recursive_get_future_type<T>::type recursive_get( std::promise<T> && );

        template <typename T>
        typename recursive_get_future_type<T>::type recursive_get( ThenableFuture<T> && );

        template <typename T>
        typename recursive_get_future_type<T>::type recursive_get( ThenableSharedFuture<T> && );

        template <typename T>
        typename recursive_get_future_type<T>::type recursive_get( ThenablePromise<T> && );

        template <typename T>
        constexpr T recursive_get( T && ) THENABLE_NOEXCEPT;

        /*
         * std future implementations. ThenableX implementations are below their class implementation at the bottom of the file.
         * */

        template <typename T>
        inline typename recursive_get_future_type<T>::type recursive_get( std::future<T> &&t ) {
            return recursive_get( t.get());
        };

        template <typename T>
        inline typename recursive_get_future_type<T>::type recursive_get( std::shared_future<T> &&t ) {
            return recursive_get( t.get());
        };

        template <>
        inline typename recursive_get_future_type<void>::type recursive_get<void>( std::future<void> &&t ) {
            t.get();
        };

        template <>
        inline typename recursive_get_future_type<void>::type recursive_get<void>( std::shared_future<void> &&t ) {
            t.get();
        };

        template <typename T>
        inline typename recursive_get_future_type<T>::type recursive_get( std::promise<T> &&t ) {
            return recursive_get( t.get_future());
        };

        /*
         * This just forwards anything that is NOT a promise or future and doesn't do anything to it.
         * */
        template <typename T>
        constexpr T recursive_get( T &&t ) THENABLE_NOEXCEPT {
            return std::forward<T>( t );
        };

        //////////

        /*
         * then_invoke_helper structure
         *
         * This just takes care of the invocation of the callback, forwarding arguments and so forth and
         * taking care of any returned futures
         * */

        template <typename Functor, typename R = fn_result_of<Functor>>
        struct then_invoke_helper {
            /*
             * If you're seeing any of the code below, it's probably because a callback given to `then` doesn't
             * have the same argument types as the value being passed from the previous future.
             * */

            template <typename... Args>
            inline static decltype( auto ) invoke( Functor &&f, std::tuple<Args...> &&args ) {
                return recursive_get( invoke_tuple( std::forward<Functor>( f ), std::forward<std::tuple<Args...>>( args )));
            }

            template <typename T>
            inline static decltype( auto ) invoke( Functor &&f, T &&arg ) {
                return recursive_get( f( std::forward<T>( arg )));
            }

            inline static decltype( auto ) invoke( Functor &&f ) {
                return recursive_get( f());
            }
        };

        /*
         * Specialization of then_invoke_helper for void callbacks.
         * */
        template <typename Functor>
        struct then_invoke_helper<Functor, void> {
            /*
             * If you're seeing any of the code below, it's probably because a callback given to `then` doesn't
             * have the same argument types as the value being passed from the previous future.
             * */

            template <typename... Args>
            inline static void invoke( Functor &&f, std::tuple<Args...> &&args ) {
                invoke_tuple( std::forward<Functor>( f ), std::forward<std::tuple<Args...>>( args ));
            }

            template <typename T>
            inline static void invoke( Functor &&f, T &&arg ) {
                f( std::forward<T>( arg ));
            }

            inline static void invoke( Functor &&f ) {
                f();
            }
        };

        //////////

        /*
         * then_helper structure
         *
         * This takes care of resolving the first given future, and optionally passing the
         * resulting value to the callback function.
         * */

        template <typename T, typename Functor>
        struct then_helper {
            inline static decltype( auto ) dispatch( std::future<T> &&s, Functor &&f ) {
                return then_invoke_helper<Functor>::invoke( std::forward<Functor>( f ), recursive_get( std::forward<std::future<T>>( s )));
            }

            inline static decltype( auto ) dispatch( std::shared_future<T> &&s, Functor &&f ) {
                return then_invoke_helper<Functor>::invoke( std::forward<Functor>( f ), recursive_get( std::forward<std::shared_future<T>>( s )));
            }
        };

        template <typename Functor>
        struct then_helper<void, Functor> {
            inline static decltype( auto ) dispatch( std::future<void> &&s, Functor &&f ) {
                s.get();

                return then_invoke_helper<Functor>::invoke( std::forward<Functor>( f ));
            }

            inline static decltype( auto ) dispatch( std::shared_future<void> &&s, Functor &&f ) {
                s.get();

                return then_invoke_helper<Functor>::invoke( std::forward<Functor>( f ));
            }
        };

        //////////

        /*
         * detached_then_helper structure
         *
         * These provide a function for catching exceptions and forwarding the result of the callbacks to the promise.
         *
         * It also accounts for void callbacks which don't return anything.
         * */

        template <typename T>
        struct detached_then_helper {
            template <typename Functor, typename K>
            static inline void dispatch( std::promise<T> &p, std::future<K> &&s, Functor &&f ) THENABLE_NOEXCEPT {
                try {
                    p.set_value( then_helper<K, Functor>::dispatch( std::forward<std::future<K>>( s ), std::forward<Functor>( f )));

                } catch( ... ) {
                    p.set_exception( std::current_exception());
                }
            }

            template <typename Functor, typename K>
            static inline void dispatch( std::promise<T> &p, std::shared_future<K> &&s, Functor &&f ) THENABLE_NOEXCEPT {
                try {
                    p.set_value( then_helper<K, Functor>::dispatch( std::forward<std::shared_future<K>>( s ), std::forward<Functor>( f )));

                } catch( ... ) {
                    p.set_exception( std::current_exception());
                }
            }
        };

        /*
         * Specialization for void Functors
         * */
        template <>
        struct detached_then_helper<void> {
            template <typename Functor, typename K>
            static inline void dispatch( std::promise<void> &p, std::future<K> &&s, Functor &&f ) THENABLE_NOEXCEPT {
                try {
                    then_helper<K, Functor>::dispatch( std::forward<std::future<K>>( s ), std::forward<Functor>( f ));

                    p.set_value();

                } catch( ... ) {
                    p.set_exception( std::current_exception());
                }
            }

            template <typename Functor, typename K>
            static inline void dispatch( std::promise<void> &p, std::shared_future<K> &&s, Functor &&f ) THENABLE_NOEXCEPT {
                try {
                    then_helper<K, Functor>::dispatch( std::forward<std::shared_future<K>>( s ), std::forward<Functor>( f ));

                    p.set_value();

                } catch( ... ) {
                    p.set_exception( std::current_exception());
                }
            }
        };

        template <typename T>
        struct make_promise_helper {
            template <typename Functor>
            inline static void dispatch( Functor &&f, const std::shared_ptr<std::promise<T>> &p ) THENABLE_NOEXCEPT {
                try {
                    f( [p]( const T &resolved_value ) THENABLE_NOEXCEPT {
                        p->set_value( resolved_value );

                    }, [p]( auto rejected_value ) THENABLE_NOEXCEPT {
                        p->set_exception( std::make_exception_ptr( rejected_value ));
                    } );

                } catch( ... ) {
                    p->set_exception( std::current_exception());
                }
            }
        };

        template <>
        struct make_promise_helper<void> {
            template <typename Functor>
            inline static void dispatch( Functor &&f, const std::shared_ptr<std::promise<void>> &p ) THENABLE_NOEXCEPT {
                try {
                    f( [p]() THENABLE_NOEXCEPT {
                        p->set_value();

                    }, [p]( auto rejected_value ) THENABLE_NOEXCEPT {
                        p->set_exception( std::make_exception_ptr( rejected_value ));
                    } );

                } catch( ... ) {
                    p->set_exception( std::current_exception());
                }
            }
        };
    }

    //This is fun...
    template <typename Functor, typename FutureType>
    using implicit_result_of = decltype( detail::then_helper<typename detail::get_future_type<typename std::remove_reference<FutureType>::type>::type, Functor>::dispatch(
        std::forward<FutureType>( std::declval<FutureType>()), std::forward<Functor>( std::declval<Functor>())));

    //////////

    template <typename T, typename Functor>
    std::future<implicit_result_of<Functor, std::future<T>>> then( std::future<T> &, Functor &&, std::launch = default_policy );

    template <typename T, typename Functor>
    std::future<implicit_result_of<Functor, std::shared_future<T>>> then( std::shared_future<T> &&, Functor &&, std::launch = default_policy );

    template <typename T, typename Functor>
    std::future<implicit_result_of<Functor, std::shared_future<T>>> then( std::shared_future<T>, Functor &&, std::launch = default_policy );

    template <typename T, typename Functor>
    std::future<implicit_result_of<Functor, std::future<T>>> then( std::future<T> &&, Functor &&, std::launch = default_policy );

    template <typename T, typename Functor>
    std::future<implicit_result_of<Functor, std::future<T>>> then( std::promise<T> &, Functor &&, std::launch = default_policy );

    //////////

    template <typename T, typename Functor>
    std::future<implicit_result_of<Functor, std::future<T>>> then( std::future<T> &, Functor &&, then_launch );

    template <typename T, typename Functor>
    std::future<implicit_result_of<Functor, std::shared_future<T>>> then( std::shared_future<T> &&, Functor &&, then_launch );

    template <typename T, typename Functor>
    std::future<implicit_result_of<Functor, std::shared_future<T>>> then( std::shared_future<T>, Functor &&, then_launch );

    template <typename T, typename Functor>
    std::future<implicit_result_of<Functor, std::future<T>>> then( std::future<T> &&, Functor &&, then_launch );

    template <typename T, typename Functor>
    std::future<implicit_result_of<Functor, std::future<T>>> then( std::promise<T> &, Functor &&, then_launch );


    /*
     * then function
     *
     * This is the overload of then that resolves futures and promises and invokes a callback with the
     * resulting value. It uses the standard std::async for asynchronous invocation.
     * */

    /*
     * Overload for simple futures
     * */
    template <typename T, typename Functor>
    std::future<implicit_result_of<Functor, std::future<T>>> then( std::future<T> &&s, Functor &&f, std::launch policy ) {
        return std::async( policy, [policy]( std::future<T> &&s2, Functor &&f2 ) {
            return detail::then_helper<T, Functor>::dispatch( std::forward<std::future<T>>( s2 ), std::forward<Functor>( f2 ));
        }, std::forward<std::future<T>>( s ), std::forward<Functor>( f ));
    };

    /*
     * Overload for futures by references. It will take ownership and forward the future,
     * calling the above overload.
     * */
    template <typename T, typename Functor>
    inline std::future<implicit_result_of<Functor, std::future<T>>> then( std::future<T> &s, Functor &&f, std::launch policy ) {
        return then( std::forward<std::future<T>>( s ), std::forward<Functor>( f ), policy );
    };

    /*
     * Overload for shared_futures, passed by value because they can be copied.
     * It's similar to the first overload, but can capture the shared_future in the lambda.
     * */
    template <typename T, typename Functor>
    inline std::future<implicit_result_of<Functor, std::shared_future<T>>> then( std::shared_future<T> &&s, Functor &&f, std::launch policy ) {
        return std::async( policy, [policy]( std::shared_future<T> &&s2, Functor &&f2 ) {
            return detail::then_helper<T, Functor>::dispatch( std::forward<std::shared_future<T>>( s2 ), std::forward<Functor>( f2 ));
        }, std::forward<std::shared_future<T>>( s ), std::forward<Functor>( f ));
    };

    template <typename T, typename Functor>
    inline std::future<implicit_result_of<Functor, std::shared_future<T>>> then( std::shared_future<T> s, Functor &&f, std::launch policy ) {
        return then( std::move( s ), std::forward<Functor>( f ), policy );
    };

    /*
     * Overload for promise references. The promise is NOT moved, but rather the future is acquired
     * from it and the promise is left intact, so it can be used elsewhere.
     * */
    template <typename T, typename Functor>
    inline std::future<implicit_result_of<Functor, std::future<T>>> then( std::promise<T> &s, Functor &&f, std::launch policy ) {
        return then( s.get_future(), std::forward<Functor>( f ), policy );
    };

    //////////

    /*
     * then function with detached launch flag.
     *
     * These are the same as the normal then functions, but launch the future resolution in a new thread
     * to immediately wait on them and execute the callbacks. That way the future destructors don't block the main thread.
     * */

    /*
     * Overload for simple futures
     * */
    template <typename T, typename Functor>
    std::future<implicit_result_of<Functor, std::future<T>>> then( std::future<T> &&s, Functor &&f, then_launch policy ) {
        //I don't really like having to do this, but I don't feel like rewriting almost all the recursive template logic above
        typedef implicit_result_of<Functor, std::future<T>> P;

        assert( policy == then_launch::detached );

        /*
         * A shared pointer is used to keep the shared state of the future alive in both threads until it's resolved
         * */

        auto p = std::make_shared<std::promise<P>>();

        std::thread( [p]( std::future<T> &&s2, Functor &&f2 ) {
            detail::detached_then_helper<P>::dispatch( *p, std::forward<std::future<T>>( s2 ), std::forward<Functor>( f2 ));
        }, std::forward<std::future<T>>( s ), std::forward<Functor>( f )).detach();

        return p->get_future();
    };

    /*
     * Overload for futures by references. It will take ownership and move the future,
     * calling the above overload.
     * */
    template <typename T, typename Functor>
    inline std::future<implicit_result_of<Functor, std::future<T>>> then( std::future<T> &s, Functor &&f, then_launch policy ) {
        return then( std::move( s ), std::forward<Functor>( f ), policy );
    };

    /*
     * Overload for shared_futures, passed by value because they can be copied.
     * It's similar to the first overload, but can capture the shared_future in the lambda.
     * */
    template <typename T, typename Functor>
    inline std::future<implicit_result_of<Functor, std::shared_future<T>>> then( std::shared_future<T> &&s, Functor &&f, then_launch policy ) {
        //I don't really like having to do this, but I don't feel like rewriting almost all the recursive template logic above
        typedef implicit_result_of<Functor, std::shared_future<T>> P;

        assert( policy == then_launch::detached );

        /*
         * A shared pointer is used to keep the shared state of the future alive in both threads until it's resolved
         * */

        auto p = std::make_shared<std::promise<P>>();

        std::thread( [p]( std::shared_future<T> &&s2, Functor &&f2 ) {
            detail::detached_then_helper<P>::dispatch( *p, std::forward<std::shared_future<T>>( s2 ), std::forward<Functor>( f2 ));
        }, std::forward<std::shared_future<T>>( s ), std::forward<Functor>( f )).detach();

        return p->get_future();
    };

    template <typename T, typename Functor>
    inline std::future<implicit_result_of<Functor, std::shared_future<T>>> then( std::shared_future<T> s, Functor &&f, then_launch policy ) {
        return then( std::move( s ), std::forward<Functor>( f ), policy );
    };

    /*
     * Overload for promise references. The promise is NOT moved, but rather the future is acquired
     * from it and the promise is left intact, so it can be used elsewhere.
     * */
    template <typename T, typename Functor>
    inline std::future<implicit_result_of<Functor, std::future<T>>> then( std::promise<T> &s, Functor &&f, then_launch policy ) {
        return then( s.get_future(), std::forward<Functor>( f ), policy );
    };

    //////////

    /*
     * then2 is a variation of then that returns a ThenableFuture instead of a normal future
     * */
    template <typename FutureType, typename Functor, typename LaunchPolicy>
    inline ThenableFuture<implicit_result_of<Functor, FutureType>> then2( FutureType &&s, Functor &&f, LaunchPolicy policy ) {
        return to_thenable( then( std::forward<FutureType>( s ), std::forward<Functor>( f ), policy ));
    }

    template <typename FutureType, typename Functor, typename LaunchPolicy>
    inline ThenableFuture<implicit_result_of<Functor, FutureType>> then2( FutureType &s, Functor &&f, LaunchPolicy policy ) {
        return to_thenable( then( s, std::forward<Functor>( f ), policy ));
    }

    //////////

    template <typename T>
    class ThenablePromise : public std::promise<T> {
        public:
            inline ThenablePromise() : std::promise<T>() {}

            inline ThenablePromise( std::promise<T> &&p ) : std::promise<T>( std::forward<std::promise<T>>( p )) {}

            inline ThenablePromise( ThenablePromise &&p ) : std::promise<T>( std::forward<std::promise<T>>( p )) {}

            ThenablePromise( const ThenablePromise & ) = delete;

            ThenablePromise &operator=( const ThenablePromise & ) = delete;

            constexpr operator std::promise<T> &() THENABLE_NOEXCEPT {
                return *static_cast<std::promise<T> *>(this);
            }

            constexpr operator std::promise<T> &&() THENABLE_NOEXCEPT {
                return std::move( *static_cast<std::promise<T> *>(this));
            }

            inline ThenableFuture<T> get_thenable_future() {
                return this->get_future(); //Automatically converts it
            }

            template <typename Functor, typename LaunchPolicy = std::launch>
            inline ThenableFuture<implicit_result_of<Functor, std::future<T>>> then( Functor &&f, LaunchPolicy policy = default_policy ) {
                return then2( this->get_future(), std::forward<Functor>( f ), policy );
            }
    };

    /*
     * This is a future object functionally equivalent to std::future,
     * but with a .then function to chain together many futures.
     * */

    template <typename T>
    class ThenableFuture : public std::future<T> {
        public:
            constexpr ThenableFuture() THENABLE_NOEXCEPT : std::future<T>() {}

            inline ThenableFuture( std::future<T> &&f ) THENABLE_NOEXCEPT : std::future<T>( std::forward<std::future<T>>( f )) {}

            inline ThenableFuture( ThenableFuture &&f ) THENABLE_NOEXCEPT : std::future<T>( std::forward<std::future<T>>( f )) {}

            ThenableFuture( const ThenableFuture & ) = delete;

            ThenableFuture &operator=( const ThenableFuture & ) = delete;

            constexpr operator std::future<T> &() {
                return *static_cast<std::future<T> *>(this);
            }

            constexpr operator std::future<T> &&() {
                return std::move( *static_cast<std::future<T> *>(this));
            }

            template <typename Functor, typename LaunchPolicy = std::launch>
            inline ThenableFuture<implicit_result_of<Functor, std::future<T>>> then( Functor &&f, LaunchPolicy policy = default_policy ) {
                return then2( std::move( *this ), std::forward<Functor>( f ), policy );
            }

            inline ThenableSharedFuture<T> share_thenable() {
                return ThenableSharedFuture<T>( this->share());
            }
    };

    template <typename T>
    class ThenableSharedFuture : public std::shared_future<T> {
        public:
            constexpr ThenableSharedFuture() THENABLE_NOEXCEPT : std::shared_future<T>() {}

            inline ThenableSharedFuture( const std::shared_future<T> &f ) THENABLE_NOEXCEPT : std::shared_future<T>( f ) {}

            inline ThenableSharedFuture( const ThenableSharedFuture &f ) THENABLE_NOEXCEPT : std::shared_future<T>( f ) {}

            inline ThenableSharedFuture( std::future<T> &&f ) THENABLE_NOEXCEPT : std::shared_future<T>( std::forward<std::shared_future<T>>( f )) {}

            inline ThenableSharedFuture( ThenableFuture<T> &&f ) THENABLE_NOEXCEPT : std::shared_future<T>( std::forward<std::shared_future<T>>( f )) {}

            inline ThenableSharedFuture( std::shared_future<T> &&f ) THENABLE_NOEXCEPT : std::shared_future<T>( std::forward<std::shared_future<T>>( f )) {}

            inline ThenableSharedFuture( ThenableSharedFuture &&f ) THENABLE_NOEXCEPT : std::shared_future<T>( std::forward<std::shared_future<T>>( f )) {}

            constexpr operator std::shared_future<T> &() {
                return *static_cast<std::shared_future<T> *>(this);
            }

            constexpr operator std::shared_future<T> &&() {
                return std::move( *static_cast<std::shared_future<T> *>(this));
            }

            template <typename Functor, typename LaunchPolicy = std::launch>
            inline ThenableFuture<implicit_result_of<Functor, std::shared_future<T>>> then( Functor &&f, LaunchPolicy policy = default_policy ) {
                return then2( *this, std::forward<Functor>( f ), policy );
            }
    };

    //////////

    namespace detail {
        template <typename T>
        inline typename recursive_get_future_type<T>::type recursive_get( ThenableFuture<T> &&t ) {
            return recursive_get( t.get());
        };

        template <typename T>
        inline typename recursive_get_future_type<T>::type recursive_get( ThenableSharedFuture<T> &&t ) {
            return recursive_get( t.get());
        };

        template <>
        inline typename recursive_get_future_type<void>::type recursive_get<void>( ThenableFuture<void> &&t ) {
            t.get();
        };

        template <>
        inline typename recursive_get_future_type<void>::type recursive_get<void>( ThenableSharedFuture<void> &&t ) {
            t.get();
        };

        template <typename T>
        inline typename recursive_get_future_type<T>::type recursive_get( ThenablePromise<T> &&t ) {
            return recursive_get( t.get_future());
        };
    }

    //////////

    /*
     * These just convert future, shared_future and promise to their Thenable equivalent
     * */

    template <typename T>
    constexpr ThenableFuture<T> to_thenable( std::future<T> &&t ) {
        return ThenableFuture<T>( std::forward<std::future<T>>( t ));
    }

    template <typename T>
    constexpr ThenableSharedFuture<T> to_thenable( const std::shared_future<T> &t ) {
        return ThenableSharedFuture<T>( t );
    }

    template <typename T>
    constexpr ThenableSharedFuture<T> to_thenable( std::shared_future<T> &&t ) {
        return ThenableSharedFuture<T>( std::forward<std::shared_future<T>>( t ));
    }

    template <typename T>
    constexpr ThenablePromise<T> to_thenable( std::promise<T> &&t ) {
        return ThenablePromise<T>( std::forward<std::promise<T>>( t ));
    }

    //////////

    template <typename T>
    constexpr ThenableFuture<T> to_thenable( ThenableFuture<T> &&t ) {
        return std::forward<ThenableFuture<T>>( t );
    }

    template <typename T>
    constexpr ThenableSharedFuture<T> to_thenable( const ThenableSharedFuture<T> &t ) {
        return std::forward<ThenableSharedFuture<T>>( t );
    }

    template <typename T>
    constexpr ThenableSharedFuture<T> to_thenable( ThenableSharedFuture<T> &&t ) {
        return std::forward<ThenableSharedFuture<T>>( t );
    }

    template <typename T>
    constexpr ThenablePromise<T> to_thenable( ThenablePromise<T> &&t ) {
        return std::forward<ThenablePromise<T>>( t );
    }

    //////////

    template <typename... Results>
    constexpr std::tuple<ThenableFuture<Results>...> to_thenable( std::tuple<std::future<Results>...> &&futures ) {
        return std::tuple<ThenableFuture<Results>...>( std::forward<std::tuple<std::future<Results>...>>( futures ));
    }

    template <typename... Results>
    constexpr std::tuple<ThenableSharedFuture<Results>...> to_thenable( std::tuple<std::shared_future<Results>...> &&futures ) {
        return std::tuple<ThenableSharedFuture<Results>...>( std::forward<std::tuple<std::shared_future<Results>...>>( futures ));
    }

    template <typename... Results>
    constexpr std::tuple<ThenablePromise<Results>...> to_thenable( std::tuple<std::promise<Results>...> &&promises ) {
        return std::tuple<ThenablePromise<Results>...>( std::forward<std::tuple<std::promise<Results>...>>( promises ));
    }

    //////////

    template <typename... Results>
    constexpr std::tuple<ThenableFuture<Results>...> to_thenable( std::tuple<ThenableFuture<Results>...> &&futures ) {
        return std::forward<std::tuple<ThenableFuture<Results>...>>( futures );
    }

    template <typename... Results>
    constexpr std::tuple<ThenableSharedFuture<Results>...> to_thenable( std::tuple<ThenableSharedFuture<Results>...> &&futures ) {
        return std::forward<std::tuple<ThenableSharedFuture<Results>...>>( futures );
    }

    template <typename... Results>
    constexpr std::tuple<ThenablePromise<Results>...> to_thenable( std::tuple<ThenablePromise<Results>...> &&promises ) {
        return std::forward<std::tuple<ThenablePromise<Results>...>>( promises );
    }

    //////////

    template <typename T, typename Functor, typename LaunchPolicy>
    std::future<T> make_promise( Functor &&f, LaunchPolicy policy ) {
        auto p = std::make_shared<std::promise<T>>();

        return then( defer( [p]( Functor &&f2 ) THENABLE_NOEXCEPT {
            detail::make_promise_helper<T>::dispatch( std::forward<Functor>( f2 ), p );

        }, std::forward<Functor>( f )), [p] {
            return p->get_future();

        }, policy );
    }

    template <typename T, typename Functor, typename LaunchPolicy>
    ThenableFuture<T> make_promise2( Functor &&f, LaunchPolicy policy ) {
        auto p = std::make_shared<std::promise<T>>();

        return then2( defer( [p]( Functor &&f2 ) THENABLE_NOEXCEPT {
            detail::make_promise_helper<T>::dispatch( std::forward<Functor>( f2 ), p );

        }, std::forward<Functor>( f )), [p] {
            return p->get_future();

        }, policy );
    }

    //////////

    namespace detail {
        template <typename... Functors>
        using promise_tuple = std::tuple<std::promise<typename recursive_get_future_type<fn_traits::fn_result_of<Functors>>::type>...>;

        template <typename... Functors>
        using result_tuple = std::tuple<std::future<typename detail::recursive_get_future_type<fn_traits::fn_result_of<Functors>>::type>...>;

        template <size_t i, typename... Functors>
        constexpr typename std::enable_if<i == sizeof...( Functors )>::type
        initialize_parallel_futures( result_tuple<Functors...> &, promise_tuple<Functors...> & ) {
            //No-op
        };

        template <size_t i, typename... Functors>
        inline typename std::enable_if<i < sizeof...( Functors )>::type
        initialize_parallel_futures( result_tuple<Functors...> &result, promise_tuple<Functors...> &promises ) {
            std::get<i>( result ) = std::get<i>( promises ).get_future();

            initialize_parallel_futures<i + 1, Functors...>( result, promises );
        };

        template <typename Functor, typename R = fn_traits::fn_result_of<Functor>>
        struct tagged_functor {
            std::atomic_bool ran;
            Functor          f;

            inline tagged_functor( Functor &&_f ) : f( std::forward<Functor>( _f )) {}

            inline void invoke( std::promise<typename recursive_get_future_type<fn_traits::fn_result_of<Functor>>::type> &p ) THENABLE_NOEXCEPT {
                try {
                    p.set_value( recursive_get( f()));

                } catch( ... ) {
                    p.set_exception( std::current_exception());
                }
            }
        };

        template <typename Functor>
        struct tagged_functor<Functor, void> {
            std::atomic_bool ran;
            Functor          f;

            inline tagged_functor( Functor &&_f ) : f( std::forward<Functor>( _f )) {}

            inline void invoke( std::promise<void> &p ) THENABLE_NOEXCEPT {
                try {
                    f();

                    p.set_value();

                } catch( ... ) {
                    p.set_exception( std::current_exception());
                }
            }
        };

        template <size_t i, typename... Functors>
        constexpr typename std::enable_if<i == sizeof...( Functors )>::type
        invoke_parallel_functors( promise_tuple<Functors...> &, std::tuple<tagged_functor<Functors>...> & ) THENABLE_NOEXCEPT {
            //No-op
        };

        template <size_t i, typename... Functors>
        inline typename std::enable_if<i < sizeof...( Functors )>::type
        invoke_parallel_functors( promise_tuple<Functors...> &promises, std::tuple<tagged_functor<Functors>...> &fns ) THENABLE_NOEXCEPT {
            bool expect_ran = false;

            auto &func = std::get<i>( fns );

            func.ran.compare_exchange_strong( expect_ran, true );

            if( !expect_ran ) {
                func.invoke( std::get<i>( promises ));
            }

            invoke_parallel_functors<i + 1, Functors...>( promises, fns );
        };

        template <typename K, typename T, std::size_t... S>
        inline K get_tuple_futures( T &&t, std::index_sequence<S...> ) {
            return K( recursive_get( std::get<S>( std::forward<T>( t )))... );
        }

        template <typename K, typename T, std::size_t... S>
        inline K get_tuple_futures_from_promises( T &&t, std::index_sequence<S...> ) {
            return K( recursive_get( std::get<S>( std::forward<T>( t )).get_future())... );
        }
    }

    template <typename... Functors>
    std::tuple<std::future<recursive_result_of<Functors>>...> parallel_n( size_t concurrency, Functors &&... fns ) {
        static_assert( sizeof...( Functors ) > 0 );
        assert( concurrency > 0 );

        typedef detail::promise_tuple<Functors...>              promise_tuple;
        typedef detail::result_tuple<Functors...>               result_tuple;
        typedef std::tuple<detail::tagged_functor<Functors>...> tagged_functors;

        result_tuple result;

        auto p = std::make_shared<promise_tuple>();
        auto f = std::make_shared<tagged_functors>( std::forward<Functors>( fns )... );

        detail::initialize_parallel_futures<0, Functors...>( result, *p );

        for( size_t i = 0, min_concurrency = std::min( concurrency, sizeof...( Functors )); i < min_concurrency; ++i ) {
            std::thread( [p, f]() THENABLE_NOEXCEPT {
                detail::invoke_parallel_functors<0, Functors...>( *p, *f );
            } ).detach();
        }

        return std::move( result );
    }

    template <typename... Functors>
    inline std::tuple<std::future<recursive_result_of<Functors>>...> parallel( Functors &&... fns ) {
        return parallel_n( std::thread::hardware_concurrency(), std::forward<Functors>( fns )... );
    }

    template <typename... Functors>
    inline std::tuple<ThenableFuture<recursive_result_of<Functors>>...> parallel2_n( size_t concurrency, Functors &&... fns ) {
        //Implicit conversion to ThenableFuture
        return parallel_n( concurrency, std::forward<Functors>( fns )... );
    }

    template <typename... Functors>
    inline std::tuple<ThenableFuture<recursive_result_of<Functors>>...> parallel2( Functors &&... fns ) {
        //Implicit conversion to ThenableFuture
        return parallel( std::forward<Functors>( fns )... );
    }

    //////////

    template <typename... Results>
    std::future<std::tuple<Results...>> await_all( std::tuple<std::future<Results>...> &&results, std::launch policy = default_policy ) {
        typedef std::tuple<std::future<Results>...> tuple_type;
        constexpr auto                              Size = std::tuple_size<tuple_type>::value;

        return std::async( policy, []( tuple_type &&inner_results ) {
            return detail::get_tuple_futures<std::tuple<Results...>>( std::forward<tuple_type>( inner_results ), std::make_index_sequence<Size>());
        }, std::forward<tuple_type>( results ));
    }

    template <typename... Results>
    std::future<std::tuple<Results...>> await_all( std::tuple<std::shared_future<Results>...> &&results, std::launch policy = default_policy ) {
        typedef std::tuple<std::shared_future<Results>...> tuple_type;
        constexpr auto                                     Size = std::tuple_size<tuple_type>::value;

        return std::async( policy, []( tuple_type &&inner_results ) {
            return detail::get_tuple_futures<std::tuple<Results...>>( std::forward<tuple_type>( inner_results ), std::make_index_sequence<Size>());
        }, std::forward<tuple_type>( results ));
    }

    template <typename... Results>
    ThenableFuture<std::tuple<Results...>> await_all( std::tuple<ThenableFuture<Results>...> &&results, std::launch policy = default_policy ) {
        typedef std::tuple<ThenableFuture<Results>...> tuple_type;
        constexpr auto                                 Size = std::tuple_size<tuple_type>::value;

        return std::async( policy, []( tuple_type &&inner_results ) {
            return detail::get_tuple_futures<std::tuple<Results...>>( std::forward<tuple_type>( inner_results ), std::make_index_sequence<Size>());
        }, std::forward<tuple_type>( results ));
    }

    template <typename... Results>
    ThenableFuture<std::tuple<Results...>> await_all( std::tuple<ThenableSharedFuture<Results>...> &&results, std::launch policy = default_policy ) {
        typedef std::tuple<ThenableSharedFuture<Results>...> tuple_type;
        constexpr auto                                       Size = std::tuple_size<tuple_type>::value;

        return std::async( policy, []( tuple_type &&inner_results ) {
            return detail::get_tuple_futures<std::tuple<Results...>>( std::forward<tuple_type>( inner_results ), std::make_index_sequence<Size>());
        }, std::forward<tuple_type>( results ));
    }

    template <typename... Results>
    std::future<std::tuple<Results...>> await_all( std::tuple<std::promise<Results>...> &&results, std::launch policy = default_policy ) {
        typedef std::tuple<ThenableSharedFuture<Results>...> tuple_type;
        constexpr auto                                       Size = std::tuple_size<tuple_type>::value;

        return std::async( policy, []( tuple_type &&inner_results ) {
            return detail::get_tuple_futures_from_promises<std::tuple<Results...>>( std::forward<tuple_type>( inner_results ), std::make_index_sequence<Size>());
        }, std::forward<tuple_type>( results ));
    }

    template <typename... Results>
    ThenableFuture<std::tuple<Results...>> await_all( std::tuple<ThenablePromise<Results>...> &&results, std::launch policy = default_policy ) {
        typedef std::tuple<ThenableSharedFuture<Results>...> tuple_type;
        constexpr auto                                       Size = std::tuple_size<tuple_type>::value;

        return std::async( policy, []( tuple_type &&inner_results ) {
            return detail::get_tuple_futures_from_promises<std::tuple<Results...>>( std::forward<tuple_type>( inner_results ), std::make_index_sequence<Size>());
        }, std::forward<tuple_type>( results ));
    }

    //////////

    template <typename... Results>
    std::future<std::tuple<Results...>> await_all( std::tuple<std::future<Results>...> &&results, then_launch policy ) {
        typedef std::tuple<std::future<Results>...> tuple_type;
        constexpr auto                              Size = std::tuple_size<tuple_type>::value;

        assert( policy == then_launch::detached );

        auto p = std::make_shared<std::promise<std::tuple<Results...>>>();

        std::thread( [p]( tuple_type &&inner_results ) {
            try {
                p->set_value( detail::get_tuple_futures<std::tuple<Results...>>( std::forward<tuple_type>( inner_results ), std::make_index_sequence<Size>()));

            } catch( ... ) {
                p->set_exception( std::current_exception());
            }
        }, std::forward<tuple_type>( results )).detach();

        return p->get_future();
    }

    template <typename... Results>
    std::future<std::tuple<Results...>> await_all( std::tuple<std::shared_future<Results>...> &&results, then_launch policy ) {
        typedef std::tuple<std::shared_future<Results>...> tuple_type;
        constexpr auto                                     Size = std::tuple_size<tuple_type>::value;

        assert( policy == then_launch::detached );

        auto p = std::make_shared<std::promise<std::tuple<Results...>>>();

        std::thread( [p]( tuple_type &&inner_results ) {
            try {
                p->set_value( detail::get_tuple_futures<std::tuple<Results...>>( std::forward<tuple_type>( inner_results ), std::make_index_sequence<Size>()));

            } catch( ... ) {
                p->set_exception( std::current_exception());
            }
        }, std::forward<tuple_type>( results )).detach();

        return p->get_future();
    }

    template <typename... Results>
    ThenableFuture<std::tuple<Results...>> await_all( std::tuple<ThenableFuture<Results>...> &&results, then_launch policy ) {
        typedef std::tuple<ThenableFuture<Results>...> tuple_type;
        constexpr auto                                 Size = std::tuple_size<tuple_type>::value;

        assert( policy == then_launch::detached );

        auto p = std::make_shared<std::promise<std::tuple<Results...>>>();

        std::thread( [p]( tuple_type &&inner_results ) {
            try {
                p->set_value( detail::get_tuple_futures<std::tuple<Results...>>( std::forward<tuple_type>( inner_results ), std::make_index_sequence<Size>()));

            } catch( ... ) {
                p->set_exception( std::current_exception());
            }
        }, std::forward<tuple_type>( results )).detach();

        return p->get_future();
    }

    template <typename... Results>
    ThenableFuture<std::tuple<Results...>> await_all( std::tuple<ThenableSharedFuture<Results>...> &&results, then_launch policy ) {
        typedef std::tuple<ThenableSharedFuture<Results>...> tuple_type;
        constexpr auto                                       Size = std::tuple_size<tuple_type>::value;

        assert( policy == then_launch::detached );

        auto p = std::make_shared<std::promise<std::tuple<Results...>>>();

        std::thread( [p]( tuple_type &&inner_results ) {
            try {
                p->set_value( detail::get_tuple_futures<std::tuple<Results...>>( std::forward<tuple_type>( inner_results ), std::make_index_sequence<Size>()));

            } catch( ... ) {
                p->set_exception( std::current_exception());
            }
        }, std::forward<tuple_type>( results )).detach();

        return p->get_future();
    }


    template <typename... Results>
    std::future<std::tuple<Results...>> await_all( std::tuple<std::promise<Results>...> &&results, then_launch policy ) {
        typedef std::tuple<ThenableSharedFuture<Results>...> tuple_type;
        constexpr auto                                       Size = std::tuple_size<tuple_type>::value;

        assert( policy == then_launch::detached );

        auto p = std::make_shared<std::promise<std::tuple<Results...>>>();

        std::thread( [p]( tuple_type &&inner_results ) {
            try {
                p->set_value( detail::get_tuple_futures_from_promises<std::tuple<Results...>>( std::forward<tuple_type>( inner_results ), std::make_index_sequence<Size>()));

            } catch( ... ) {
                p->set_exception( std::current_exception());
            }
        }, std::forward<tuple_type>( results )).detach();

        return p->get_future();
    }

    template <typename... Results>
    ThenableFuture<std::tuple<Results...>> await_all( std::tuple<ThenablePromise<Results>...> &&results, then_launch policy ) {
        typedef std::tuple<ThenableSharedFuture<Results>...> tuple_type;
        constexpr auto                                       Size = std::tuple_size<tuple_type>::value;

        assert( policy == then_launch::detached );

        auto p = std::make_shared<std::promise<std::tuple<Results...>>>();

        std::thread( [p]( tuple_type &&inner_results ) {
            try {
                p->set_value( detail::get_tuple_futures_from_promises<std::tuple<Results...>>( std::forward<tuple_type>( inner_results ), std::make_index_sequence<Size>()));

            } catch( ... ) {
                p->set_exception( std::current_exception());
            }
        }, std::forward<tuple_type>( results )).detach();

        return p->get_future();
    }

    namespace detail {
        /*
         * These are very similar to the detached_then_helper helper structures, exception it bypasses the then_helper::dispatch part since this doesn't have to wait
         * on a future. It just invokes it with a value using then_invoke_helper, and sets the promise to the result
         * */
        template <typename T>
        struct detached_waterfall_helper {
            template <typename Functor>
            static inline void dispatch( std::promise<T> &p, Functor &&f ) {
                try {
                    p.set_value( then_invoke_helper<Functor>::invoke( std::forward<Functor>( f )));

                } catch( ... ) {
                    p.set_exception( std::current_exception());
                }
            }
        };

        template <>
        struct detached_waterfall_helper<void> {
            template <typename Functor>
            static inline void dispatch( std::promise<void> &p, Functor &&f ) {
                try {
                    then_invoke_helper<Functor>::invoke( std::forward<Functor>( f ));

                    p.set_value();

                } catch( ... ) {
                    p.set_exception( std::current_exception());
                }
            }
        };
    }


    /*
     * So because of the nature of variadic templates, the actual waterfall implementation needed to be in reverse.
     * It goes from the last functor to first
     * */
    template <typename Functor>
    inline THENABLE_DECLTYPE_AUTO_HINTED( std::future ) reverse_waterfall( std::launch policy, Functor &&f ) {
        return std::async( policy, []( Functor &&f2 ) {
            return detail::then_invoke_helper<Functor>::invoke( std::forward<Functor>( f2 ));
        }, std::forward<Functor>( f ));
    }

    template <typename Functor>
    THENABLE_DECLTYPE_AUTO_HINTED( std::future ) reverse_waterfall( then_launch policy, Functor &&f ) {
        typedef decltype( detail::then_invoke_helper<Functor>::invoke( std::forward<Functor>( f ))) P;

        assert( policy == then_launch::detached );

        auto p = std::make_shared<std::promise<P>>();

        std::thread( [p]( Functor &&f2 ) {
            detail::detached_waterfall_helper<P>::dispatch( *p, std::forward<Functor>( f2 ));
        }, std::forward<Functor>( f )).detach();

        return p->get_future();
    }

    template <typename PolicyType, typename Functor, typename... Functors>
    inline THENABLE_DECLTYPE_AUTO_HINTED( std::future ) reverse_waterfall( PolicyType policy, Functor &&f, Functors &&... fns ) {
        return then( reverse_waterfall( policy, std::forward<Functors>( fns )... ), std::forward<Functor>( f ), policy );
    }


    namespace detail {
        /*
         * Found this solution at http://stackoverflow.com/a/15907838
         *
         * It applies variadic templates to a function in reverse by recursively calling itself with the template parameters shifted over
         * */

        template <class ...Head>
        struct forward_waterfall;

        template <>
        struct forward_waterfall<> {
            template <class ...Rest>
            static inline decltype( auto ) apply( Rest &&... rest ) {
                return reverse_waterfall( std::forward<Rest>( rest )... );
            }
        };

        template <class First, class ...Head>
        struct forward_waterfall<First, Head...> {
            template <class ...Rest>
            static inline decltype( auto ) apply( First &&first, Head &&... head, Rest &&... rest ) {
                return forward_waterfall<Head...>::apply( std::forward<Head>( head )..., std::forward<First>( first ), std::forward<Rest>( rest )... );
            }
        };
    }


    /*
     * Actual waterfall implementations that just forward the policy and functors through the forward_waterfall
     * which reverses them and calls reverse_waterfall
     * */

    template <typename... Functors>
    inline THENABLE_DECLTYPE_AUTO_HINTED( std::future ) waterfall( std::launch policy, Functors &&... fns ) {
        return detail::forward_waterfall<Functors..., std::launch>::apply( std::forward<Functors>( fns )..., std::forward<std::launch>( policy ));
    }

    template <typename... Functors>
    inline THENABLE_DECLTYPE_AUTO_HINTED( std::future ) waterfall( then_launch policy, Functors &&... fns ) {
        return detail::forward_waterfall<Functors..., then_launch>::apply( std::forward<Functors>( fns )..., std::forward<then_launch>( policy ));
    }

    template <typename... Functors>
    inline THENABLE_DECLTYPE_AUTO_HINTED( std::future ) waterfall( Functors &&... fns ) {
        return waterfall( default_policy, std::forward<Functors>( fns )... );
    }

    template <typename... Args>
    inline THENABLE_DECLTYPE_AUTO_HINTED( std::future ) waterfall2( Args &&... args ) {
        return to_thenable( waterfall( std::forward<Args>( args )... ));
    };
}

#endif //THENABLE_IMPLEMENTATION_HPP
