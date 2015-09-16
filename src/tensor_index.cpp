#include "tensor_index.h"

#include "error.h"
#include "util.h"

using namespace std;

namespace simit {
namespace ir {

TensorIndex::TensorIndex(std::string name, pe::PathExpression pexpr)
    : name(name), pexpr(pexpr) {
  string prefix = (name == "") ? name : name + "_";
  this->coordArray = Var(prefix + "coords", ArrayType::make(ScalarType::Int));
  this->sinkArray  = Var(prefix + "sinks",  ArrayType::make(ScalarType::Int));
}

ostream &operator<<(ostream& os, const TensorIndex& ti) {
  os << "tensor-index " << ti.getName() << ": " << ti.getPathExpression()
     << endl;
  os << "  " << ti.getCoordArray() << endl;
  os << "  " << ti.getSinkArray();
  return os;
}

}}
