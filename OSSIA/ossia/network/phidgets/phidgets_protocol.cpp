// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "phidgets_protocol.hpp"
#include "phidgets_parameter.hpp"
#include "phidgets_device.hpp"
#include "phidgets_node.hpp"

namespace ossia
{

phidget_protocol::phidget_protocol()
{
  m_mgr.onPhidgetCreated = [=](ppp::phidget_ptr phid) {
    m_functionQueue.enqueue([=] {
      auto phid_node = new phidget_node(*m_dev, *m_dev);
      phid_node->set_parameter(
          std::make_unique<phidget_parameter>(phid, *this, *phid_node));
      m_dev->add_child(std::unique_ptr<phidget_node>(phid_node));
    });

    if (m_commandCb)
      m_commandCb();
  };
  m_mgr.onPhidgetDestroyed = [=](ppp::phidget_ptr phid) {
    m_functionQueue.enqueue([=] {
      for (auto& cld : m_dev->children_copy())
      {
        auto phid_node = dynamic_cast<phidget_node*>(cld);
        if (phid_node)
        {
          auto addr = dynamic_cast<phidget_parameter*>(phid_node->get_parameter());
          if (addr && addr->phidget() == phid)
          {
            m_dev->remove_child(*phid_node);
            break;
          }
        }
      }
    });

    if (m_commandCb)
      m_commandCb();

  };

  m_mgr.open();
}

bool phidget_protocol::pull(net::parameter_base&)
{
  return true;
}

std::future<void> phidget_protocol::pull_async(net::parameter_base&)
{
  return {};
}

void phidget_protocol::request(net::parameter_base&)
{
}

bool phidget_protocol::push(const net::parameter_base&)
{
  return true;
}

bool phidget_protocol::observe(net::parameter_base&, bool)
{
  return true;
}

bool phidget_protocol::observe_quietly(net::parameter_base&, bool)
{
  return true;
}

bool phidget_protocol::update(net::node_base& node_base)
{
  return true;
}

void phidget_protocol::set_device(net::device_base& dev)
{
  if (auto d = dynamic_cast<phidget_device*>(&dev))
    m_dev = d;
}

void phidget_protocol::set_command_callback(std::function<void()> c)
{
  m_commandCb = c;
}

void phidget_protocol::run_commands()
{
  bool ok = false;
  std::function<void()> cmd;
  do
  {
    ok = m_functionQueue.try_dequeue(cmd);
    if (ok && cmd)
      cmd();
  } while (ok);
}
}
