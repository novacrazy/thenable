//
// Created by Aaron on 9/21/2016.
//

#ifndef THENABLE_PARALLEL_HPP_INCLUDED
#define THENABLE_PARALLEL_HPP_INCLUDED

#include <thenable/future.hpp>
#include <thenable/await.hpp>

#include <cassert>

namespace thenable {
    namespace detail {
        template <typename... Functors>
        using promise_tuple = std::tuple<std::promise<result_of_cb_t<Functors>>...>;

        template <typename... Functors>
        using result_tuple = std::tuple<std::future<result_of_cb_t<Functors>>...>;

        //////////

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

        //////////

        template <typename Functor, typename ResultType = result_of_cb_t<Functor>>
        struct tagged_functor {
            std::atomic_bool      ran;
            std::decay_t<Functor> fn;

            inline tagged_functor( Functor &&_fn ) : ran( false ), fn( std::forward<Functor>( _fn )) {}

            tagged_functor() = delete;

            tagged_functor( const tagged_functor & ) = delete;

            tagged_functor( tagged_functor && ) = delete;

            tagged_functor &operator=( const tagged_functor & ) = delete;

            tagged_functor &operator=( tagged_functor && ) = delete;

            inline void invoke( std::promise<ResultType> &p ) THENABLE_NOEXCEPT {
                bool expect_ran = false;

                ran.compare_exchange_strong( expect_ran, true );

                if( !expect_ran ) {
                    try {
                        p.set_value( recursive_get( fn()));

                    } catch( ... ) {
                        p.set_exception( std::current_exception());
                    }
                }
            }
        };

        template <typename Functor>
        struct tagged_functor<Functor, void> {
            std::atomic_bool      ran;
            std::decay_t<Functor> fn;

            inline tagged_functor( Functor &&_fn ) : ran( false ), fn( std::forward<Functor>( _fn )) {}

            tagged_functor() = delete;

            tagged_functor( const tagged_functor & ) = delete;

            tagged_functor( tagged_functor && ) = delete;

            tagged_functor &operator=( const tagged_functor & ) = delete;

            tagged_functor &operator=( tagged_functor && ) = delete;

            inline void invoke( std::promise<void> &p ) THENABLE_NOEXCEPT {
                bool expect_ran = false;

                ran.compare_exchange_strong( expect_ran, true );

                if( !expect_ran ) {
                    try {
                        fn();

                        p.set_value();

                    } catch( ... ) {
                        p.set_exception( std::current_exception());
                    }
                }
            }
        };

        template <typename... Functors>
        using tagged_functors = std::tuple<tagged_functor<Functors>...>;

        //////////

        template <size_t i, typename... Functors>
        constexpr std::enable_if_t<i == sizeof...( Functors )>
        invoke_parallel_functors( promise_tuple<Functors...> &, tagged_functors<Functors...> & ) THENABLE_NOEXCEPT {
            //No-op
        };

        template <size_t i, typename... Functors>
        inline std::enable_if_t<i < sizeof...( Functors )>
        invoke_parallel_functors( promise_tuple<Functors...> &promises, tagged_functors<Functors...> &fns ) THENABLE_NOEXCEPT {
            std::get<i>( fns ).invoke( std::get<i>( promises ));

            invoke_parallel_functors<i + 1, Functors...>( promises, fns );
        };
    }

    template <typename... Futures>
    struct parallel_result {
        std::tuple<Futures...> futures;

        parallel_result() = delete;

        parallel_result( const parallel_result & ) = delete;

        parallel_result &operator=( const parallel_result & ) = delete;

        parallel_result( std::tuple<Futures...> &&fts ) : futures( std::forward<std::tuple<Futures...>>( fts )) {}

        parallel_result( parallel_result &&p ) : futures( std::move( p.futures )) {}

        parallel_result &operator=( parallel_result &&p ) {
            this->futures = std::move( p.futures );
            return *this;
        }

        std::future<std::tuple<detail::get_future_type<Futures>...>> await_all() {
            return ::thenable::await_all( std::move( this->futures ));
        }

        //TODO: await_any
    };

    template <typename... Futures>
    constexpr parallel_result<thenable_equivalent_t<Futures>...> to_thenable( parallel_result<Futures...> &&result ) {
        return parallel_result<thenable_equivalent_t<Futures>...>( std::move( result.futures ));
    }

    template <typename... Functors>
    parallel_result<std::future<result_of_cb_t<Functors>>...> parallel_n( size_t concurrency, Functors &&... fns ) {
        static_assert( sizeof...( Functors ) > 0 );
        assert( concurrency > 0 );

        typedef detail::promise_tuple<Functors...>   promise_tuple;
        typedef detail::result_tuple<Functors...>    result_tuple;
        typedef detail::tagged_functors<Functors...> tagged_functors;

        result_tuple result;

        auto p = std::make_shared<promise_tuple>();
        auto f = std::make_shared<tagged_functors>( std::forward<Functors>( fns )... );

        detail::initialize_parallel_futures<0, Functors...>( result, *p );

        for( size_t i = 0, min_concurrency = std::min( concurrency, sizeof...( Functors )); i < min_concurrency; ++i ) {
            std::thread( [p, f]() THENABLE_NOEXCEPT {
                detail::invoke_parallel_functors<0, Functors...>( *p, *f );
            } ).detach();
        }

        return parallel_result<std::future<result_of_cb_t<Functors>>...>( std::move( result ));
    }

    template <typename... Functors>
    inline parallel_result<std::future<result_of_cb_t<Functors>>...> parallel( Functors &&... fns ) {
        return parallel_n( std::thread::hardware_concurrency(), std::forward<Functors>( fns )... );
    }

    template <typename... Functors>
    inline parallel_result<ThenableFuture<result_of_cb_t<Functors>>...> parallel2( Functors &&... fns ) {
        return to_thenable( parallel( std::forward<Functors>( fns )... ));
    }

    template <typename... Functors>
    inline parallel_result<ThenableFuture<result_of_cb_t<Functors>>...> parallel2_n( size_t concurrency, Functors &&... fns ) {
        return to_thenable( parallel_n( concurrency, std::forward<Functors>( fns )... ));
    }
}

#endif //THENABLE_PARALLEL_HPP_INCLUDED
