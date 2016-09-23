//
// Created by Aaron on 9/22/2016.
//

#ifndef THENABLE_FUTURE_HPP_INCLUDED
#define THENABLE_FUTURE_HPP_INCLUDED

#include <thenable/thenable.hpp>

namespace thenable {
    template <typename>
    class ThenableFuture;

    template <typename>
    class ThenableSharedFuture;

    template <typename>
    class ThenablePromise;

    //////////

    namespace detail {
        template <typename T>
        struct is_future_impl<ThenableFuture<T>> : std::true_type {
        };

        template <typename T>
        struct is_future_impl<ThenableSharedFuture<T>> : std::true_type {
        };

        template <typename FutureType>
        struct thenable_equivalent_impl {
        };

        template <typename T>
        struct thenable_equivalent_impl<std::future<T>> {
            typedef ThenableFuture<T> type;
        };

        template <typename T>
        struct thenable_equivalent_impl<std::shared_future<T>> {
            typedef ThenableSharedFuture<T> type;
        };

        template <typename T>
        struct thenable_equivalent_impl<std::promise<T>> {
            typedef ThenablePromise<T> type;
        };

        template <typename T>
        struct thenable_equivalent_impl<ThenableFuture<T>>
            : public thenable_equivalent_impl<std::future<T>> {
        };

        template <typename T>
        struct thenable_equivalent_impl<ThenableSharedFuture<T>>
            : public thenable_equivalent_impl<std::shared_future<T>> {
        };

        template <typename T>
        struct thenable_equivalent_impl<ThenablePromise<T>>
            : public thenable_equivalent_impl<std::promise<T>> {
        };
    }

    template <typename FutureType>
    struct thenable_equivalent {
        typedef typename detail::thenable_equivalent_impl<FutureType>::type type;
    };

    template <typename FutureType>
    using thenable_equivalent_t = typename thenable_equivalent<FutureType>::type;

    //////////

    template <typename T, template <typename> typename FutureType>
    constexpr thenable_equivalent_t<FutureType<T>> to_thenable( FutureType<T> && );

    template <typename T, template <typename> typename FutureType>
    constexpr thenable_equivalent_t<FutureType<T>> to_thenable( FutureType<T> & );

    /*
     * The first two parameters of the following must be specified so as to not conflict with the above overloads.
     * */

    template <typename T0,
              typename T1,
              typename... Ts,
        template <typename> typename FutureType0,
        template <typename> typename FutureType1,
        template <typename> typename... FutureTypes>
    constexpr std::tuple<thenable_equivalent_t<FutureType0<T0>>,
                         thenable_equivalent_t<FutureType1<T1>>,
                         thenable_equivalent_t<FutureTypes<Ts>>...>
    to_thenable( FutureType0<T0> &&fut0,
                 FutureType1<T1> &&fut1,
                 FutureTypes<Ts> &&... futures );

    template <typename T0,
              typename T1,
              typename... Ts,
        template <typename> typename FutureType0,
        template <typename> typename FutureType1,
        template <typename> typename... FutureTypes>
    constexpr std::tuple<thenable_equivalent_t<FutureType0<T0>>,
                         thenable_equivalent_t<FutureType1<T1>>,
                         thenable_equivalent_t<FutureTypes<Ts>>...>
    to_thenable( FutureType0<T0> &fut0,
                 FutureType1<T1> &fut1,
                 FutureTypes<Ts> &... futures );

    /*
     * Even though the compiler is smart enough to usually select the correct one, my IDE can't and I feel that means having
     * the following also be named to_thenable would lead to real ambiguities someplace. For example,
     * FutureType<T> could be std::tuple<int> and it would be ambiguous.
     *
     * So to solve that and clarify things, I'm just naming these to_thenable_tuple to differentiate them.
     * */

    template <typename... Ts, template <typename> typename... FutureTypes>
    constexpr std::tuple<thenable_equivalent_t<FutureTypes<Ts>>...> to_thenable_tuple( std::tuple<FutureTypes<Ts>...> &&futures );

    template <typename... Ts, template <typename> typename... FutureTypes>
    constexpr std::tuple<thenable_equivalent_t<FutureTypes<Ts>>...> to_thenable_tuple( std::tuple<FutureTypes<Ts>...> &futures );

    //////////

    /*
     * then2 is a variation of then that returns a ThenableFuture instead of std::future
     * */

    template <typename T, typename Functor, template <typename> typename FutureType, typename PolicyType = std::launch>
    inline ThenableFuture<result_of_cb_t<Functor, T>> then2( FutureType<T> &&fut, Functor &&fn, PolicyType policy = default_policy ) {
        return to_thenable( then( std::forward<FutureType<T>>( fut ), std::forward<Functor>( fn ), policy ));
    };

    template <typename T, typename Functor, template <typename> typename FutureType, typename PolicyType = std::launch>
    inline ThenableFuture<result_of_cb_t<Functor, T>> then2( FutureType<T> &fut, Functor &&fn, PolicyType policy = default_policy ) {
        return to_thenable( then( std::move( fut ), std::forward<Functor>( fn ), policy ));
    };

    //////////

    template <typename T>
    class ThenablePromise : public std::promise<T> {
        public:
            inline ThenablePromise() : std::promise<T>() {}

            inline ThenablePromise( std::promise<T> &&p ) : std::promise<T>( std::forward<std::promise<T>>( p )) {}

            inline ThenablePromise( ThenablePromise &&p ) : std::promise<T>( std::forward<std::promise<T>>( p )) {}

            ThenablePromise( const ThenablePromise & ) = delete;

            ThenablePromise &operator=( const ThenablePromise & ) = delete;

            constexpr operator std::promise<T> &() THENABLE_NOEXCEPT {
                return *static_cast<std::promise<T> *>(this);
            }

            constexpr operator std::promise<T> &&() THENABLE_NOEXCEPT {
                return std::move( *static_cast<std::promise<T> *>(this));
            }

            inline ThenableFuture<T> get_thenable_future() {
                return this->get_future(); //Automatically converts it
            }

            template <typename Functor, typename PolicyType = std::launch>
            inline ThenableFuture<typename result_of_cb<Functor, T>::type> then( Functor &&fn, PolicyType policy = default_policy ) {
                return then2( this->get_future(), std::forward<Functor>( fn ), policy );
            }
    };

    /*
     * This is a future object functionally equivalent to std::future,
     * but with a .then function to chain together many futures.
     * */

    template <typename T>
    class ThenableFuture : public std::future<T> {
        public:
            constexpr ThenableFuture() THENABLE_NOEXCEPT : std::future<T>() {}

            inline ThenableFuture( std::future<T> &&fut ) THENABLE_NOEXCEPT : std::future<T>( std::forward<std::future<T>>( fut )) {}

            inline ThenableFuture( ThenableFuture &&fut ) THENABLE_NOEXCEPT : std::future<T>( std::forward<std::future<T>>( fut )) {}

            ThenableFuture( const ThenableFuture & ) = delete;

            ThenableFuture &operator=( const ThenableFuture & ) = delete;

            constexpr operator std::future<T> &() {
                return *static_cast<std::future<T> *>(this);
            }

            constexpr operator std::future<T> &&() {
                return std::move( *static_cast<std::future<T> *>(this));
            }

            template <typename Functor, typename PolicyType = std::launch>
            inline ThenableFuture<typename result_of_cb<Functor, T>::type> then( Functor &&fn, PolicyType policy = default_policy ) {
                return then2( std::move( *this ), std::forward<Functor>( fn ), policy );
            }

            inline ThenableSharedFuture<T> share_thenable() {
                return ThenableSharedFuture<T>( this->share());
            }
    };

    template <typename T>
    class ThenableSharedFuture : public std::shared_future<T> {
        public:
            constexpr ThenableSharedFuture() THENABLE_NOEXCEPT : std::shared_future<T>() {}

            inline ThenableSharedFuture( const std::shared_future<T> &fut ) THENABLE_NOEXCEPT : std::shared_future<T>( fut ) {}

            inline ThenableSharedFuture( const ThenableSharedFuture &fut ) THENABLE_NOEXCEPT : std::shared_future<T>( fut ) {}

            inline ThenableSharedFuture( std::future<T> &&fut ) THENABLE_NOEXCEPT : std::shared_future<T>( std::forward<std::shared_future<T>>( fut )) {}

            inline ThenableSharedFuture( ThenableFuture<T> &&fut ) THENABLE_NOEXCEPT : std::shared_future<T>( std::forward<std::shared_future<T>>( fut )) {}

            inline ThenableSharedFuture( std::shared_future<T> &&fut ) THENABLE_NOEXCEPT : std::shared_future<T>( std::forward<std::shared_future<T>>( fut )) {}

            inline ThenableSharedFuture( ThenableSharedFuture &&fut ) THENABLE_NOEXCEPT : std::shared_future<T>( std::forward<std::shared_future<T>>( fut )) {}

            constexpr operator std::shared_future<T> &() {
                return *static_cast<std::shared_future<T> *>(this);
            }

            constexpr operator std::shared_future<T> &&() {
                return std::move( *static_cast<std::shared_future<T> *>(this));
            }

            template <typename Functor, typename PolicyType = std::launch>
            inline ThenableFuture<typename result_of_cb<Functor, T>::type> then( Functor &&fn, PolicyType policy = default_policy ) {
                //Makes a copy of the current shared future so it doesn't move and invalidate this
                return then2( std::shared_future<T>( *this ), std::forward<Functor>( fn ), policy );
            }
    };

    //////////

    template <typename T, template <typename> typename FutureType>
    constexpr thenable_equivalent_t<FutureType<T>>
    to_thenable( FutureType<T> &&fut ) {
        return thenable_equivalent_t<FutureType<T>>( std::forward<FutureType<T>>( fut ));
    }

    template <typename T, template <typename> typename FutureType>
    constexpr thenable_equivalent_t<FutureType<T>>
    to_thenable( FutureType<T> &fut ) {
        return thenable_equivalent_t<FutureType<T>>( std::move( fut ));
    }

    template <typename T0,
              typename T1,
              typename... Ts,
        template <typename> typename FutureType0,
        template <typename> typename FutureType1,
        template <typename> typename... FutureTypes>
    constexpr std::tuple<thenable_equivalent_t<FutureType0<T0>>,
                         thenable_equivalent_t<FutureType1<T1>>,
                         thenable_equivalent_t<FutureTypes<Ts>>...>
    to_thenable( FutureType0<T0> &&fut0,
                 FutureType1<T1> &&fut1,
                 FutureTypes<Ts> &&... futures ) {
        return std::tuple<thenable_equivalent_t<FutureType0<T0>>,
                          thenable_equivalent_t<FutureType1<T1>>,
                          thenable_equivalent_t<FutureTypes<Ts>>...>( std::forward<FutureType0<T0>>( fut0 ),
                                                                      std::forward<FutureType1<T1>>( fut1 ),
                                                                      std::forward<FutureTypes<Ts>>( futures )... );
    };

    template <typename T0,
              typename T1,
              typename... Ts,
        template <typename> typename FutureType0,
        template <typename> typename FutureType1,
        template <typename> typename... FutureTypes>
    constexpr std::tuple<thenable_equivalent_t<FutureType0<T0>>,
                         thenable_equivalent_t<FutureType1<T1>>,
                         thenable_equivalent_t<FutureTypes<Ts>>...>
    to_thenable( FutureType0<T0> &fut0,
                 FutureType1<T1> &fut1,
                 FutureTypes<Ts> &... futures ) {
        return std::tuple<thenable_equivalent_t<FutureType0<T0>>,
                          thenable_equivalent_t<FutureType1<T1>>,
                          thenable_equivalent_t<FutureTypes<Ts>>...>( std::move( fut0 ),
                                                                      std::move( fut1 ),
                                                                      std::move( futures )... );
    };

    template <typename... Ts, template <typename> typename... FutureTypes>
    constexpr std::tuple<thenable_equivalent_t<FutureTypes<Ts>>...> to_thenable_tuple( std::tuple<FutureTypes<Ts>...> &&futures ) {
        return std::tuple<thenable_equivalent_t<FutureTypes<Ts>>...>( std::forward<std::tuple<FutureTypes<Ts>...>>( futures ));
    };

    template <typename... Ts, template <typename> typename... FutureTypes>
    constexpr std::tuple<thenable_equivalent_t<FutureTypes<Ts>>...> to_thenable_tuple( std::tuple<FutureTypes<Ts>...> &futures ) {
        return std::tuple<thenable_equivalent_t<FutureTypes<Ts>>...>( std::move( futures ));
    };

    //////////

    /*
     * defer2 is a variation of defer that returns a ThenableFuture instead of std::future
     * */

    template <typename Functor, typename... Args>
    inline ThenableFuture<typename std::result_of<Functor( Args... )>::type> defer2( Functor &&fn, Args &&... args ) {
        return to_thenable( defer( std::forward<Functor>( fn ), std::forward<Args>( args )... ));
    };

    //////////

    /*
     * make_promise2 is a variation of make_promise that returns a ThenableFuture instead of std::future
     * */

    template <typename T = void, typename Functor, typename PolicyType = std::launch>
    inline ThenableFuture<T> make_promise2( Functor &&fn, PolicyType policy = default_policy ) {
        return to_thenable( make_promise<T>( std::forward<Functor>( fn ), policy ));
    };

    //////////

    /*
     * timeout2 is a variation of timeout that returns a ThenableFuture instead of std::future
     * */

    template <typename T, typename _Rep, typename _Period>
    inline ThenableFuture<std::decay_t<T>> timeout2( std::chrono::duration<_Rep, _Period> duration, T &&value, std::launch policy = default_policy ) {
        return to_thenable( timeout( duration, std::forward<T>( value ), policy ));
    };

    template <typename T, typename _Rep, typename _Period>
    inline ThenableFuture<std::decay_t<T>> timeout2( std::chrono::duration<_Rep, _Period> duration, T &&value, then_launch policy ) {
        return to_thenable( timeout( duration, std::forward<T>( value ), policy ));
    };

    template <typename _Rep, typename _Period>
    inline ThenableFuture<void> timeout2( std::chrono::duration<_Rep, _Period> duration, std::launch policy = default_policy ) {
        return to_thenable( timeout( duration, policy ));
    };

    template <typename _Rep, typename _Period>
    inline ThenableFuture<void> timeout2( std::chrono::duration<_Rep, _Period> duration, then_launch policy ) {
        return to_thenable( timeout( duration, policy ));
    };
}

#endif //THENABLE_FUTURE_HPP_INCLUDED
