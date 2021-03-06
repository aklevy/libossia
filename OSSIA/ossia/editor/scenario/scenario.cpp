// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <ossia/editor/scenario/scenario.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/editor/scenario/time_event.hpp>
#include <ossia/editor/scenario/time_sync.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/logger.hpp>
#include <ossia/editor/exceptions.hpp>
#include <cassert>
#include <hopscotch_map.h>
#include <iostream>
#include <map>
#include <set>

namespace ossia
{
scenario::scenario()
{
  // create the start TimeSync
  m_nodes.push_back(std::make_shared<time_sync>());
}

scenario::~scenario()
{
  for (auto& timesync : m_nodes)
  {
    timesync->cleanup();
  }
}

void scenario::start(ossia::state& st)
{
  for(auto& node : m_nodes)
  {
    auto bool_expr = node->get_expression().target<ossia::expressions::expression_bool>();
    if(!bool_expr || !bool_expr->evaluate() || node == m_nodes[0])
    {
      auto ok = ossia::all_of(
            node->get_time_events(),
            [] (const std::shared_ptr<ossia::time_event>& ev) {
        return ev->previous_time_intervals().empty();
      });
      if(ok)
      {
        m_waitingNodes.push_back(node.get());
      }
    }
  }

  // start each TimeInterval if possible
  for (const auto& timeInterval : m_intervals)
  {
    time_interval& cst = *timeInterval;
    time_event::status startStatus = cst.get_start_event().get_status();
    time_event::status endStatus = cst.get_end_event().get_status();

    // the interval is in the past
    if (startStatus == time_event::status::HAPPENED
        && endStatus == time_event::status::HAPPENED)
    {
    }
    // the start of the interval is pending
    else if (
        startStatus == time_event::status::PENDING
        && endStatus == time_event::status::NONE)
    {
    }
    // the interval is supposed to be running
    else if (
        startStatus == time_event::status::HAPPENED
        && endStatus == time_event::status::NONE)
    {
      m_runningIntervals.insert(&cst);
      cst.start(st);
    }
    // the interval starts in the void and ends on a timesync that did
    // execute
    else if (
        startStatus == time_event::status::NONE
        && endStatus == time_event::status::HAPPENED)
    {
    }
    // the end of the interval is pending
    else if (
        startStatus == time_event::status::HAPPENED
        && endStatus == time_event::status::PENDING)
    {
      m_runningIntervals.insert(&cst);
      cst.start(st);
    }
    // the interval is in the future
    else if (
        startStatus == time_event::status::NONE
        && endStatus == time_event::status::NONE)
    {
    }
    // error
    else
    {
      throw execution_error(
          "scenario_impl::start: "
          "TimeEvent's status configuration of the "
          "TimeInterval is not handled");
    }
  }
}

void scenario::stop()
{
  // stop each running TimeIntervals
  for (const auto& timeInterval : m_intervals)
  {
    time_interval& cst = *timeInterval;
    cst.stop();
  }

  for (const auto& node : m_nodes)
  {
    node->reset();
  }

  m_runningIntervals.clear();
  intervals_started.clear();
  intervals_stopped.clear();
  m_waitingNodes.clear();
  m_overticks.clear();
  m_lastDate = time_value{};
}

void scenario::pause()
{
  // pause all running TimeIntervals
  for (const auto& timeInterval : m_intervals)
  {
    auto& cst = *timeInterval;
    cst.pause();
  }
}

void scenario::resume()
{
  // resume all running TimeIntervals
  for (const auto& timeInterval : m_intervals)
  {
    auto& cst = *timeInterval;
    cst.resume();
  }
}

void scenario::add_time_interval(
    std::shared_ptr<time_interval> timeInterval)
{
  time_interval& cst = *timeInterval;

  // store the TimeInterval if it is not already stored
  if (!contains(m_intervals, timeInterval))
  {
    m_intervals.push_back(std::move(timeInterval));
  }

  // store TimeInterval's start node if it is not already stored
  add_time_sync(cst.get_start_event().get_time_sync().shared_from_this());

  // store TimeInterval's end node if it is not already stored
  add_time_sync(cst.get_end_event().get_time_sync().shared_from_this());
}

void scenario::remove_time_interval(
    const std::shared_ptr<time_interval>& timeInterval)
{
  remove_one(m_intervals, timeInterval);
}

void scenario::add_time_sync(std::shared_ptr<time_sync> timeSync)
{
  // store a TimeSync if it is not already stored
  if (!contains(m_nodes, timeSync))
  {
    m_nodes.push_back(std::move(timeSync));
  }
}

void scenario::remove_time_sync(const std::shared_ptr<time_sync>& timeSync)
{
  remove_one(m_nodes, timeSync);
}

const std::shared_ptr<time_sync>& scenario::get_start_time_sync() const
{
  return m_nodes[0];
}

const ptr_container<time_sync>& scenario::get_time_syncs() const
{
  return m_nodes;
}

const ptr_container<time_interval>& scenario::get_time_intervals() const
{
  return m_intervals;
}
}
