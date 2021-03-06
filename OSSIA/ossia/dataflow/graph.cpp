// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <ossia/dataflow/audio_parameter.hpp>
#include <ossia/dataflow/graph.hpp>
#include <boost/range/algorithm/lexicographical_compare.hpp>

namespace ossia
{
struct topological_ordering
{
  const graph& gr;
  const std::vector<graph_node*>& topo_order;
  const execution_state& st;
  bool operator()(const graph_node* lhs, const graph_node* rhs) const
  {
    // just return the nodes in their topological order
    for(std::size_t i = 0, N = topo_order.size(); i < N; i++)
    {
      if(topo_order[i] == lhs)
        return true;
      if(topo_order[i] == rhs)
        return false;
    }
    throw std::runtime_error("lhs and rhs have to be found");
  }
};

struct temporal_ordering
{
  const graph& gr;
  const std::vector<graph_node*>& topo_order;
  const execution_state& st;
  bool operator()(const graph_node* lhs, const graph_node* rhs) const
  {
    return boost::range::lexicographical_compare(lhs->temporal_priority, rhs->temporal_priority);
  }
};

struct custom_ordering
{
  const graph& gr;
  const std::vector<graph_node*>& topo_order;
  const execution_state& st;
  bool operator()(const graph_node* lhs, const graph_node* rhs) const
  {
    return boost::range::lexicographical_compare(lhs->custom_priority, rhs->custom_priority);
  }
};

template<typename OrderingPolicy = topological_ordering>
struct node_sorter
{
  const graph& gr;
  const std::vector<graph_node*>& topo_order;
  const execution_state& st;

  bool operator()(const graph_node* lhs, const graph_node* rhs) const
  {
    // This sorting method ensures that if for instance
    // node A produces "/a" and node B consumes "/a",
    // A executes before B.
    bool c1 = lhs->has_port_inputs();
    bool c2 = rhs->has_port_inputs();
    if (c1 && !c2)
      return true;
    else if (!c1 && c2)
      return false;
    else if (c1 && c2)
      // the nodes are already sorted through the toposort
      // so we can just keep their original order
      return OrderingPolicy{gr, topo_order, st}(lhs, rhs);

    bool l1 = lhs->has_local_inputs(st);
    bool l2 = rhs->has_local_inputs(st);

    if (l1 && !l2)
      return true;
    else if (!l1 && l2)
      return false;
    else if (l1 && l2)
      return OrderingPolicy{gr, topo_order, st}(lhs, rhs);

    bool g1 = lhs->has_global_inputs();
    bool g2 = rhs->has_global_inputs();
    if (g1 && !g2)
      return true;
    else if (!g1 && g2)
      return false;
    else if (g1 && g2)
      return OrderingPolicy{gr, topo_order, st}(lhs, rhs);

    return OrderingPolicy{gr, topo_order, st}(lhs, rhs);
  }
};

struct init_node_visitor
{
  inlet& in;
  graph_edge& edge;
  execution_state& e;

  void operator()(immediate_glutton_connection) const
  {
    operator()();
  }

  void operator()(immediate_strict_connection) const
  {
    graph::copy(*edge.out, in);
  }

  void operator()(delayed_glutton_connection& con) const
  {
    graph::copy(con.buffer, con.pos, in);
    con.pos++;
  }

  void operator()(delayed_strict_connection& con) const
  {
    graph::copy(con.buffer, con.pos, in);
    con.pos++;
  }

  void operator()(reduction_connection) const
  {
    operator()();
  }
  void operator()(replacing_connection) const
  {
    operator()();
  }
  void operator()(dependency_connection) const
  {
    operator()();
  }
  void operator()() const
  {
    if (edge.out_node->enabled())
      graph::copy(*edge.out, in);
    else
    {
      // todo delay, etc
      graph::pull_from_parameter(in, e);
    }
  }
};

struct env_writer
{
  outlet& out;
  graph_edge& edge;
  execution_state& e;
  void operator()(immediate_glutton_connection) const
  {
    if (!edge.in_node->enabled())
      out.write(e);
  }
  void operator()(immediate_strict_connection) const
  {
    // Nothing to do : copied on "input" phase
  }
  void operator()(delayed_glutton_connection& con) const
  {
    // Copy to the buffer
    if (con.buffer && out.data && con.buffer.which() == out.data.which())
      eggs::variants::apply(copy_data{}, out.data, con.buffer);
  }
  void operator()(delayed_strict_connection& con) const
  {
    // Copy to the buffer
    if (con.buffer && out.data && con.buffer.which() == out.data.which())
      eggs::variants::apply(copy_data{}, out.data, con.buffer);
  }
  void operator()(reduction_connection) const
  {
  }
  void operator()(replacing_connection) const
  {
  }
  void operator()(dependency_connection) const
  {
  }
  void operator()() const
  {
  }
};

graph::~graph()
{
  clear();
}

void graph::add_node(node_ptr n)
{
  auto vtx = boost::add_vertex(n, m_graph);
  m_nodes.insert(node_bimap_v{vtx, std::move(n)});
}

void graph::remove_node(const node_ptr& n)
{
  auto it = m_nodes.right.find(n);
  if (it != m_nodes.right.end())
  {
    boost::remove_vertex(it->second, m_graph);
    m_nodes.right.erase(it);
  }
}

void graph::enable(graph_node& n)
{
  m_user_enabled_nodes.insert(&n);
  n.set_enabled(true);
  // TODO handle temporal ordering here.
}

void graph::disable(graph_node& n)
{
  m_user_enabled_nodes.erase(&n);
  n.set_enabled(false);
}

void graph::connect(const std::shared_ptr<graph_edge>& edge)
{
  auto it1 = m_nodes.right.find(edge->in_node);
  auto it2 = m_nodes.right.find(edge->out_node);
  if (it1 != m_nodes.right.end() && it2 != m_nodes.right.end())
  {
    // TODO check that two edges can be added
    auto res = boost::add_edge(it1->second, it2->second, edge, m_graph);
    if (res.second)
    {
      m_edges.insert(edge_bimap_v{res.first, edge});
      m_edge_map.insert(std::make_pair(
          std::make_pair(edge->in_node.get(), edge->out_node.get()),
          edge.get()));
    }
  }
}

void graph::disconnect(const std::shared_ptr<graph_edge>& edge)
{
  auto it = m_edges.right.find(edge);
  if (it != m_edges.right.end())
  {
    boost::remove_edge(it->second, m_graph);
    m_edge_map.erase(
        std::make_pair(edge->in_node.get(), edge->out_node.get()));
  }
}

void graph::clear()
{
  // TODO clear all the connections, ports, etc, to ensure that there is no
  // shared_ptr loop
  for (auto& node : m_nodes.right)
  {
    node.first->clear();
  }
  for (auto& edge : m_edges.right)
  {
    edge.first->clear();
  }
  m_nodes.clear();
  m_edges.clear();
  m_user_enabled_nodes.clear();
  m_graph.clear();
  m_edge_map.clear();
  m_time = 0;
}

void graph::state()
{
  execution_state e;
  state(e);
  e.commit();
}

template<typename Comparator>
using node_set = boost::container::flat_set<graph_node*, Comparator>;

template<typename T>
void tick(
    graph& g,
    execution_state& e,
    std::vector<graph_node*>& active_nodes,
    T next_nodes)
{
  while (!active_nodes.empty())
  {
    next_nodes.clear();

    // Find all the nodes for which the inlets have executed
    // (or without cables on the inlets)
    for(graph_node* node : active_nodes)
      if(node->can_execute(e))
        next_nodes.insert(node);

    if (!next_nodes.empty())
    {
      // First look if there is a replacement or reduction relationship between
      // the first n nodes
      // If there is, we run all the nodes

      // If there is not we just run the first node
      auto& first_node = **next_nodes.begin();
      g.init_node(first_node, e);
      first_node.run(e);
      first_node.set_executed(true);
      g.teardown_node(first_node, e);
      active_nodes.erase(ossia::find(active_nodes, &first_node));
    }
    else
    {
      break; // nothing more to execute
    }
  }
}

void graph::state(execution_state& e)
{
  // TODO in the future, temporal_graph, space_graph that can be used as
  // processes.

  // Filter disabled nodes (through strict relationships).
  set<graph_node*> enabled(
      m_user_enabled_nodes.begin(), m_user_enabled_nodes.end());
  disable_strict_nodes_rec(enabled);

  // Get a total order on nodes
  std::vector<graph_node*> active_nodes;
  std::deque<graph_vertex_t> topo_order;
  try
  {
    boost::topological_sort(m_graph, std::back_inserter(topo_order));
  }
  catch (const boost::not_a_dag& e)
  {
    return;
  }

  for (graph_vertex_t vtx : topo_order)
  {
    auto n = m_graph[vtx].get();
    if (n->enabled())
      active_nodes.push_back(n);
  }

  // At this point, active_nodes contains
  // all the nodes that will run at this tick.

  // Start executing the nodes

  switch(m_ordering)
  {
    case node_ordering::topological:
      tick(*this, e, active_nodes, node_set<node_sorter<>>{
             node_sorter<>{*this, active_nodes, e}});
      break;
    case node_ordering::temporal:
      tick(*this, e, active_nodes, node_set<node_sorter<temporal_ordering>>{
             node_sorter<temporal_ordering>{*this, active_nodes, e}});
      break;
    case node_ordering::hierarchical:
      tick(*this, e, active_nodes, node_set<node_sorter<custom_ordering>>{
             node_sorter<custom_ordering>{*this, active_nodes, e}});
      break;
  };

  for (auto& node : m_nodes.right)
  {
    node.first->set_executed(false);
  }
}

set<graph_node*>
graph::disable_strict_nodes(const set<graph_node*>& enabled_nodes)
{
  set<graph_node*> ret;

  for (const auto& node : enabled_nodes)
  {
    for (const auto& in : node->inputs())
    {
      for (const auto& edge : in->sources)
      {
        assert(edge->out_node);

        if (immediate_strict_connection* sc = edge->con.target<immediate_strict_connection>())
        {
          if ((sc->required_sides
               & immediate_strict_connection::required_sides_t::outbound)
              && node->enabled() && !edge->out_node->enabled())
          {
            ret.insert(node);
          }
        }
        else if (delayed_strict_connection* delay = edge->con.target<delayed_strict_connection>())
        {
          const auto n = ossia::apply(data_size{}, delay->buffer);
          if (n == 0 || delay->pos >= n)
            ret.insert(node);
        }
      }
    }

    for (const auto& out : node->outputs())
    {
      for (const auto& edge : out->targets)
      {
        assert(edge->in_node);

        if (auto sc = edge->con.target<immediate_strict_connection>())
        {
          if ((sc->required_sides
               & immediate_strict_connection::required_sides_t::inbound)
              && node->enabled() && !edge->in_node->enabled())
          {
            ret.insert(node);
          }
        }
      }
    }
  }

  return ret;
}

set<graph_node*> graph::disable_strict_nodes(const set<node_ptr>& n)
{
  using namespace boost::adaptors;
  auto res = (n | transformed([](auto p) { return p.get(); }));
  return disable_strict_nodes(set<graph_node*>{res.begin(), res.end()});
}

void graph::disable_strict_nodes_rec(set<graph_node*>& cur_enabled_node)
{
  set<graph_node*> to_disable;
  do
  {
    to_disable = disable_strict_nodes(cur_enabled_node);
    for (auto n : to_disable)
    {
      disable(*n);
      cur_enabled_node.erase(n);
      // note: we have to add a dependency between all the inlets and outlets
    }

  } while (!to_disable.empty());
}

void graph::copy_from_local(const data_type& out, inlet& in)
{
  if (out.which() == in.data.which() && out && in.data)
  {
    eggs::variants::apply(copy_data{}, out, in.data);
  }
}

void graph::copy(const delay_line_type& out, std::size_t pos, inlet& in)
{
  if (out.which() == in.data.which() && out && in.data)
  {
    eggs::variants::apply(copy_data_pos{pos}, out, in.data);
  }
}

void graph::copy(const outlet& out, inlet& in)
{
  copy_from_local(out.data, in);
}

void graph::copy_to_local(
    const data_type& out, const destination& d, execution_state& in)
{
  in.insert(destination_t{&d.address()}, out);
}

void graph::copy_to_global(
    const data_type& out, const destination& d, execution_state& in)
{
  // TODO
}

void graph::pull_from_parameter(inlet& in, execution_state& e)
{
  if (auto addr_ptr = in.address.target<ossia::net::parameter_base*>())
  {
    ossia::net::parameter_base* addr = *addr_ptr;
    if (in.scope & port::scope_t::local)
    {
      e.find_and_copy(*addr, in);
    }
    else
    {
      e.copy_from_global(*addr, in);
    }
  }
  else if (auto pattern = in.address.target<std::string>())
  {
    // TODO
  }
}

void graph::init_node(graph_node& n, execution_state& e)
{
  // Clear the outputs of the node
  for (const outlet_ptr& out : n.outputs())
  {
    if (out->data)
      eggs::variants::apply(clear_data{}, out->data);
  }

  // Copy from environment and previous ports to inputs
  for (const inlet_ptr& in : n.inputs())
  {
    if (!in->sources.empty())
    {
      for (auto edge : in->sources)
      {
        ossia::apply(init_node_visitor{*in, *edge, e}, edge->con);
      }
    }
    else
    {
      pull_from_parameter(*in, e);
    }
  }
}

void graph::teardown_node(graph_node& n, execution_state& e)
{
  for (const inlet_ptr& in : n.inputs())
  {
    if (in->data)
      eggs::variants::apply(clear_data{}, in->data);
  }

  // Copy from output ports to environment
  for (const outlet_ptr& out : n.outputs())
  {
    if (out->targets.empty())
    {
      out->write(e);
    }
    else
    {
      // If the following target has been deactivated
      for (auto tgt : out->targets)
      {
        ossia::apply(env_writer{*out, *tgt, e}, tgt->con);
      }
    }
  }
}

audio_parameter::~audio_parameter()
{
}

#if defined(OSSIA_PROTOCOL_MIDI)
midi_generic_parameter::~midi_generic_parameter()
{
}
#endif

texture_generic_parameter::~texture_generic_parameter()
{
}
}
