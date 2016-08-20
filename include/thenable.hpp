//
// Created by Aaron on 8/17/2016.
//

#ifndef THENABLE_IMPLEMENTATION_HPP
#define THENABLE_IMPLEMENTATION_HPP

#include <function_traits.hpp>

#include <assert.h>
#include <future>
#include <memory>

#ifndef DOXYGEN_DEFINE
#define THENABLE_DECLTYPE_AUTO decltype(auto)
#else
#define THENABLE_DECLTYPE_AUTO auto
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

    /*
     * Forward declarations for thenable classes
     * */

    template <typename>
    class ThenableFuture;

    template <typename>
    class ThenableSharedFuture;

    template <typename>
    class ThenablePromise;

    //////////

    /*
     * Forward declarations and default arguments for standard then overloads
     * */

    template <typename T, typename Functor>
    THENABLE_DECLTYPE_AUTO then( std::future<T> &, Functor, std::launch = default_policy );

    template <typename T, typename Functor>
    THENABLE_DECLTYPE_AUTO then( std::shared_future<T>, Functor, std::launch = default_policy );

    template <typename T, typename Functor>
    THENABLE_DECLTYPE_AUTO then( std::future<T> &&, Functor, std::launch = default_policy );

    template <typename T, typename Functor>
    THENABLE_DECLTYPE_AUTO then( std::promise<T> &, Functor, std::launch = default_policy );

    //////////

    /*
     * Forward declarations (without default arguments) for detached then overloads.
     *
     * There are no default overloads because these will ONLY be called if the then_launch::detached
     * flag is given, ensuring these overloads are called instead of the above.
     * */

    template <typename T, typename Functor>
    THENABLE_DECLTYPE_AUTO then( std::future<T> &, Functor, then_launch );

    template <typename T, typename Functor>
    THENABLE_DECLTYPE_AUTO then( std::shared_future<T>, Functor, then_launch );

    template <typename T, typename Functor>
    THENABLE_DECLTYPE_AUTO then( std::future<T> &&, Functor, then_launch );

    template <typename T, typename Functor>
    THENABLE_DECLTYPE_AUTO then( std::promise<T> &, Functor, then_launch );

    //////////

    /*
     * Forward declaration for then2, which returns a ThenableFuture instead of a std::future
     * */

    template <typename... Args>
    THENABLE_DECLTYPE_AUTO then2( Args &&... );

    //////////

    /*
     * Forward declaration for to_thenable, which converts a future or shared_future into a ThenableFuture or ThenableSharedFuture
     * */

    template <typename T>
    inline ThenableFuture<T> to_thenable( std::future<T> && );

    template <typename T>
    inline ThenableSharedFuture<T> to_thenable( std::shared_future<T> );

    template <typename T>
    inline ThenablePromise<T> to_thenable( std::promise<T> && );

    namespace detail {
        using namespace fn_traits;

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
        THENABLE_DECLTYPE_AUTO recursive_get( std::future<T> && );

        template <typename T>
        THENABLE_DECLTYPE_AUTO recursive_get( std::shared_future<T> && );

        template <typename T>
        THENABLE_DECLTYPE_AUTO recursive_get( std::promise<T> && );

        template <typename T>
        THENABLE_DECLTYPE_AUTO recursive_get( ThenableFuture<T> && );

        template <typename T>
        THENABLE_DECLTYPE_AUTO recursive_get( ThenableSharedFuture<T> && );

        template <typename T>
        THENABLE_DECLTYPE_AUTO recursive_get( ThenablePromise<T> && );

        template <typename T>
        T recursive_get( T && ) noexcept;

        /*
         * std future implementations. ThenableX implementations are below their class implementation at the bottom of the file.
         * */

        template <typename T>
        inline THENABLE_DECLTYPE_AUTO recursive_get( std::future<T> &&t ) {
            return recursive_get( t.get());
        };

        template <typename T>
        inline THENABLE_DECLTYPE_AUTO recursive_get( std::shared_future<T> &&t ) {
            return recursive_get( t.get());
        };

        template <>
        inline THENABLE_DECLTYPE_AUTO recursive_get<void>( std::future<void> &&t ) {
            t.get();
        };

        template <>
        inline THENABLE_DECLTYPE_AUTO recursive_get<void>( std::shared_future<void> &&t ) {
            t.get();
        };

        template <typename T>
        inline THENABLE_DECLTYPE_AUTO recursive_get( std::promise<T> &&t ) {
            return recursive_get( t.get_future());
        };

        /*
         * This just forwards anything that is NOT a promise or future and doesn't do anything to it.
         * */
        template <typename T>
        inline T recursive_get( T &&t ) noexcept {
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
            template <typename... Args>
            inline static THENABLE_DECLTYPE_AUTO invoke( Functor f, Args &&... args ) {
                return recursive_get( f( std::forward<Args>( args )... ));
            }
        };

        /*
         * Specialization of then_invoke_helper for void callbacks.
         * */
        template <typename Functor>
        struct then_invoke_helper<Functor, void> {
            template <typename... Args>
            inline static void invoke( Functor f, Args &&... args ) {
                f( std::forward<Args>( args )... );
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
            inline static THENABLE_DECLTYPE_AUTO dispatch( std::future<T> &&s, Functor f ) {
                return then_invoke_helper<Functor>::invoke( f, recursive_get( std::forward<std::future<T>>( s )));
            }

            inline static THENABLE_DECLTYPE_AUTO dispatch( std::shared_future<T> s, Functor f ) {
                return then_invoke_helper<Functor>::invoke( f, recursive_get( s ));
            }
        };

        template <typename Functor>
        struct then_helper<void, Functor> {
            inline static THENABLE_DECLTYPE_AUTO dispatch( std::future<void> &&s, Functor f ) {
                s.get();

                return then_invoke_helper<Functor>::invoke( f );
            }

            inline static THENABLE_DECLTYPE_AUTO dispatch( std::shared_future<void> s, Functor f ) {
                s.get();

                return then_invoke_helper<Functor>::invoke( f );
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
            static inline void dispatch( std::promise<T> &p, std::future<K> &&s, Functor f ) {
                try {
                    p.set_value( then_helper<K, Functor>::dispatch( std::forward<std::future<K>>( s ), f ));

                } catch( ... ) {
                    p.set_exception( std::current_exception());
                }
            }

            template <typename Functor, typename K>
            static inline void dispatch( std::promise<T> &p, std::shared_future<K> s, Functor f ) {
                try {
                    p.set_value( then_helper<K, Functor>::dispatch( s, f ));

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
            static inline void dispatch( std::promise<void> &p, std::future<K> &&s, Functor f ) {
                try {
                    then_helper<K, Functor>::dispatch( std::forward<std::future<K>>( s ), f );

                    p.set_value();

                } catch( ... ) {
                    p.set_exception( std::current_exception());
                }
            }

            template <typename Functor, typename K>
            static inline void dispatch( std::promise<void> &p, std::shared_future<K> s, Functor f ) {
                try {
                    then_helper<K, Functor>::dispatch( s, f );

                    p.set_value();

                } catch( ... ) {
                    p.set_exception( std::current_exception());
                }
            }
        };

        //////////

        /*
         * Just a little trick to get the type of a future when it's not known beforehand.
         *
         * Used in then2 below.
         * */

        template <typename K>
        struct get_future_type {
            //No type is given here
        };

        template <typename T>
        struct get_future_type<std::future<T>> {
            typedef T type;
        };

        template <typename T>
        struct get_future_type<std::shared_future<T>> {
            typedef T type;
        };
    }

    //////////

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
    inline THENABLE_DECLTYPE_AUTO then( std::future<T> &&s, Functor f, std::launch policy ) {
        return std::async( policy, [policy, f]( std::future<T> &&s2 ) {
            return detail::then_helper<T, Functor>::dispatch( std::forward<std::future<T>>( s2 ), f );
        }, std::forward<std::future<T>>( s ));
    };

    /*
     * Overload for futures by references. It will take ownership and forward the future,
     * calling the above overload.
     * */
    template <typename T, typename Functor>
    inline THENABLE_DECLTYPE_AUTO then( std::future<T> &s, Functor f, std::launch policy ) {
        return then( std::forward<std::future<T>>( s ), f, policy );
    };

    /*
     * Overload for shared_futures, passed by value because they can be copied.
     * It's similar to the first overload, but can capture the shared_future in the lambda.
     * */
    template <typename T, typename Functor>
    inline THENABLE_DECLTYPE_AUTO then( std::shared_future<T> s, Functor f, std::launch policy ) {
        return std::async( policy, [policy, s, f]() {
            return detail::then_helper<T, Functor>::dispatch( s, f );
        } );
    };

    /*
     * Overload for promise references. The promise is NOT moved, but rather the future is acquired
     * from it and the promise is left intact, so it can be used elsewhere.
     * */
    template <typename T, typename Functor>
    inline THENABLE_DECLTYPE_AUTO then( std::promise<T> &s, Functor f, std::launch policy ) {
        return then( s.get_future(), f, policy );
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
    inline THENABLE_DECLTYPE_AUTO then( std::future<T> &&s, Functor f, then_launch policy ) {
        //I don't really like having to do this, but I don't feel like rewriting almost all the recursive template logic above
        typedef decltype( detail::then_helper<T, Functor>::dispatch( std::forward<std::future<T>>( s ), f )) P;

        //This is here because these templates are so complicated it's confused my IDE, so I kind of have to hint it's a type
        typedef std::promise<P> promiseP;

        assert( policy == then_launch::detached );

        /*
         * A shared pointer is used to keep the shared state of the future alive in both threads until it's resolved
         * */

        std::shared_ptr<promiseP> p = std::make_shared<promiseP>();

        std::thread( [p, f]( std::future<T> &&s2 ) {
            detail::detached_then_helper<P>::dispatch( *p, std::forward<std::future<T>>( s2 ), f );
        }, std::forward<std::future<T>>( s )).detach();

        return p->get_future();
    };

    /*
     * Overload for futures by references. It will take ownership and move the future,
     * calling the above overload.
     * */
    template <typename T, typename Functor>
    inline THENABLE_DECLTYPE_AUTO then( std::future<T> &s, Functor f, then_launch policy ) {
        return then( std::move( s ), f, policy );
    };

    /*
     * Overload for shared_futures, passed by value because they can be copied.
     * It's similar to the first overload, but can capture the shared_future in the lambda.
     * */
    template <typename T, typename Functor>
    inline THENABLE_DECLTYPE_AUTO then( std::shared_future<T> s, Functor f, then_launch policy ) {
        //I don't really like having to do this, but I don't feel like rewriting almost all the recursive template logic above
        typedef decltype( detail::then_helper<T, Functor>::dispatch( s, f )) P;

        //This is here because these templates are so complicated it's confused my IDE, so I kind of have to hint it's a type
        typedef std::promise<P> promiseP;

        assert( policy == then_launch::detached );

        /*
         * A shared pointer is used to keep the shared state of the future alive in both threads until it's resolved
         * */

        std::shared_ptr<promiseP> p = std::make_shared<promiseP>();

        std::thread( [s, p, f] {
            detail::detached_then_helper<P>::dispatch( *p, s, f );
        } ).detach();

        return p->get_future();
    };

    /*
     * Overload for promise references. The promise is NOT moved, but rather the future is acquired
     * from it and the promise is left intact, so it can be used elsewhere.
     * */
    template <typename T, typename Functor>
    inline THENABLE_DECLTYPE_AUTO then( std::promise<T> &s, Functor f, then_launch policy ) {
        return then( s.get_future(), f, policy );
    };

    //////////

    template <typename T>
    class ThenablePromise : public std::promise<T> {
        public:
            inline ThenablePromise() : std::promise<T>() {}

            inline ThenablePromise( std::promise<T> &&p ) : std::promise<T>( std::forward<std::promise<T>>( p )) {}

            inline ThenablePromise( ThenablePromise &&p ) : std::promise<T>( std::forward<std::promise<T>>( p )) {}

            ThenablePromise( const ThenablePromise & ) = delete;

            ThenablePromise &operator=( const ThenablePromise & ) = delete;

            inline operator std::promise<T> &() noexcept {
                return *static_cast<std::promise<T> *>(this);
            }

            inline operator std::promise<T> &&() noexcept {
                return std::move( *static_cast<std::promise<T> *>(this));
            }

            inline ThenableFuture<T> get_thenable_future() {
                return this->get_future(); //Automatically converts it
            }

            template <typename... Args>
            inline THENABLE_DECLTYPE_AUTO then( Args &&... args ) {
                return then2( this->get_future(), std::forward<Args>( args )... );
            }
    };

    /*
     * This is a future object functionally equivalent to std::future,
     * but with a .then function to chain together many futures.
     * */

    template <typename T>
    class ThenableFuture : public std::future<T> {
        public:
            constexpr ThenableFuture() noexcept : std::future<T>() {}

            inline ThenableFuture( std::future<T> &&f ) noexcept : std::future<T>( std::forward<std::future<T>>( f )) {}

            inline ThenableFuture( ThenableFuture &&f ) noexcept : std::future<T>( std::forward<std::future<T>>( f )) {}

            /*
             * These need to be deleted so that the compiler doesn't just use the defaults
             * */

            ThenableFuture( const ThenableFuture & ) = delete;

            ThenableFuture &operator=( const ThenableFuture & ) = delete;

            /*
             * These operator overloads allow ThenableFuture to be used as a normal future rather easily
             * */
            inline operator std::future<T> &() {
                return *static_cast<std::future<T> *>(this);
            }

            inline operator std::future<T> &&() {
                return std::move( *static_cast<std::future<T> *>(this));
            }

            template <typename... Args>
            inline THENABLE_DECLTYPE_AUTO then( Args &&... args ) {
                //NOTE: if `then` is used instead of `then2`, the namespace should be explicitly given, or this function will recurse
                return then2( std::move( *this ), std::forward<Args>( args )... );
            }

            inline ThenableSharedFuture<T> share_thenable() {
                return ThenableSharedFuture<T>( this->share());
            }
    };

    template <typename T>
    class ThenableSharedFuture : public std::shared_future<T> {
        public:
            constexpr ThenableSharedFuture() noexcept : std::shared_future<T>() {}

            inline ThenableSharedFuture( const std::shared_future<T> &f ) noexcept : std::shared_future<T>( f ) {}

            inline ThenableSharedFuture( const ThenableSharedFuture &f ) noexcept : std::shared_future<T>( f ) {}

            inline ThenableSharedFuture( std::future<T> &&f ) noexcept : std::shared_future<T>( std::forward<std::shared_future<T>>( f )) {}

            inline ThenableSharedFuture( ThenableFuture<T> &&f ) noexcept : std::shared_future<T>( std::forward<std::shared_future<T>>( f )) {}

            inline ThenableSharedFuture( std::shared_future<T> &&f ) noexcept : std::shared_future<T>( std::forward<std::shared_future<T>>( f )) {}

            inline ThenableSharedFuture( ThenableSharedFuture &&f ) noexcept : std::shared_future<T>( std::forward<std::shared_future<T>>( f )) {}

            /*
             * Assignment operator doesn't need to be overloaded here because it'll just cast *this to shared_future<T>
             * and use shared_future<T>'s assignment operator
             * */

            /*
             * These operator overloads allow ThenableSharedFuture to be used as a normal shared_future rather easily
             * */
            inline operator std::shared_future<T> &() {
                return *static_cast<std::shared_future<T> *>(this);
            }

            inline operator std::shared_future<T> &&() {
                return std::move( *static_cast<std::shared_future<T> *>(this));
            }

            template <typename... Args>
            inline THENABLE_DECLTYPE_AUTO then( Args &&... args ) {
                //NOTE: if `then` is used instead of `then2`, the namespace should be explicitly given, or this function will recurse
                return then2( *this, std::forward<Args>( args )... );
            }
    };

    //////////

    namespace detail {
        /*
         * recursive_get implementations for ThenableX classes
         * */

        template <typename T>
        inline THENABLE_DECLTYPE_AUTO recursive_get( ThenableFuture<T> &&t ) {
            return recursive_get( t.get());
        };

        template <typename T>
        inline THENABLE_DECLTYPE_AUTO recursive_get( ThenableSharedFuture<T> &&t ) {
            return recursive_get( t.get());
        };

        template <>
        inline THENABLE_DECLTYPE_AUTO recursive_get<void>( ThenableFuture<void> &&t ) {
            t.get();
        };

        template <>
        inline THENABLE_DECLTYPE_AUTO recursive_get<void>( ThenableSharedFuture<void> &&t ) {
            t.get();
        };

        template <typename T>
        inline THENABLE_DECLTYPE_AUTO recursive_get( ThenablePromise<T> &&t ) {
            return recursive_get( t.get_future());
        };
    }

    //////////

    /*
     * then2 is a variation of then that returns a ThenableFuture instead of a normal future
     * */
    template <typename... Args>
    inline THENABLE_DECLTYPE_AUTO then2( Args &&... args ) {
        auto &&k = then( std::forward<Args>( args )... );

        typedef typename std::remove_reference<decltype( k )>::type K;

        return ThenableFuture<typename detail::get_future_type<K>::type>( std::forward<K>( k ));
    }

    //////////

    /*
     * These just convert future, shared_future and promise to their Thenable equivalent
     * */

    template <typename T>
    inline ThenableFuture<T> to_thenable( std::future<T> &&t ) {
        return ThenableFuture<T>( std::forward<std::future<T>>( t ));
    }

    template <typename T>
    inline ThenableSharedFuture<T> to_thenable( std::shared_future<T> t ) {
        return ThenableSharedFuture<T>( t );
    }

    template <typename T>
    inline ThenablePromise<T> to_thenable( std::promise<T> &&t ) {
        return ThenablePromise<T>( std::forward<std::promise<T>>( t ));
    }

    //////////

    namespace detail {
        template <typename T>
        struct promisify_helper {
            template <typename Functor>
            inline static void dispatch( Functor f, const std::shared_ptr<std::promise<T>> &p ) {
                try {
                    f( [p]( const T &resolved_value ) {
                        p->set_value( resolved_value );

                    }, [p]( auto rejected_value ) {
                        p->set_exception( std::make_exception_ptr( rejected_value ));
                    } );

                } catch( ... ) {
                    p->set_exception( std::current_exception());
                }
            }
        };

        template <>
        struct promisify_helper<void> {
            template <typename Functor>
            inline static void dispatch( Functor f, const std::shared_ptr<std::promise<void>> &p ) {
                try {
                    f( [p] {
                        p->set_value();

                    }, [p]( auto rejected_value ) {
                        p->set_exception( std::make_exception_ptr( rejected_value ));
                    } );

                } catch( ... ) {
                    p->set_exception( std::current_exception());
                }
            }
        };
    }

    template <typename T = void, typename Functor, typename... Args>
    inline THENABLE_DECLTYPE_AUTO promisify( Functor f, Args... args ) {
        auto p = std::make_shared<std::promise<T>>();

        return then( std::async( std::launch::deferred, [f, p]() noexcept {
            detail::promisify_helper<T>::dispatch( f, p );

        }, std::forward<Args>( args )... ), [p] {
            return p->get_future();
        } );
    }

    template <typename T = void, typename Functor, typename... Args>
    inline THENABLE_DECLTYPE_AUTO promisify2( Functor f, Args... args ) {
        auto p = std::make_shared<std::promise<T>>();

        return then2( std::async( std::launch::deferred, [f, p]() noexcept {
            detail::promisify_helper<T>::dispatch( f, p );

        }, std::forward<Args>( args )... ), [p] {
            return p->get_future();
        } );
    }

    //////////

    namespace std_future {
        template <typename T>
        using promise = ThenablePromise<T>;

        template <typename T>
        using future = ThenableFuture<T>;

        template <typename T>
        using shared_future = ThenableSharedFuture<T>;
    }
}

#endif //THENABLE_IMPLEMENTATION_HPP
