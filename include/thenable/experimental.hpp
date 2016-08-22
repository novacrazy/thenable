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

            for( size_t i = 0, min_concurrency = std::min( concurrency, sizeof...( Functors )); i < min_concurrency; ++i ) {
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

        namespace detail {
            template <typename K, typename T, std::size_t... S>
            inline K get_tuple_futures( T &&t, std::index_sequence<S...> ) {
                return K( recursive_get( std::get<S>( std::forward<T>( t )))... );
            }
        }

        template <typename... Results>
        std::future<std::tuple<Results...>>
        await_all( std::tuple<std::future<Results>...> &&results, std::launch policy = default_policy ) {
            typedef std::tuple<std::future<Results>...> tuple_type;
            constexpr auto                              Size = std::tuple_size<tuple_type>::value;

            return std::async( policy, []( tuple_type &&inner_results ) {
                return detail::get_tuple_futures<std::tuple<Results...>>( std::forward<tuple_type>( inner_results ), std::make_index_sequence<Size>());
            }, std::forward<tuple_type>( results ));
        }

        template <typename... Results>
        std::future<std::tuple<Results...>>
        await_all( std::tuple<std::shared_future<Results>...> &&results, std::launch policy = default_policy ) {
            typedef std::tuple<std::shared_future<Results>...> tuple_type;
            constexpr auto                                     Size = std::tuple_size<tuple_type>::value;

            return std::async( policy, []( tuple_type &&inner_results ) {
                return detail::get_tuple_futures<std::tuple<Results...>>( std::forward<tuple_type>( inner_results ), std::make_index_sequence<Size>());
            }, std::forward<tuple_type>( results ));
        }

        template <typename... Results>
        ::thenable::ThenableFuture<std::tuple<Results...>>
        await_all( std::tuple<::thenable::ThenableFuture<Results>...> &&results, std::launch policy = default_policy ) {
            typedef std::tuple<::thenable::ThenableFuture<Results>...> tuple_type;
            constexpr auto                                             Size = std::tuple_size<tuple_type>::value;

            return std::async( policy, []( tuple_type &&inner_results ) {
                return detail::get_tuple_futures<std::tuple<Results...>>( std::forward<tuple_type>( inner_results ), std::make_index_sequence<Size>());
            }, std::forward<tuple_type>( results ));
        }

        template <typename... Results>
        ::thenable::ThenableFuture<std::tuple<Results...>>
        await_all( std::tuple<::thenable::ThenableSharedFuture<Results>...> &&results, std::launch policy = default_policy ) {
            typedef std::tuple<::thenable::ThenableSharedFuture<Results>...> tuple_type;
            constexpr auto                                                   Size = std::tuple_size<tuple_type>::value;

            return std::async( policy, []( tuple_type &&inner_results ) {
                return detail::get_tuple_futures<std::tuple<Results...>>( std::forward<tuple_type>( inner_results ), std::make_index_sequence<Size>());
            }, std::forward<tuple_type>( results ));
        }

        //////////

        template <typename... Results>
        std::future<std::tuple<Results...>>
        await_all( std::tuple<std::future<Results>...> &&results, then_launch policy ) {
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
        std::future<std::tuple<Results...>>
        await_all( std::tuple<std::shared_future<Results>...> &&results, then_launch policy ) {
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
        ::thenable::ThenableFuture<std::tuple<Results...>>
        await_all( std::tuple<::thenable::ThenableFuture<Results>...> &&results, then_launch policy ) {
            typedef std::tuple<::thenable::ThenableFuture<Results>...> tuple_type;
            constexpr auto                                             Size = std::tuple_size<tuple_type>::value;

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
        ::thenable::ThenableFuture<std::tuple<Results...>>
        await_all( std::tuple<::thenable::ThenableSharedFuture<Results>...> &&results, then_launch policy ) {
            typedef std::tuple<::thenable::ThenableSharedFuture<Results>...> tuple_type;
            constexpr auto                                                   Size = std::tuple_size<tuple_type>::value;

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
    }
}


#endif //THENABLE_EXPERIMENTAL_HPP_INCLUDED
