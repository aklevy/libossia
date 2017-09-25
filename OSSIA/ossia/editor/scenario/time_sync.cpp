// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/editor/scenario/time_sync.hpp>

namespace ossia
{

time_sync::time_sync() : m_expression(expressions::make_expression_true())
{
}

time_sync::~time_sync() = default;

bool time_sync::trigger(ossia::state& st)
{
  // if all TimeEvents are not PENDING
  if (m_pending.size() != get_time_events().size())
  {
    // stop expression observation because the Synchronization is not ready to be
    // processed
    observe_expression(false);

    // the triggering failed
    return false;
  }

  // now TimeEvents will happen or be disposed
  for (auto& timeEvent : m_pending)
  {
    time_event& ev = *timeEvent;
    auto& expr = ev.get_expression();
    // update any Destination value into the expression
    expressions::update(expr);

    if (expressions::evaluate(expr))
      ev.happen(st);
    else
      ev.dispose();
  }

  // stop expression observation now the Synchronization has been processed
  observe_expression(false);

  // notify observers
  triggered.send();

  // the triggering succeded
  return true;
}

time_value time_sync::get_date() const
{
  // compute the date from each first previous time interval
  // ignoring zero duration time interval
  if (!get_time_events().empty())
  {
    for (auto& timeEvent : get_time_events())
    {
      if (!timeEvent->previous_time_intervals().empty())
      {
        if (timeEvent->previous_time_intervals()[0]->get_nominal_duration()
            > Zero)
          return timeEvent->previous_time_intervals()[0]
                     ->get_nominal_duration()
                 + timeEvent->previous_time_intervals()[0]
                       ->get_start_event()
                       .get_time_sync()
                       .get_date();
      }
    }
  }

  return Zero;
}

const expression& time_sync::get_expression() const
{
  return *m_expression;
}

time_sync& time_sync::set_expression(expression_ptr exp)
{
  assert(exp);
  m_expression = std::move(exp);
  return *this;
}

time_sync::iterator time_sync::insert(
    time_sync::const_iterator pos, std::shared_ptr<time_event> ev)
{
  return m_timeEvents.insert(pos, std::move(ev));
}

void time_sync::remove(const std::shared_ptr<time_event>& e)
{
  remove_one(m_pending, e.get());
  remove_one(m_timeEvents, e);
}

time_sync::iterator time_sync::emplace(
    const_iterator pos, time_event::exec_callback callback,
    ossia::expression_ptr exp)
{
  return insert(
      pos, std::make_shared<time_event>(callback, *this, std::move(exp)));
}

void time_sync::process(std::vector<time_event*>& statusChangedEvents, ossia::state& st)
{
  // prepare to remember which event changed its status to PENDING
  // because it is needed in time_sync::trigger
  m_pending.clear();

  bool maximalDurationReached = false;

  for (auto& timeEvent : get_time_events())
  {
    switch (timeEvent->get_status())
    {
      // check if NONE TimeEvent is ready to become PENDING
      case time_event::status::NONE:
      {
        bool minimalDurationReached = true;

        for (auto& timeInterval : timeEvent->previous_time_intervals())
        {
          // previous TimeIntervals with a DISPOSED start event are ignored
          if (timeInterval->get_start_event().get_status()
              == time_event::status::DISPOSED)
            continue;

          // previous TimeInterval with a none HAPPENED start event
          // can't have reached its minimal duration
          if (timeInterval->get_start_event().get_status()
              != time_event::status::HAPPENED)
          {
            minimalDurationReached = false;
            break;
          }

          // previous TimeInterval which doesn't reached its minimal duration
          // force to quit
          if (timeInterval->get_date() < timeInterval->get_min_duration())
          {
            minimalDurationReached = false;
            break;
          }
        }

        // access to PENDING status once all previous TimeIntervals allow it
        if (minimalDurationReached)
          timeEvent->set_status(time_event::status::PENDING);
        else
          break;
      }

      // PENDING TimeEvent is ready for evaluation
      case time_event::status::PENDING:
      {
        m_pending.push_back(timeEvent.get());

        for (auto& timeInterval : timeEvent->previous_time_intervals())
        {
          if (timeInterval->get_date() >= timeInterval->get_max_duration())
          {
            maximalDurationReached = true;
            break;
          }
        }

        break;
      }

      // HAPPENED TimeEvent propagates recursively the execution to the end of
      // each next TimeIntervals
      case time_event::status::HAPPENED:
      {
        for (auto& timeInterval : timeEvent->next_time_intervals())
        {
          timeInterval->get_end_event().get_time_sync().process(
              statusChangedEvents, st);
        }

        break;
      }

      // DISPOSED TimeEvent stops the propagation of the execution
      case time_event::status::DISPOSED:
      {
        break;
      }
    }
  }

  // if all TimeEvents are not PENDING
  if (m_pending.size() != m_timeEvents.size())
  {
    if (m_evaluating)
    {
      m_evaluating = false;
      left_evaluation.send();
    }

    return;
  }

  if (!m_evaluating)
  {
    m_evaluating = true;
    entered_evaluation.send();
  }

  //! \todo force triggering if at leat one TimeEvent has
  // at least one TimeInterval over its maximal duration

  // update the expression one time
  // then observe and evaluate Synchronization's expression before to trig
  // only if no maximal duration have been reached
  if (*m_expression != expressions::expression_true()
      && !maximalDurationReached)
  {
    if (!is_observing_expression())
      expressions::update(*m_expression);

    observe_expression(true);

    if (!expressions::evaluate(*m_expression))
      return;
  }

  // trigger the time sync
  if (trigger(st))
  {
    // former PENDING TimeEvents are now HAPPENED or DISPOSED
    statusChangedEvents.insert(
        statusChangedEvents.end(), m_pending.begin(), m_pending.end());

    m_evaluating = false;
    finished_evaluation.send(maximalDurationReached);
  }
}

bool time_sync::is_evaluating() const
{
  return m_evaluating;
}

bool time_sync::is_observing_expression() const
{
  return m_observe;
}

void time_sync::observe_expression(bool observe)
{
  if (!m_expression || *m_expression == expressions::expression_true()
      || *m_expression == expressions::expression_false())
    return;

  if (observe != m_observe)
  {
    bool wasObserving = m_observe;
    m_observe = observe;

    if (m_observe)
    {
      // pull value

      // start expression observation; dummy callback used.
      // Do not remove it : else the expressions will stop listening.
      m_callback = expressions::add_callback(*m_expression, [](bool) {});
    }
    else
    {
      // stop expression observation
      if (wasObserving && m_callback)
      {
        expressions::remove_callback(*m_expression, *m_callback);
        m_callback = ossia::none;
      }
    }
  }
}

void time_sync::reset()
{
  for (auto& timeEvent : m_timeEvents)
  {
    timeEvent->reset();
  }

  m_pending.clear();
  m_observe = false;
  m_evaluating = false;
}

void time_sync::cleanup()
{
  for (auto& timeevent : m_timeEvents)
    timeevent->cleanup();

  m_pending.clear();
  m_timeEvents.clear();
}
}
