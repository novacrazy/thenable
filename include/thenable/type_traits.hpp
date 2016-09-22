//
// Created by Aaron on 9/21/2016.
//

#ifndef THENABLE_TYPE_TRAITS_HPP
#define THENABLE_TYPE_TRAITS_HPP

#include <tuple>
#include <type_traits>
#include <future>

namespace thenable {
    namespace detail {
        template <typename T>
        struct is_future_impl : std::false_type {
        };

        template <typename T>
        struct is_future_impl<std::future<T>> : std::true_type {
        };

        template <typename T>
        struct is_future_impl<std::shared_future<T>> : std::true_type {
        };

        template <typename T>
        struct get_future_type {
            //No type defined
        };

        template <template <typename> typename FutureType, typename T>
        struct get_future_type<FutureType<T>> {
            static_assert( is_future_impl<FutureType<T>>::value );

            typedef T type;
        };

        template <typename T>
        struct recursive_get_future_type {
            typedef T type;
        };

        template <template <typename> typename FutureType, typename T>
        struct recursive_get_future_type<FutureType<T>> {
            static_assert( is_future_impl<FutureType<T>>::value );

            typedef typename recursive_get_future_type<T>::type type;
        };

        template <typename Functor, typename Arg>
        struct result_of_cb_unpacked {
            typedef typename std::result_of<Functor( Arg )>::type type;
        };

        template <typename Functor>
        struct result_of_cb_unpacked<Functor, void> {
            typedef typename std::result_of<Functor()>::type type;
        };

        template <typename Functor, typename... Args>
        struct result_of_cb_unpacked<Functor, std::tuple<Args...>> {
            typedef typename std::result_of<Functor( Args... )>::type type;
        };

        template <typename Functor, typename T>
        struct result_of_cb_impl {
            typedef typename recursive_get_future_type<
                typename result_of_cb_unpacked<
                    std::decay_t<Functor>,
                    typename recursive_get_future_type<T>::type
                >::type
            >::type type;
        };

        struct recursive_sentinel {
        };

        template <typename T, typename Functor, typename... Functors>
        struct recursive_result_of_cb_impl {
            typedef typename recursive_result_of_cb_impl<typename result_of_cb_impl<Functor, T>::type, Functors...>::type type;
        };

        template <typename T>
        struct recursive_result_of_cb_impl<T, recursive_sentinel> {
            typedef T type;
        };
    }

    template <typename T>
    using is_future = detail::is_future_impl<T>;

    template <typename T>
    constexpr bool is_future_v = is_future<T>::value;

    template <typename Functor, typename T>
    struct result_of_cb {
        typedef typename detail::result_of_cb_impl<Functor, T>::type type;
    };

    template <typename Functor, typename T>
    using result_of_cb_t = typename result_of_cb<Functor, T>::type;

    template <typename... Functors>
    struct recursive_result_of_cb {
        typedef typename detail::recursive_result_of_cb_impl<void, Functors..., detail::recursive_sentinel>::type type;
    };

    template <typename... Functors>
    using recursive_result_of_cb_t = typename recursive_result_of_cb<Functors...>::type;
}

#endif //THENABLE_TYPE_TRAITS_HPP
