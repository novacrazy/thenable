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
    }
}


#endif //THENABLE_EXPERIMENTAL_HPP_INCLUDED
