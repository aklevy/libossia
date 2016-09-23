#pragma once
#include <ossia/editor/dataspace/dataspace_base.hpp>

namespace ossia
{

template<typename T>
struct speed_ratio;

using meter_per_second_u = speed_ratio<std::ratio<1>>;
using miles_per_hour_u = speed_ratio<std::ratio<254 * 12 * 5280, 3600 * 10000>>;
using kilometer_per_hour_u = speed_ratio<std::ratio<1000, 3600>>;
using knot_u = speed_ratio<std::ratio<1852, 3600>>;
using foot_per_hour_u = speed_ratio<std::ratio<254 * 12, 3600 * 10000>>;
using foot_per_second_u = speed_ratio<std::ratio<254 * 12, 10000>>;

template<typename Impl>
struct speed_unit
{
  using neutral_unit = meter_per_second_u;
  using value_type = Float;
  using concrete_type = Impl;
  using dataspace_type = struct speed_dataspace;
};

template<typename T>
struct speed_ratio :
    public linear_unit<speed_unit<speed_ratio<T>>, T>
{
  using linear_unit<speed_unit<speed_ratio<T>>, T>::linear_unit;
};

using meter_per_second = strong_value<meter_per_second_u>;
using miles_per_hour = strong_value<miles_per_hour_u>;
using kilometer_per_hour = strong_value<kilometer_per_hour_u>;
using knot = strong_value<knot_u>;
using foot_per_second = strong_value<foot_per_second_u>;
using foot_per_hour = strong_value<foot_per_hour_u>;

using speed_u =
  eggs::variant<
    meter_per_second_u, miles_per_hour_u, kilometer_per_hour_u, knot_u, foot_per_second_u, foot_per_hour_u>;
using speed =
  eggs::variant<
    meter_per_second, miles_per_hour, kilometer_per_hour, knot, foot_per_second, foot_per_hour>;


template<>
struct unit_traits<meter_per_second_u>
{
  static constexpr const auto text()
  { return ossia::make_array("m/s"); }
};

template<>
struct unit_traits<miles_per_hour_u>
{
  static constexpr const auto text()
  { return ossia::make_array("mph"); }
};

template<>
struct unit_traits<kilometer_per_hour_u>
{
  static constexpr const auto text()
  { return ossia::make_array("k/h"); }
};

template<>
struct unit_traits<knot_u>
{
  static constexpr const auto text()
  { return ossia::make_array("kn"); }
};

template<>
struct unit_traits<foot_per_hour_u>
{
  static constexpr const auto text()
  { return ossia::make_array("f/h"); }
};

template<>
struct unit_traits<foot_per_second_u>
{
  static constexpr const auto text()
  { return ossia::make_array("f/s"); }
};

}