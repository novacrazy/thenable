//
// Created by Aaron on 8/17/2016.
//

#ifndef THENABLE_IMPLEMENTATION_HPP_INCLUDED
#define THENABLE_IMPLEMENTATION_HPP_INCLUDED

#include <thenable/type_traits.hpp>

#include <future>
#include <tuple>
#include <memory>
#include <thread>
#include <chrono>

#define THENABLE_NOEXCEPT noexcept

namespace thenable {
    constexpr std::launch default_policy = std::launch::deferred | std::launch::async;

    enum class then_launch {
            detached = 4
    };

    //////////

    template <typename Functor, typename... Args>
    inline std::future<typename std::result_of<Functor( Args... )>::type> defer( Functor &&fn, Args &&...args ) {
        return std::async( std::launch::deferred, std::forward<Functor>( fn ), std::forward<Args>( args )... );
    };

    //////////

    namespace detail {
        template <template <typename> typename FutureType, typename T>
        typename recursive_get_future_type<T>::type recursive_get( FutureType<T> && );

        template <template <typename> typename FutureType>
        void recursive_get( FutureType<void> && );

        template <typename T>
        constexpr T recursive_get( T && ) THENABLE_NOEXCEPT;


        template <template <typename> typename FutureType, typename T>
        inline typename recursive_get_future_type<T>::type recursive_get( FutureType<T> &&f ) {
            static_assert( is_future<FutureType<T>>::value );

            return recursive_get( f.get());
        };

        template <template <typename> typename FutureType>
        inline void recursive_get( FutureType<void> &&f ) {
            static_assert( is_future<FutureType<void>>::value );

            f.get();
        };

        template <typename T>
        constexpr T recursive_get( T &&t ) THENABLE_NOEXCEPT {
            return std::forward<T>( t );
        };

        //////////

        template <typename Functor, typename ResultType>
        struct then_invoke_helper {
            /*
             * If you're seeing any of the code below, it's probably because a callback given to `then` doesn't
             * have the same argument types as the value being passed from the previous future.
             * */

            template <typename... Args>
            inline static ResultType invoke( Functor &&fn, Args &&...args ) {
                return recursive_get( fn( std::forward<Args>( args )... ));
            }

            template <typename... Args>
            inline static ResultType invoke( Functor &&fn, std::tuple<Args...> &&args ) {
                return recursive_get( apply( std::forward<Functor>( fn ), std::forward<std::tuple<Args...>>( args )));
            }
        };

        template <typename Functor>
        struct then_invoke_helper<Functor, void> {
            /*
             * If you're seeing any of the code below, it's probably because a callback given to `then` doesn't
             * have the same argument types as the value being passed from the previous future.
             * */

            template <typename... Args>
            inline static void invoke( Functor &&fn, Args &&...args ) {
                fn( std::forward<Args>( args )... );
            }

            template <typename... Args>
            inline static void invoke( Functor &&fn, std::tuple<Args...> &&args ) {
                apply( std::forward<Functor>( fn ), std::forward<std::tuple<Args...>>( args ));
            }
        };

        template <typename Functor, typename T>
        struct then_helper {
            typedef typename result_of_cb<Functor, T>::type ResultType;

            template <template <typename> typename FutureType>
            inline static ResultType dispatch( FutureType<T> &&fut, Functor &&fn ) {
                return then_invoke_helper<Functor, ResultType>::invoke( std::forward<Functor>( fn ),
                                                                        recursive_get( std::forward<FutureType<T>>( fut )));
            }
        };

        template <typename Functor>
        struct then_helper<Functor, void> {
            typedef typename result_of_cb<Functor, void>::type ResultType;

            template <template <typename> typename FutureType>
            inline static ResultType dispatch( FutureType<void> &&fut, Functor &&fn ) {
                fut.get();

                return then_invoke_helper<Functor, ResultType>::invoke( std::forward<Functor>( fn ));
            }
        };

        template <typename T, typename Functor, template <typename> typename FutureType>
        inline result_of_cb_t<Functor, T> deferred_then_dispatch( FutureType<T> &&fut, Functor &&fn ) {
            return then_helper<Functor, T>::dispatch( std::forward<FutureType<T>>( fut ),
                                                      std::forward<Functor>( fn ));
        };
    }

    template <typename T, typename Functor, template <typename> typename FutureType>
    inline std::future<result_of_cb_t<Functor, T>> then( FutureType<T> &&fut, Functor &&fn, std::launch policy = default_policy ) {
        static_assert( is_future_v<FutureType<T>> );

        return std::async( policy, detail::deferred_then_dispatch<T, Functor, FutureType>,
                           std::forward<FutureType<T>>( fut ), std::forward<Functor>( fn ));
    };

    template <typename T, typename Functor, template <typename> typename FutureType>
    inline std::future<result_of_cb_t<Functor, T>> then( FutureType<T> &fut, Functor &&fn, std::launch policy = default_policy ) {
        return then( std::move( fut ), std::forward<Functor>( fn ), policy );
    };

    template <typename T, typename Functor>
    inline std::future<result_of_cb_t<Functor, T>> then( std::promise<T> &p, Functor &&fn, std::launch policy = default_policy ) {
        return then( p.get_future(), std::forward<Functor>( fn ), policy );
    };

    namespace detail {
        template <typename T>
        struct detached_then_helper {
            template <typename Functor, typename K, template <typename> typename FutureType>
            static inline void dispatch( std::promise<T> &p, FutureType<K> &&fut, Functor &&fn ) THENABLE_NOEXCEPT {
                try {
                    p.set_value( then_helper<Functor, K>::dispatch( std::forward<FutureType<K>>( fut ),
                                                                    std::forward<Functor>( fn )));
                } catch( ... ) {
                    p.set_exception( std::current_exception());
                }
            }
        };

        template <>
        struct detached_then_helper<void> {
            template <typename Functor, typename K, template <typename> typename FutureType>
            static inline void dispatch( std::promise<void> &p, FutureType<K> &&fut, Functor &&fn ) THENABLE_NOEXCEPT {
                try {
                    then_helper<Functor, K>::dispatch( std::forward<FutureType<K>>( fut ),
                                                       std::forward<Functor>( fn ));

                    p.set_value();

                } catch( ... ) {
                    p.set_exception( std::current_exception());
                }
            }
        };
    }

    template <typename T, typename Functor, template <typename> typename FutureType>
    std::future<result_of_cb_t<Functor, T>> then( FutureType<T> &&fut, Functor &&fn, then_launch policy ) {
        static_assert( is_future_v<FutureType<T>> );

        typedef typename result_of_cb<Functor, T>::type ResultType;

        //Because my IDE is silly sometimes, I need to point out to it that FutureType<T> exists on the inner scope below
        typedef FutureType<T> FutureTypeT;

        auto p = std::make_shared<std::promise<ResultType>>();

        //Use lambda to capture the shared_ptr by value
        std::thread( [p]( FutureType<T> &&fut2, Functor &&fn2 ) {
            detail::detached_then_helper<ResultType>::dispatch( *p,
                                                                std::forward<FutureTypeT>( fut2 ),
                                                                std::forward<Functor>( fn2 ));

        }, std::forward<FutureType<T>>( fut ), std::forward<Functor>( fn )).detach();

        return p->get_future();
    };

    template <typename T, typename Functor, template <typename> typename FutureType>
    inline std::future<result_of_cb_t<Functor, T>> then( FutureType<T> &fut, Functor &&fn, then_launch policy ) {
        return then( std::move( fut ), std::forward<Functor>( fn ), policy );
    };

    namespace detail {
        template <typename T>
        struct make_promise_helper {
            template <typename Functor>
            inline static void dispatch( Functor &&fn, const std::shared_ptr<std::promise<T>> &p ) THENABLE_NOEXCEPT {
                try {
                    fn( [p]( const T &resolved_value ) THENABLE_NOEXCEPT {
                        p->set_value( std::move( resolved_value ));

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
            inline static void dispatch( Functor &&fn, const std::shared_ptr<std::promise<void>> &p ) THENABLE_NOEXCEPT {
                try {
                    fn( [p]() THENABLE_NOEXCEPT {
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

    template <typename T = void, typename Functor, typename PolicyType = std::launch>
    std::future<T> make_promise( Functor &&fn, PolicyType policy = default_policy ) {
        auto p = std::make_shared<std::promise<T>>();

        return then( defer( [p]( Functor &&fn2 ) THENABLE_NOEXCEPT {
            detail::make_promise_helper<T>::dispatch( std::forward<Functor>( fn2 ), p );

        }, std::forward<Functor>( fn )), [p] {
            return p->get_future();

        }, policy );
    }

    template <typename T, typename _Rep, typename _Period>
    std::future<std::decay_t<T>> timeout( std::chrono::duration<_Rep, _Period> duration, T &&value ) {
        auto p = std::make_shared<std::promise<std::decay_t<T>>>();

        std::thread( [p, duration]( std::decay_t<T> &&value2 ) THENABLE_NOEXCEPT {
            std::this_thread::sleep_for( duration );

            p->set_value( value2 );
        }, std::forward<std::decay_t<T>>( value )).detach();

        return p->get_future();
    };

    template <typename _Rep, typename _Period>
    std::future<void> timeout( std::chrono::duration<_Rep, _Period> duration ) {
        auto p = std::make_shared<std::promise<void>>();

        std::thread( [p, duration]() THENABLE_NOEXCEPT {
            std::this_thread::sleep_for( duration );

            p->set_value();
        } ).detach();

        return p->get_future();
    };
}

#endif //THENABLE_IMPLEMENTATION_HPP_INCLUDED
