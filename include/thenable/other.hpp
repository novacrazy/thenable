//
// Created by Aaron on 9/21/2016.
//

#ifndef THENABLE_OTHER_HPP_INCLUDED
#define THENABLE_OTHER_HPP_INCLUDED

#include <tuple>

namespace thenable {
    namespace detail {
        template <typename Functor, typename... Args, std::size_t... S>
        inline typename std::result_of<Functor(Args...)>::type
        invoke_helper( Functor &&func, std::tuple<Args...> &&args, std::index_sequence<S...> ) {
            return func( std::get<S>( std::forward<std::tuple<Args...>>( args ))... );
        }

        template <typename Functor, typename... Args>
        inline typename std::result_of<Functor(Args...)>::type
        invoke_tuple( Functor &&func, std::tuple<Args...> &&args ) {
            return invoke_helper( std::forward<Functor>( func ),
                                  std::forward<std::tuple<Args...>>( args ),
                                  std::make_index_sequence<sizeof...(Args)>{} );
        }
    }
}

#endif //THENABLE_OTHER_HPP_INCLUDED
