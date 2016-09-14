//
// Created by Aaron on 8/21/2016.
//

#ifndef THENABLE_EXPERIMENTAL_HPP_INCLUDED
#define THENABLE_EXPERIMENTAL_HPP_INCLUDED

#include <thenable/thenable.hpp>

#include <atomic>
#include <iostream>

namespace thenable {
    namespace experimental {
        namespace detail {
            using namespace ::thenable::detail;

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
        decltype( auto ) reverse_waterfall( std::launch policy, Functor &&f ) {
            return std::async( policy, []( Functor &&f2 ) {
                return detail::then_invoke_helper<Functor>::invoke( std::forward<Functor>( f2 ));
            }, std::forward<Functor>( f ));
        }

        template <typename Functor>
        decltype( auto ) reverse_waterfall( then_launch policy, Functor &&f ) {
            typedef decltype( detail::then_invoke_helper<Functor>::invoke( std::forward<Functor>( f ))) P;

            assert( policy == then_launch::detached );

            auto p = std::make_shared<std::promise<P>>();

            std::thread( [p]( Functor &&f2 ) {
                detail::detached_waterfall_helper<P>::dispatch( *p, std::forward<Functor>( f2 ));
            }, std::forward<Functor>( f )).detach();

            return p->get_future();
        }

        template <typename PolicyType, typename Functor, typename... Functors>
        inline decltype( auto ) reverse_waterfall( PolicyType policy, Functor &&f, Functors &&... fns ) {
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
        inline decltype( auto ) waterfall( std::launch policy, Functors &&... fns ) {
            return detail::forward_waterfall<Functors..., std::launch>::apply( std::forward<Functors>( fns )..., std::forward<std::launch>( policy ));
        }

        template <typename... Functors>
        inline decltype( auto ) waterfall( then_launch policy, Functors &&... fns ) {
            return detail::forward_waterfall<Functors..., then_launch>::apply( std::forward<Functors>( fns )..., std::forward<then_launch>( policy ));
        }

        template <typename... Functors>
        inline decltype( auto ) waterfall( Functors &&... fns ) {
            return waterfall( default_policy, std::forward<Functors>( fns )... );
        }

        template <typename... Args>
        inline decltype( auto ) waterfall2( Args &&... args ) {
            return to_thenable( waterfall( std::forward<Args>( args )... ));
        };
    }
}


#endif //THENABLE_EXPERIMENTAL_HPP_INCLUDED
