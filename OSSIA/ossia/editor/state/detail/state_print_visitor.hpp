#pragma once
#include <ossia/editor/state/state_element.hpp>
#include <ossia/editor/value/value.hpp>
#include <ossia/network/base/address.hpp>
#include <ossia/network/base/node.hpp>
#include <iostream>
#include <string>

namespace ossia
{
struct state_print_visitor
{
  std::ostream& out;
  std::string padding;

  void operator()(const state& s)
  {
    out << padding << "state {\n";
    padding.push_back(' ');
    for (auto& sub : s)
      eggs::variants::apply(*this, sub);
    padding.pop_back();
    out << "}\n";
  }

  void operator()(const custom_state& e)
  {
    out << padding << "custom\n";
  }

  void operator()(const message& m)
  {
    if(m.destination.value)
    {
      out << padding << "message: "
          << m.destination.value->getTextualAddress() << " => "
          << ossia::to_pretty_string(m.value) << "\n";
    }
    else
    {
      out << padding << "invalid message";
    }
  }

  void operator()()
  {
    out << "no state\n";
  }
};
}