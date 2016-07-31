#pragma once
#include <ossia_export.h>
#include <utility>

namespace ossia
{

enum class curve_segment_type
{
  INT,
  FLOAT,
  DOUBLE,
  BOOL
};
using curve_type
    = std::pair<ossia::curve_segment_type, ossia::curve_segment_type>;

class OSSIA_EXPORT curve_abstract
{
public:
  /*! destructor */
  virtual ~curve_abstract();

  /*! get the type of both coordinates */
  virtual curve_type getType() const = 0;

  /*! Clear internal data structures of the curve.
   * Call before a new execution. */
  virtual void reset() = 0;
};
}
