//
// Created by Aaron on 8/21/2016.
//

#ifndef THENABLE_EXPERIMENTAL_HPP_INCLUDED
#define THENABLE_EXPERIMENTAL_HPP_INCLUDED

#include <thenable/thenable.hpp>

#include <atomic>
#include <algorithm>
#include <utility>
#include <initializer_list>

#if defined(THENABLE_USE_BOOST_VARIANT)

#include <boost/variant.hpp>
//#elif defined(THENABLE_USE_STD_VARIANT)
//#include <variant.hpp>
#endif

namespace thenable {
    namespace experimental {
        namespace detail {
            using namespace ::thenable::detail;
        }

#ifdef THENABLE_USE_BOOST_VARIANT

        namespace detail {
            template <typename... Results>
            using await_any_state = std::pair<std::atomic_bool, std::promise<boost::variant<Results...>>>;

            template <typename... Results>
            constexpr void await_any_start( std::shared_ptr<await_any_state<Results...>> &p ) {
                //No-op
            }

            template <template <typename> typename HeadFuture, typename... Results, typename Head, typename... Rest>
            void await_any_start( std::shared_ptr<await_any_state<Results...>> &p, HeadFuture<Head> &&head, Rest &&... rest ) {
                std::thread( [p]( HeadFuture<Head> &&head2 ) {
                    bool expect_resolved = false;

                    try {
                        Head &&value = head2.get();

                        p->first.compare_exchange_strong( expect_resolved, true );

                        if( !expect_resolved ) {
                            p->second.set_value( std::forward<Head>( value ));
                        }

                    } catch( ... ) {
                        p->first.compare_exchange_strong( expect_resolved, true );

                        if( !expect_resolved ) {
                            p->second.set_exception( std::current_exception());
                        }
                    }
                }, std::forward<HeadFuture<Head>>( head )).detach();

                await_any_start( p, std::forward<Rest>( rest )... );
            };

            template <template <typename> typename HeadFuture, typename... Results, typename... Rest>
            void await_any_start( std::shared_ptr<await_any_state<Results...>> &p, HeadFuture<void> &&head, Rest &&... rest ) {
                std::thread( [p]( HeadFuture<void> &&head2 ) {
                    bool expect_resolved = false;

                    try {
                        head2.get();

                        p->first.compare_exchange_strong( expect_resolved, true );

                        if( !expect_resolved ) {
                            p->second.set_value( boost::variant<Results...>());
                        }

                    } catch( ... ) {
                        p->first.compare_exchange_strong( expect_resolved, true );

                        if( !expect_resolved ) {
                            p->second.set_exception( std::current_exception());
                        }
                    }
                }, std::forward<HeadFuture<void>>( head )).detach();

                await_any_start( p, std::forward<Rest>( rest )... );
            };
        }

        template <template <typename> typename... FutureTypes, typename... Results>
        std::future<boost::variant<Results...>> await_any( FutureTypes<Results> &&...futures ) {
            auto p = std::make_shared<detail::await_any_state<Results...>>();

            detail::await_any_start( p, std::forward<FutureTypes<Results>>( futures )... );

            return p->second.get_future();
        }

        template <template <typename> typename... FutureTypes, typename... Results>
        std::future<boost::variant<Results...>> await_any( FutureTypes<Results> &...futures ) {
            return await_any( std::move( futures )... );
        }

#endif
    }
}


#endif //THENABLE_EXPERIMENTAL_HPP_INCLUDED
