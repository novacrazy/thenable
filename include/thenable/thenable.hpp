//
// Created by Aaron on 8/17/2016.
//

#ifndef THENABLE_IMPLEMENTATION_HPP
#define THENABLE_IMPLEMENTATION_HPP

#include <thenable/fwd.hpp>

#include <function_traits.hpp>
#include <assert.h>
#include <memory>
#include <thread>

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
    //////////

    template <typename Functor, typename... Args>
    inline THENABLE_DECLTYPE_AUTO_HINTED( std::future ) defer( Functor &&f, Args &&... args ) {
        return std::async( std::launch::deferred, std::forward<Functor>( f ), std::forward<Args>( args )... );
    };

    template <typename Functor, typename... Args>
    inline THENABLE_DECLTYPE_AUTO_HINTED( ThenableFuture ) defer2( Functor &&f, Args &&... args ) {
        return to_thenable( defer( std::forward<Functor>( f ), std::forward<Args>( args )... ));
    };

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
        constexpr THENABLE_DECLTYPE_AUTO recursive_get( T && ) noexcept;

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
        constexpr THENABLE_DECLTYPE_AUTO recursive_get( T &&t ) noexcept {
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
            inline static THENABLE_DECLTYPE_AUTO invoke( Functor &&f, Args &&... args ) {
                /*
                 * If you're seeing this line, it's probably because a callback given to `then` doesn't
                 * have the same argument types as the value being passed from the previous future.
                 * */
                return recursive_get( f( std::forward<Args>( args )... ));
            }
        };

        /*
         * Specialization of then_invoke_helper for void callbacks.
         * */
        template <typename Functor>
        struct then_invoke_helper<Functor, void> {
            template <typename... Args>
            inline static void invoke( Functor &&f, Args &&... args ) {
                /*
                 * If you're seeing this line, it's probably because a callback given to `then` doesn't
                 * have the same argument types as the value being passed from the previous future.
                 * */
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
            inline static THENABLE_DECLTYPE_AUTO dispatch( std::future<T> &&s, Functor &&f ) {
                return then_invoke_helper<Functor>::invoke( std::forward<Functor>( f ), recursive_get( std::forward<std::future<T>>( s )));
            }

            inline static THENABLE_DECLTYPE_AUTO dispatch( std::shared_future<T> &&s, Functor &&f ) {
                return then_invoke_helper<Functor>::invoke( std::forward<Functor>( f ), recursive_get( std::forward<std::shared_future<T>>( s )));
            }
        };

        template <typename Functor>
        struct then_helper<void, Functor> {
            inline static THENABLE_DECLTYPE_AUTO dispatch( std::future<void> &&s, Functor &&f ) {
                s.get();

                return then_invoke_helper<Functor>::invoke( std::forward<Functor>( f ));
            }

            inline static THENABLE_DECLTYPE_AUTO dispatch( std::shared_future<void> &&s, Functor &&f ) {
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
            static inline void dispatch( std::promise<T> &p, std::future<K> &&s, Functor &&f ) noexcept {
                try {
                    p.set_value( then_helper<K, Functor>::dispatch( std::forward<std::future<K>>( s ), std::forward<Functor>( f )));

                } catch( ... ) {
                    p.set_exception( std::current_exception());
                }
            }

            template <typename Functor, typename K>
            static inline void dispatch( std::promise<T> &p, std::shared_future<K> &&s, Functor &&f ) noexcept {
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
            static inline void dispatch( std::promise<void> &p, std::future<K> &&s, Functor &&f ) noexcept {
                try {
                    then_helper<K, Functor>::dispatch( std::forward<std::future<K>>( s ), std::forward<Functor>( f ));

                    p.set_value();

                } catch( ... ) {
                    p.set_exception( std::current_exception());
                }
            }

            template <typename Functor, typename K>
            static inline void dispatch( std::promise<void> &p, std::shared_future<K> &&s, Functor &&f ) noexcept {
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
            inline static void dispatch( Functor &&f, const std::shared_ptr<std::promise<T>> &p ) noexcept {
                try {
                    f( [p]( const T &resolved_value ) noexcept {
                        p->set_value( resolved_value );

                    }, [p]( auto rejected_value ) noexcept {
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
            inline static void dispatch( Functor &&f, const std::shared_ptr<std::promise<void>> &p ) noexcept {
                try {
                    f( [p]() noexcept {
                        p->set_value();

                    }, [p]( auto rejected_value ) noexcept {
                        p->set_exception( std::make_exception_ptr( rejected_value ));
                    } );

                } catch( ... ) {
                    p->set_exception( std::current_exception());
                }
            }
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
    inline THENABLE_DECLTYPE_AUTO_HINTED( std::future ) then( std::future<T> &&s, Functor &&f, std::launch policy ) {
        return std::async( policy, [policy]( std::future<T> &&s2, Functor &&f2 ) {
            return detail::then_helper<T, Functor>::dispatch( std::forward<std::future<T>>( s2 ), std::forward<Functor>( f2 ));
        }, std::forward<std::future<T>>( s ), std::forward<Functor>( f ));
    };

    /*
     * Overload for futures by references. It will take ownership and forward the future,
     * calling the above overload.
     * */
    template <typename T, typename Functor>
    inline THENABLE_DECLTYPE_AUTO_HINTED( std::future ) then( std::future<T> &s, Functor &&f, std::launch policy ) {
        return then( std::forward<std::future<T>>( s ), std::forward<Functor>( f ), policy );
    };

    /*
     * Overload for shared_futures, passed by value because they can be copied.
     * It's similar to the first overload, but can capture the shared_future in the lambda.
     * */
    template <typename T, typename Functor>
    inline THENABLE_DECLTYPE_AUTO_HINTED( std::future ) then( std::shared_future<T> &&s, Functor &&f, std::launch policy ) {
        return std::async( policy, [policy]( std::shared_future<T> &&s2, Functor &&f2 ) {
            return detail::then_helper<T, Functor>::dispatch( std::forward<std::shared_future<T>>( s2 ), std::forward<Functor>( f2 ));
        }, std::forward<std::shared_future<T>>( s ), std::forward<Functor>( f ));
    };

    template <typename T, typename Functor>
    inline THENABLE_DECLTYPE_AUTO_HINTED( std::future ) then( std::shared_future<T> s, Functor &&f, std::launch policy ) {
        return then( std::move( s ), std::forward<Functor>( f ), policy );
    };

    /*
     * Overload for promise references. The promise is NOT moved, but rather the future is acquired
     * from it and the promise is left intact, so it can be used elsewhere.
     * */
    template <typename T, typename Functor>
    inline THENABLE_DECLTYPE_AUTO_HINTED( std::future ) then( std::promise<T> &s, Functor &&f, std::launch policy ) {
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
    THENABLE_DECLTYPE_AUTO_HINTED( std::future ) then( std::future<T> &&s, Functor &&f, then_launch policy ) {
        //I don't really like having to do this, but I don't feel like rewriting almost all the recursive template logic above
        typedef decltype( detail::then_helper<T, Functor>::dispatch( std::forward<std::future<T>>( s ), std::forward<Functor>( f ))) P;

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
    inline THENABLE_DECLTYPE_AUTO_HINTED( std::future ) then( std::future<T> &s, Functor &&f, then_launch policy ) {
        return then( std::move( s ), std::forward<Functor>( f ), policy );
    };

    /*
     * Overload for shared_futures, passed by value because they can be copied.
     * It's similar to the first overload, but can capture the shared_future in the lambda.
     * */
    template <typename T, typename Functor>
    THENABLE_DECLTYPE_AUTO_HINTED( std::future ) then( std::shared_future<T> &&s, Functor &&f, then_launch policy ) {
        //I don't really like having to do this, but I don't feel like rewriting almost all the recursive template logic above
        typedef decltype( detail::then_helper<T, Functor>::dispatch( std::forward<std::shared_future<T>>( s ), std::forward<Functor>( f ))) P;

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
    inline THENABLE_DECLTYPE_AUTO_HINTED( std::future ) then( std::shared_future<T> s, Functor &&f, then_launch policy ) {
        return then( std::move( s ), std::forward<Functor>( f ), policy );
    };

    /*
     * Overload for promise references. The promise is NOT moved, but rather the future is acquired
     * from it and the promise is left intact, so it can be used elsewhere.
     * */
    template <typename T, typename Functor>
    inline THENABLE_DECLTYPE_AUTO_HINTED( std::future ) then( std::promise<T> &s, Functor &&f, then_launch policy ) {
        return then( s.get_future(), std::forward<Functor>( f ), policy );
    };

    //////////

    /*
     * then2 is a variation of then that returns a ThenableFuture instead of a normal future
     * */
    template <typename FutureType, typename Functor, typename LaunchPolicy>
    inline THENABLE_DECLTYPE_AUTO_HINTED( ThenableFuture ) then2( FutureType &&s, Functor &&f, LaunchPolicy policy ) {
        return to_thenable( then( std::forward<FutureType>( s ), std::forward<Functor>( f ), policy ));
    }

    template <typename FutureType, typename Functor, typename LaunchPolicy>
    inline THENABLE_DECLTYPE_AUTO_HINTED( ThenableFuture ) then2( FutureType &s, Functor &&f, LaunchPolicy policy ) {
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

            constexpr operator std::promise<T> &() noexcept {
                return *static_cast<std::promise<T> *>(this);
            }

            constexpr operator std::promise<T> &&() noexcept {
                return std::move( *static_cast<std::promise<T> *>(this));
            }

            inline ThenableFuture<T> get_thenable_future() {
                return this->get_future(); //Automatically converts it
            }

            template <typename Functor, typename LaunchPolicy = std::launch>
            inline THENABLE_DECLTYPE_AUTO_HINTED( ThenableFuture ) then( Functor &&f, LaunchPolicy policy = default_policy ) {
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
            constexpr ThenableFuture() noexcept : std::future<T>() {}

            inline ThenableFuture( std::future<T> &&f ) noexcept : std::future<T>( std::forward<std::future<T>>( f )) {}

            inline ThenableFuture( ThenableFuture &&f ) noexcept : std::future<T>( std::forward<std::future<T>>( f )) {}

            ThenableFuture( const ThenableFuture & ) = delete;

            ThenableFuture &operator=( const ThenableFuture & ) = delete;

            constexpr operator std::future<T> &() {
                return *static_cast<std::future<T> *>(this);
            }

            constexpr operator std::future<T> &&() {
                return std::move( *static_cast<std::future<T> *>(this));
            }

            template <typename Functor, typename LaunchPolicy = std::launch>
            inline THENABLE_DECLTYPE_AUTO_HINTED( ThenableFuture ) then( Functor &&f, LaunchPolicy policy = default_policy ) {
                return then2( std::move( *this ), std::forward<Functor>( f ), policy );
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

            constexpr operator std::shared_future<T> &() {
                return *static_cast<std::shared_future<T> *>(this);
            }

            constexpr operator std::shared_future<T> &&() {
                return std::move( *static_cast<std::shared_future<T> *>(this));
            }

            template <typename Functor, typename LaunchPolicy = std::launch>
            inline THENABLE_DECLTYPE_AUTO_HINTED( ThenableFuture ) then( Functor &&f, LaunchPolicy policy = default_policy ) {
                return then2( *this, std::forward<Functor>( f ), policy );
            }
    };

    //////////

    namespace detail {
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
     * These just convert future, shared_future and promise to their Thenable equivalent
     * */

    template <typename T>
    inline ThenableFuture<T> to_thenable( std::future<T> &&t ) {
        return ThenableFuture<T>( std::forward<std::future<T>>( t ));
    }

    template <typename T>
    inline ThenableSharedFuture<T> to_thenable( const std::shared_future<T> &t ) {
        return ThenableSharedFuture<T>( t );
    }

    template <typename T>
    inline ThenableSharedFuture<T> to_thenable( std::shared_future<T> &&t ) {
        return ThenableSharedFuture<T>( std::forward<std::shared_future<T>>( t ));
    }

    template <typename T>
    inline ThenablePromise<T> to_thenable( std::promise<T> &&t ) {
        return ThenablePromise<T>( std::forward<std::promise<T>>( t ));
    }

    //////////

    template <typename T, typename Functor, typename LaunchPolicy>
    THENABLE_DECLTYPE_AUTO_HINTED( std::future ) make_promise( Functor &&f, LaunchPolicy policy ) {
        auto p = std::make_shared<std::promise<T>>();

        return then( defer( [p]( Functor &&f2 ) noexcept {
            detail::make_promise_helper<T>::dispatch( std::forward<Functor>( f2 ), p );

        }, std::forward<Functor>( f )), [p] {
            return p->get_future();

        }, policy );
    }

    template <typename T, typename Functor, typename LaunchPolicy>
    THENABLE_DECLTYPE_AUTO_HINTED( ThenableFuture ) make_promise2( Functor &&f, LaunchPolicy policy ) {
        auto p = std::make_shared<std::promise<T>>();

        return then2( defer( [p]( Functor &&f2 ) noexcept {
            detail::make_promise_helper<T>::dispatch( std::forward<Functor>( f2 ), p );

        }, std::forward<Functor>( f )), [p] {
            return p->get_future();

        }, policy );
    }
}

#endif //THENABLE_IMPLEMENTATION_HPP
