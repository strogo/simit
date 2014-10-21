#ifndef SIMIT_TYPES_H
#define SIMIT_TYPES_H

#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include <map>

#include "domain.h"

namespace simit {
namespace ir {

struct TypeNode {};
struct ScalarType;
struct TensorType;
struct ElementType;
struct SetType;
struct TupleType;

class Type {
public:
  enum Kind {Tensor, Element, Set, Tuple};
  Type() : ptr(nullptr) {}
  Type(TensorType *tensor);
  Type(ElementType *element);
  Type(SetType *set);
  Type(TupleType *tuple);

  bool defined() const { return ptr != nullptr; }

  Kind kind() const { return _kind; }

  bool isTensor()  const { return _kind==Tensor; }
  bool isElement() const { return _kind==Element; }
  bool isSet()     const { return _kind==Set; }
  bool isTuple()   const { return _kind==Tuple; }

  const TensorType  *toTensor()  const { assert(isTensor());  return tensor; }
  const ElementType *toElement() const { assert(isElement()); return element; }
  const SetType     *toSet()     const { assert(isSet());     return set; }
  const TupleType   *toTuple()   const { assert(isTuple());   return tuple; }

private:
  Kind _kind;
  union {
    TensorType  *tensor;
    ElementType *element;
    SetType     *set;
    TupleType   *tuple;
  };
  std::shared_ptr<TypeNode> ptr;
};

struct ScalarType {
  enum Kind {Int, Float};

  ScalarType() : kind(Int), bits(0) {}
  ScalarType(Kind kind, int bits) : kind(kind), bits(bits) {}

  Kind kind;
  int bits;

  int bytes() const { return (bits + 7) / 8; }

  bool isInt () const { return kind == Int; }
  bool isFloat() const { return kind == Float; }
};

struct TensorType : TypeNode {
  ScalarType componentType;
  std::vector<IndexDomain> dimensions;

  /// Marks whether the tensor type is a column vector.  This information is
  /// not used by the Simit compiler, but is here to ease frontend development.
  bool isColumnVector;

  size_t order() const { return dimensions.size(); }
  size_t size() const;

  static Type make(ScalarType componentType) {
    std::vector<IndexDomain> dimensions;
    return TensorType::make(componentType, dimensions);
  }

  static Type make(ScalarType componentType, std::vector<IndexDomain> dimensions,
                   bool isColumnVector = false) {
    TensorType *type = new TensorType;
    type->componentType = componentType;
    type->dimensions = dimensions;
    type->isColumnVector = isColumnVector;
    return type;
  }
};

inline Type Int(int bits) {
  return TensorType::make(ScalarType(ScalarType::Int, bits));
}

inline Type Float(int bits) {
  return TensorType::make(ScalarType(ScalarType::Float, bits));
}

struct ElementType : TypeNode {
  std::string name;
  std::map<std::string,Type> fields;

  static Type make(std::string name, std::map<std::string,Type> fields) {
    ElementType *type = new ElementType;
    type->name = name;
    type->fields = fields;
    return type;
  }
};

struct SetType : TypeNode {
  Type elementType;

  static Type make(Type elementType) {
    assert(elementType.isElement());
    SetType *type = new SetType;
    type->elementType = elementType;
    return type;
  }
};

struct TupleType : TypeNode {
  Type elementType;
  size_t size;

  static Type make(Type elementType, size_t size) {
    assert(elementType.isElement());
    TupleType *type = new TupleType;
    type->elementType = elementType;
    type->size = size;
    return type;
  }
};


// Type functions
inline Type::Type(TensorType *tensor)
    : _kind(Tensor), tensor(tensor), ptr(tensor) {}
inline Type::Type(ElementType *element)
    : _kind(Element), element(element), ptr(element) {}
inline Type::Type(SetType *set)
    : _kind(Set), set(set), ptr(set) {}
inline Type::Type(TupleType *tuple)
    : _kind(Tuple), tuple(tuple), ptr(tuple) {}

inline bool isScalarTensor(Type type) {
  return type.kind()==Type::Tensor && type.toTensor()->order() == 0;
}

bool operator==(const Type &, const Type &);
bool operator!=(const Type &, const Type &);

bool operator==(const ScalarType &, const ScalarType &);
bool operator==(const TensorType &, const TensorType &);
bool operator==(const ElementType &, const ElementType &);
bool operator==(const SetType &, const SetType &);
bool operator==(const TupleType &, const TupleType &);

bool operator!=(const ScalarType &, const ScalarType &);
bool operator!=(const TensorType &, const TensorType &);
bool operator!=(const ElementType &, const ElementType &);
bool operator!=(const SetType &, const SetType &);
bool operator!=(const TupleType &, const TupleType &);

std::ostream &operator<<(std::ostream &os, const Type &);
std::ostream &operator<<(std::ostream &os, const ScalarType &);
std::ostream &operator<<(std::ostream &os, const TensorType &);
std::ostream &operator<<(std::ostream &os, const ElementType &);
std::ostream &operator<<(std::ostream &os, const SetType &);
std::ostream &operator<<(std::ostream &os, const TupleType &);

}} // namespace simit::ir

#endif
