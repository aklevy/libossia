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
using past_events_map
    = boost::container::flat_map<time_value, std::shared_ptr<time_event>>;
using DateMap = tsl::hopscotch_map<time_sync*, time_value>;
using EventPtr = std::shared_ptr<ossia::time_event>;
using IntervalPtr = std::shared_ptr<ossia::time_interval>;
static void process_synchronization_dates(time_sync& t, DateMap& map)
{
  map.insert(std::make_pair(&t, t.get_date()));

  for (EventPtr& ev : t.get_time_events())
  {
    for (IntervalPtr& cst : ev->next_time_intervals())
    {
      process_synchronization_dates(cst->get_end_event().get_time_sync(), map);
    }
  }
}

void process_offset(
    time_sync& synchronization, time_value offset, past_events_map& pastEvents)
{
  time_value date = synchronization.get_date();
  auto get_event_status = [](const time_event& event) {
    switch (event.get_offset_behavior())
    {
      case time_event::offset_behavior::EXPRESSION_TRUE:
        return time_event::status::HAPPENED;
      case time_event::offset_behavior::EXPRESSION_FALSE:
        return time_event::status::DISPOSED;
      case time_event::offset_behavior::EXPRESSION:
        return expressions::evaluate(event.get_expression())
                   ? time_event::status::HAPPENED
                   : time_event::status::DISPOSED;
      default:
        return time_event::status::NONE;
    }
  };

  for (auto& ev_ptr : synchronization.get_time_events())
  {
    auto& event = *ev_ptr;
    time_event::status eventStatus;

    // evaluate event status considering its time sync date
    if (date < offset)
    {
      eventStatus = get_event_status(event);
    }
    else if (date == offset)
    {
      eventStatus = time_event::status::PENDING;
    }
    else
    {
      eventStatus = time_event::status::NONE;
    }

    // evaluate event status considering previous time intervals
    for (const auto& timeInterval : event.previous_time_intervals())
    {
      time_value intervalOffset
          = offset
            - timeInterval->get_start_event().get_time_sync().get_date();

      if (intervalOffset < Zero)
      {
        eventStatus = time_event::status::NONE;
      }
      else if (
          intervalOffset >= Zero
          && intervalOffset <= timeInterval->get_max_duration())
      {
        eventStatus = intervalOffset > timeInterval->get_min_duration()
                          ? time_event::status::PENDING
                          : time_event::status::NONE;
      }
      else if (intervalOffset > timeInterval->get_max_duration())
      {
        eventStatus = get_event_status(event);
      }
    }

    // setup event status
    event.set_status(eventStatus);

    // add HAPPENED event to offset event list
    if (eventStatus == time_event::status::HAPPENED)
      pastEvents.insert(std::make_pair(date, ev_ptr));

    // propagate offset processing to setup all TimeEvents
    for (const auto& timeInterval : event.next_time_intervals())
    {
      process_offset(
          timeInterval->get_end_event().get_time_sync(), offset, pastEvents);
    }
  }
}

state_element scenario::offset(ossia::time_value offset, double pos)
{
  // reset internal offset list and state

  // a temporary list to order all past events to build the
  // offset state
  past_events_map pastEvents;

  m_runningIntervals.clear();
  ossia::state cur_state;

  // Precompute the default date of every synchronization.
  tsl::hopscotch_map<time_sync*, time_value> time_map;
  process_synchronization_dates(*m_nodes[0], time_map);

  // Set *every* time interval prior to this one to be rigid
  // note : this change the semantics of the score and should not be done like
  // this;
  // it's only a temporary (1 year later: haha) bugfix for
  // https://github.com/OSSIA/score/issues/253 .
  for (auto& elt : time_map)
  {
    if (elt.second < offset)
    {
      for (EventPtr& ev : elt.first->get_time_events())
      {
        for (IntervalPtr& cst_ptr : ev->previous_time_intervals())
        {
          time_interval& cst = *cst_ptr;
          auto dur = cst.get_nominal_duration();
          cst.set_min_duration(dur);
          cst.set_max_duration(dur);
        }
      }
    }
    else
    {
      for (EventPtr& ev_ptr : elt.first->get_time_events())
      {
        for (IntervalPtr& cst_ptr : ev_ptr->previous_time_intervals())
        {
          time_interval& cst = *cst_ptr;
          auto& start_tn = cst.get_start_event().get_time_sync();
          auto start_date_it = time_map.find(&start_tn);
          if (start_date_it != time_map.end())
          {
            auto start_date = start_date_it->second;
            if (start_date < offset)
            {
              auto dur = cst.get_nominal_duration();
              auto dur_min = cst.get_min_duration();
              if (dur_min < dur)
                cst.set_min_duration(offset - start_date);
            }
          }
        }
      }
    }
  }

  // propagate offset from the first Synchronization
  process_offset(*m_nodes[0], offset, pastEvents);

  // build offset state from all ordered past events
  if (unmuted())
  {
    for (const auto& p : pastEvents)
    {
      merge_flatten_and_filter(cur_state, p.second->get_state());
    }
  }

  // offset all TimeIntervals
  for (const auto& timeInterval : m_intervals)
  {
    ossia::time_interval& cst = *timeInterval;

    auto& sev = cst.get_start_event();
    auto& stn = sev.get_time_sync();
    auto start_date = sev.get_time_sync().get_date();
    bool all_empty = ossia::all_of(stn.get_time_events(), [](const auto& ev) {
      return ev->previous_time_intervals().empty();
    });

    if (all_empty && &stn != m_nodes[0].get())
      continue;

    // offset TimeInterval's Clock
    time_value intervalOffset = offset - start_date;

    if (intervalOffset >= Zero && intervalOffset <= cst.get_max_duration())
    {
      flatten_and_filter(cur_state, cst.offset(intervalOffset));
      m_runningIntervals.insert(&cst);
    }
  }

  m_lastState = cur_state;
  m_lastDate = offset;

  return m_lastState;
}
}
