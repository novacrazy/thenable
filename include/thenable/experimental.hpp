//
// Created by Aaron on 8/21/2016.
//

#ifndef THENABLE_EXPERIMENTAL_HPP_INCLUDED
#define THENABLE_EXPERIMENTAL_HPP_INCLUDED

#include <thenable/thenable.hpp>

#include <atomic>

namespace thenable {
    namespace experimental {
        template <typename... Functors>
        THENABLE_DECLTYPE_AUTO_HINTED( std::future ) waterfall( Functors &&..., std::launch = default_policy );

        template <typename... Functors>
        THENABLE_DECLTYPE_AUTO_HINTED( std::future ) waterfall( Functors &&..., then_launch );

        template <typename... Functors>
        THENABLE_DECLTYPE_AUTO_HINTED( ThenableFuture ) waterfall2( Functors &&..., std::launch = default_policy );

        template <typename... Functors>
        THENABLE_DECLTYPE_AUTO_HINTED( ThenableFuture ) waterfall2( Functors &&..., then_launch );

        //////////

        namespace detail {
            using namespace ::thenable::detail;

            template <typename... Functors>
            using promise_tuple = std::tuple<std::promise<fn_traits::fn_result_of<Functors>>...>;

            template <typename... Functors>
            using result_tuple = std::tuple<std::future<fn_traits::fn_result_of<Functors>>...>;

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

                inline void invoke( std::promise<fn_traits::fn_result_of<Functor>> &p ) noexcept {
                    try {
                        p.set_value( f());

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

                inline void invoke( std::promise<void> &p ) noexcept {
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
            invoke_parallel_functors( promise_tuple<Functors...> &, std::tuple<tagged_functor<Functors>...> & ) noexcept {
                //No-op
            };

            template <size_t i, typename... Functors>
            inline typename std::enable_if<i < sizeof...( Functors )>::type
            invoke_parallel_functors( promise_tuple<Functors...> &promises, std::tuple<tagged_functor<Functors>...> &fns ) noexcept {
                bool expect_ran = false;

                auto &func = std::get<i>( fns );

                func.ran.compare_exchange_strong( expect_ran, true );

                if( !expect_ran ) {
                    func.invoke( std::get<i>( promises ));
                }

                invoke_parallel_functors<i + 1, Functors...>( promises, fns );
            };
        }

        template <typename... Functors>
        std::tuple<std::future<fn_traits::fn_result_of<Functors>>...> parallel_n( size_t concurrency, Functors &&... fns ) {
            static_assert( sizeof...( Functors ) > 0 );
            assert( concurrency > 0 );

            typedef detail::promise_tuple<Functors...>              promise_tuple;
            typedef detail::result_tuple<Functors...>               result_tuple;
            typedef std::tuple<detail::tagged_functor<Functors>...> tagged_functors;

            result_tuple result;

            auto p = std::make_shared<promise_tuple>();
            auto f = std::make_shared<tagged_functors>( std::forward<Functors>( fns )... );

            detail::initialize_parallel_futures<0, Functors...>( result, *p );

            for( size_t i = 0; i < concurrency; ++i ) {
                std::thread( [p, f]() noexcept {
                    detail::invoke_parallel_functors<0, Functors...>( *p, *f );
                } ).detach();
            }

            return std::move( result );
        }

        template <typename... Functors>
        inline std::tuple<std::future<fn_traits::fn_result_of<Functors>>...> parallel( Functors &&... fns ) {
            return parallel_n( std::thread::hardware_concurrency(), std::forward<Functors>( fns )... );
        }

        template <typename... Functors>
        inline std::tuple<::thenable::ThenableFuture<fn_traits::fn_result_of<Functors>>...> parallel2_n( size_t concurrency, Functors &&... fns ) {
            //Implicit conversion to ThenableFuture
            return parallel_n( concurrency, std::forward<Functors>( fns )... );
        }

        template <typename... Functors>
        inline std::tuple<::thenable::ThenableFuture<fn_traits::fn_result_of<Functors>>...> parallel2( Functors &&... fns ) {
            //Implicit conversion to ThenableFuture
            return parallel( std::forward<Functors>( fns )... );
        }

        //////////

        template <typename... Results>
        inline std::future<std::tuple<Results...>> await_all( std::tuple<std::future<Results>...> &&results ) {

        }

        template <typename... Results>
        inline std::future<std::tuple<Results...>> await_all( std::tuple<std::shared_future<Results>...> &&results ) {

        }

        template <typename... Results>
        inline std::future<std::tuple<Results...>> await_all( std::tuple<::thenable::ThenableFuture<Results>...> &&results ) {

        }

        template <typename... Results>
        inline std::future<std::tuple<Results...>> await_all( std::tuple<::thenable::ThenableSharedFuture<Results>...> &&results ) {

        }
    }
}


#endif //THENABLE_EXPERIMENTAL_HPP_INCLUDED
