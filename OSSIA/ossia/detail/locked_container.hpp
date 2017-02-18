#pragma once
#include <ossia/detail/mutex.hpp>
namespace ossia
{
/**
 * \brief Thread-safe read-only reference to a container.
 */
template<typename Container>
class locked_container
{
public:
  locked_container(Container& src, shared_mutex_t& mutex):
    m_ref{src},
    m_mutex{mutex}
  {
    m_mutex.lock_shared();
  }

  ~locked_container()
  {
    m_mutex.unlock_shared();
  }

  auto& front() { return m_ref.front(); }
  const auto& front() const { return m_ref.front(); }

  auto empty() const { return m_ref.empty(); }

  auto size() const { return m_ref.size(); }
  auto begin() const { return m_ref.cbegin(); }
  auto end() const { return m_ref.cend(); }
  auto cbegin() const { return m_ref.cbegin(); }
  auto cend() const { return m_ref.cend(); }
  auto begin() { return m_ref.begin(); }
  auto end() { return m_ref.end(); }
  auto cbegin() { return m_ref.cbegin(); }
  auto cend() { return m_ref.cend(); }

  auto& operator[](std::size_t i) { return m_ref[i]; }
  const auto& operator[](std::size_t i) const { return m_ref[i]; }

private:
  Container& m_ref;
  shared_mutex_t& m_mutex;
};
}
