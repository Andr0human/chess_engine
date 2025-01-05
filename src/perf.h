
#ifndef PERF_H
#define PERF_H

#include <cstdint>
#include <iostream>
#include <chrono>
#include <utility>

#define FAST_IO() std::ios::sync_with_stdio(0); std::cin.tie(0)

using perf_clock = std::chrono::_V2::high_resolution_clock::time_point;
using perf_time = std::chrono::duration<double>;
using perf_ms_time = std::chrono::duration<int64_t, std::milli>;
using perf_ns_time = std::chrono::duration<int64_t, std::nano>;

namespace perf
{

inline perf_clock
now()
{ return std::chrono::high_resolution_clock::now(); }

struct Timer
{
    perf_clock _start, _end;
    perf_time duration;
    std::string func_name;

    Timer()
    { 
        func_name = "Timer";
        _start = now();
    }

    Timer(std::string function_name)
    {
        func_name = function_name;
        _start = now();
    }

    ~Timer()
    {
        _end = now();
        duration = _end - _start;
        std::cout << func_name << " took " << duration.count()
                  << " sec." << std::endl;
    }
};


/**
 * @brief Returns the time_elpased by a function
 * 
 * Not applicable for template or non-static member functions.
 * 
 * @tparam _Callable 
 * @tparam _Args 
 * @param __f function
 * @param __args arguments of the functions
 * @return func_time (in sec.)
 */
template <typename _Callable, typename... _Args> double
Time(const _Callable &__f, _Args&&... __args)
{
    const perf_clock start = now();

    __f(std::forward<_Args>(__args)...);
    
    const perf_time dur = now() - start;
    return dur.count();
}


/**
 * @brief Returns the value & time_elapsed by func. as a pair.
 * 
 * Not applicable for void, template or non-static member functions. (use perf::Time instead for voids)
 * 
 * @tparam _Callable 
 * @tparam _Args 
 * @param __f function
 * @param __args arguments of the functions
 * @return pair( func_return_value, func_time(in sec.) )
 */
template <typename _Callable, typename... _Args> const auto
run_algo(const _Callable &__f, _Args&&... __args)
{
    const perf_clock start = now();

    const auto ret_val = __f(std::forward<_Args>(__args)...);
    
    const perf_time dur = now() - start;
    return std::make_pair(ret_val, dur.count());
}


}


#endif



