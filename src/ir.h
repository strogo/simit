#ifndef SIMIT_IR_H
#define SIMIT_IR_H

#include <string>
#include <list>
#include <cstring>

#include <iostream>
#include "ir_printer.h"

#include "intrusive_ptr.h"
#include "uncopyable.h"
#include "types.h"
#include "indexvar.h"
#include "error.h"

namespace simit {
namespace ir {

namespace {
struct VarContent {
  std::string name;
  Type type;

  mutable long ref = 0;
  friend inline void aquire(VarContent *c) {++c->ref;}
  friend inline void release(VarContent *c) {if (--c->ref==0) delete c;}
};
}

class Var : public util::IntrusivePtr<VarContent> {
public:
  Var() : IntrusivePtr() {}
  Var(std::string name, Type type) : IntrusivePtr(new VarContent) {
    ptr->name = name;
    ptr->type = type;
  }

  const std::string &getName() const {return ptr->name;}
  const Type &getType() const {return ptr->type;}
};

inline std::ostream &operator<<(std::ostream &os, const Var &v) {
  return os << v.getName();
}


/// The base class of all nodes in the Simit Intermediate Representation
/// (Simit IR)
struct IRNode : private simit::interfaces::Uncopyable {
public:
  IRNode() : ref(0) {}
  virtual ~IRNode() {}
  virtual void accept(IRVisitor *visitor) const = 0;

private:
  mutable long ref;
  friend void aquire(const IRNode *node) {
    ++node->ref;
  }
  
  friend void release(const IRNode *node) {
    if (--node->ref == 0) {
      delete node;
    }
  }
};

struct ExprNodeBase : public IRNode {
  Type type;
};

struct StmtNodeBase : public IRNode {
};

template <typename T>
struct ExprNode : public ExprNodeBase {
  void accept(IRVisitor *v) const { v->visit((const T *)this); }
};

template <typename T>
struct StmtNode : public StmtNodeBase {
  void accept(IRVisitor *v) const { v->visit((const T *)this); }
};

struct IRHandle : util::IntrusivePtr<const IRNode> {
  IRHandle() : util::IntrusivePtr<const IRNode>() {}
  IRHandle(const IRNode *n) : util::IntrusivePtr<const IRNode>(n) {}
};

class Expr : public IRHandle {
public:
  Expr() : IRHandle() {}
  Expr(const ExprNodeBase *expr) : IRHandle(expr) {}
  Expr(const Var &var);

  Expr(int val);
  Expr(double val);

  Type type() const {return static_cast<const ExprNodeBase*>(ptr)->type;}

  void accept(IRVisitor *v) const {ptr->accept(v);}

  template <typename E> friend bool isa(Expr);
  template <typename E> friend const E* to(Expr);
};

template <typename E>
inline bool isa(Expr e) {
  return e.defined() && dynamic_cast<const E*>(e.ptr) != nullptr;
}

template <typename E>
inline const E* to(Expr e) {
  iassert(isa<E>(e)) << "Wrong Expr type";
  return static_cast<const E*>(e.ptr);
}

class Stmt : public IRHandle {
public:
  Stmt() : IRHandle() {}
  Stmt(const StmtNodeBase *stmt) : IRHandle(stmt) {}

  void accept(IRVisitor *v) const {ptr->accept(v);}

  template <typename S> friend bool isa(Stmt);
  template <typename S> friend const S* to(Stmt);
};

template <typename S>
inline bool isa(Stmt s) {
  return s.defined() && dynamic_cast<const S*>(s.ptr) != nullptr;
}

template <typename S>
inline const S* to(Stmt s) {
  iassert(isa<S>(s)) << "Wrong Expr type";
  return static_cast<const S*>(s.ptr);
}


/// A Simit function
namespace {
struct FuncContent {
  int kind;

  std::string name;
  std::vector<Var> arguments;
  std::vector<Var> results;
  Stmt body;

  std::vector<Var> temporaries;

  mutable long ref = 0;
  friend inline void aquire(FuncContent *c) {++c->ref;}
  friend inline void release(FuncContent *c) {if (--c->ref==0) delete c;}
};
}

class Func : public util::IntrusivePtr<FuncContent> {
public:
  enum Kind { Internal=0, Intrinsic=1 };

  Func() : IntrusivePtr() {}

  Func(const std::string &name, const std::vector<Var> &arguments,
       const std::vector<Var> &results, Stmt body, Kind kind=Internal)
      : IntrusivePtr(new FuncContent) {
    ptr->kind = kind;
    ptr->name = name;
    ptr->arguments = arguments;
    ptr->results = results;
    ptr->body = body;
  }

  Func(const std::string &name, const std::vector<Var> &arguments,
       const std::vector<Var> &results, Kind kind)
      : Func(name, arguments, results, Stmt(), kind) {
    iassert(kind != Internal);
  }

  /// Creates a new func with the same prototype as the given func, but with
  /// the new body
  Func(const Func &func, Stmt body)
      : Func(func.getName(), func.getArguments(), func.getResults(), body,
             func.getKind()) {}

  Func::Kind getKind() const {return static_cast<Kind>(ptr->kind);}
  std::string getName() const {return ptr->name;}
  const std::vector<Var> &getArguments() const {return ptr->arguments;}
  const std::vector<Var> &getResults() const {return ptr->results;}
  Stmt getBody() const {return ptr->body;}

  void accept(IRVisitor *visitor) const { visitor->visit(this); };
};


// Intrinsics:
class Intrinsics {
public:
  static Func mod;
  static Func sin;
  static Func cos;
  static Func atan2;
  static Func sqrt;
  static Func log;
  static Func exp;
  static Func norm;
  static Func solve;
  static std::map<std::string, Func> byName;
};


// Type compute functions
Type getFieldType(Expr elementOrSet, std::string fieldName);
Type getBlockType(Expr tensor);
Type getIndexExprType(std::vector<IndexVar> lhsIndexVars, Expr expr);


/// Represents a \ref Tensor that is defined as a constant or loaded.  Note
/// that it is only possible to define dense tensor literals.
struct Literal : public ExprNode<Literal> {
  void *data;
  size_t size;

  void cast(Type type);

  static Expr make(Type type) {
    return Literal::make(type, nullptr);
  }

  static Expr make(Type type, void *values) {
    iassert(type.isTensor()) << "only tensor literals are supported for now";

    size_t size = 0;
    switch (type.kind()) {
      case Type::Tensor: {
          const TensorType *ttype = type.toTensor();
          size = ttype->size() * ttype->componentType.bytes();
        break;
      }
      case Type::Element:
      case Type::Set:
      case Type::Tuple:
        iassert(false) << "Only tensor and scalar literals currently supported";
        break;
    }

    Literal *node = new Literal;
    node->type = type;
    node->size = size;
    node->data = malloc(node->size);
    if (values != nullptr) {
      memcpy(node->data, values, node->size);
    }
    else {
      memset(node->data, 0, node->size);
    } 
    return node;
  }

  static Expr make(Type type, std::vector<double> values) {
    iassert(isScalar(type) || type.toTensor()->size() == values.size());
    return Literal::make(type, values.data());
  }

  ~Literal() {free(data);}
};

struct VarExpr : public ExprNode<VarExpr> {
  Var var;

  static Expr make(Var var) {
    VarExpr *node = new VarExpr;
    node->type = var.getType();
    node->var = var;
    return node;
  }
};

/// Expression that reads a tensor from an element or set field.
struct FieldRead : public ExprNode<FieldRead> {
  Expr elementOrSet;
  std::string fieldName;

  static Expr make(Expr elementOrSet, std::string fieldName) {
    iassert(elementOrSet.type().isElement() || elementOrSet.type().isSet());
    FieldRead *node = new FieldRead;
    node->type = getFieldType(elementOrSet, fieldName);
    node->elementOrSet = elementOrSet;
    node->fieldName = fieldName;
    return node;
  }
};

/// Expression that reads a tensor from a tensor location.
struct TensorRead : public ExprNode<TensorRead> {
  Expr tensor;
  std::vector<Expr> indices;

  static Expr make(Expr tensor, std::vector<Expr> indices) {
    iassert(tensor.type().isTensor());
    for (auto &index : indices) {
      iassert(isScalar(index.type()) || index.type().isElement());
    }

    TensorRead *node = new TensorRead;
    node->type = getBlockType(tensor);
    node->tensor = tensor;
    node->indices = indices;
    return node;
  }
};

struct TupleRead : public ExprNode<TupleRead> {
  Expr tuple, index;

  static Expr make(Expr tuple, Expr index) {
    iassert(tuple.type().isTuple());
    TupleRead *node = new TupleRead;
    node->type = tuple.type().toTuple()->elementType;
    node->tuple = tuple;
    node->index = index;
    return node;
  }
};

/// An IndexRead retrieves an index from an edge set.  An example of an index
/// is the endpoints of the edges in the set.
struct IndexRead : public ExprNode<IndexRead> {
  Expr edgeSet;
  std::string indexName;

  static Expr make(Expr edgeSet, std::string indexName) {
    iassert(edgeSet.type().isSet());
    iassert(indexName == "endpoints")
        << "Only endpoints index supported for now";

    IndexRead *node = new IndexRead;
    node->type = TensorType::make(ScalarType(ScalarType::Int),
                                  {IndexDomain(IndexSet(edgeSet))});
    node->edgeSet = edgeSet;
    node->indexName = indexName;
    return node;
  }
};

/// TODO: Consider merging Length and IndexRead into e.g. PropertyRead.
struct Length : public ExprNode<Length> {
  IndexSet indexSet;

  static Expr make(IndexSet indexSet) {
    Length *node = new Length;
    node->type = TensorType::make(ScalarType(ScalarType::Int));
    node->indexSet = indexSet;
    return node;
  }
};

struct IndexedTensor : public ExprNode<IndexedTensor> {
  Expr tensor;
  std::vector<IndexVar> indexVars;

  static Expr make(Expr tensor, std::vector<IndexVar> indexVars) {
    iassert(tensor.type().isTensor()) << "Only tensors can be indexed.";
    iassert(indexVars.size() == tensor.type().toTensor()->order());
    for (size_t i=0; i < indexVars.size(); ++i) {
      iassert(indexVars[i].getDomain() == tensor.type().toTensor()->dimensions[i]
             && "IndexVar domain does not match tensordimension");
    }

    IndexedTensor *node = new IndexedTensor;
    node->type = TensorType::make(tensor.type().toTensor()->componentType);
    node->tensor = tensor;
    node->indexVars = indexVars;
    return node;
  }
};

struct IndexExpr : public ExprNode<IndexExpr> {
  std::vector<IndexVar> resultVars;
  Expr value;

  std::vector<IndexVar> domain() const;

  static Expr make(std::vector<IndexVar> resultVars, Expr value) {
    iassert(isScalar(value.type()));
    for (auto &idxVar : resultVars) {  // No reduction variables on lhs
      iassert(idxVar.isFreeVar());
    }
    IndexExpr *node = new IndexExpr;
    node->type = getIndexExprType(resultVars, value);
    node->resultVars = resultVars;
    node->value = value;
    return node;
  }
};

struct Call : public ExprNode<Call> {
  Func func;
  std::vector<Expr> actuals;

  static Expr make(Func func, std::vector<Expr> actuals) {
    iassert(func.getResults().size() == 1)
        << "only calls of function with one results is currently supported.";

    Call *node = new Call;
    node->type = func.getResults()[0].getType();
    node->func = func;
    node->actuals = actuals;
    return node;
  }
};

struct Neg : public ExprNode<Neg> {
  Expr a;

  static Expr make(Expr a) {
    iassert(isScalar(a.type()));

    Neg *node = new Neg;
    node->type = a.type();
    node->a = a;
    return node;
  }
};

struct Add : public ExprNode<Add> {
  Expr a, b;

  static Expr make(Expr a, Expr b) {
    iassert(isScalar(a.type()));
    iassert(a.type() == b.type());

    Add *node = new Add;
    node->type = a.type();
    node->a = a;
    node->b = b;
    return node;
  }
};

struct Sub : public ExprNode<Sub> {
  Expr a, b;

  static Expr make(Expr a, Expr b) {
    iassert(isScalar(a.type()));
    iassert(a.type() == b.type());

    Sub *node = new Sub;
    node->type = a.type();
    node->a = a;
    node->b = b;
    return node;
  }
};

struct Mul : public ExprNode<Mul> {
  Expr a, b;

  static Expr make(Expr a, Expr b) {
    iassert(isScalar(a.type()));
    iassert(a.type() == b.type());

    Mul *node = new Mul;
    node->type = a.type();
    node->a = a;
    node->b = b;
    return node;
  }
};

struct Div : public ExprNode<Div> {
  Expr a, b;

  static Expr make(Expr a, Expr b) {
    iassert(isScalar(a.type()));
    iassert(a.type() == b.type());

    Div *node = new Div;
    node->type = a.type();
    node->a = a;
    node->b = b;
    return node;
  }
};

struct Load : public ExprNode<Load> {
  Expr buffer;
  Expr index;

  static Expr make(Expr buffer, Expr index) {
    iassert(isScalar(index.type()));

    // TODO: Create a buffer/array type and assert that buffer is that type.
    //       Then get the component from the buffer.
    ScalarType ctype = buffer.type().toTensor()->componentType;

    Load  *node = new Load;
    node->type = TensorType::make(ctype);
    node->buffer = buffer;
    node->index = index;
    return node;
  }
};

// Statements
struct AssignStmt : public StmtNode<AssignStmt> {
  Var var;
  Expr value;

  static Stmt make(Var var, Expr value) {
    AssignStmt *node = new AssignStmt;
    node->var = var;
    node->value = value;
    return node;
  }
};

struct Map : public StmtNode<Map> {
  std::vector<Var> vars;
  Func function;
  Expr target, neighbors;
  ReductionOperator reduction;

  static Stmt make(std::vector<Var> vars, Func function,
                   Expr target, Expr neighbors=Expr(),
                   ReductionOperator reduction=ReductionOperator()) {
    iassert(target.type().isSet());
    iassert(!neighbors.defined() || neighbors.type().isSet());
    iassert(vars.size() == function.getResults().size());
    Map *node = new Map;
    node->vars = vars;
    node->function = function;
    node->target = target;
    node->neighbors = neighbors;
    node->reduction = reduction;
    return node;
  }
};

struct FieldWrite : public StmtNode<FieldWrite> {
  Expr elementOrSet;
  std::string fieldName;
  Expr value;

  static Stmt make(Expr elementOrSet, std::string fieldName, Expr value) {
    FieldWrite *node = new FieldWrite;
    node->elementOrSet = elementOrSet;
    node->fieldName = fieldName;
    node->value = value;
    return node;
  }
};

struct TensorWrite : public StmtNode<TensorWrite> {
  Expr tensor;
  std::vector<Expr> indices;
  Expr value;

  static Stmt make(Expr tensor, std::vector<Expr> indices, Expr value) {
    TensorWrite *node = new TensorWrite;
    node->tensor = tensor;
    node->indices = indices;
    node->value = value;
    return node;
  }
};

struct Store : public StmtNode<Store> {
  Expr buffer;
  Expr index;
  Expr value;

  static Stmt make(Expr buffer, Expr index, Expr value) {
    Store *node = new Store;
    node->buffer = buffer;
    node->index = index;
    node->value = value;
    return node;
  }
};

/// A `for` over a range.
struct ForRange : public StmtNode<ForRange> {
  Var var;
  Expr start;
  Expr end;
  Stmt body;
  
  static Stmt make(Var var, Expr start, Expr end, Stmt body) {
    ForRange *node = new ForRange;
    node->var = var;
    node->start = start;
    node->end = end;
    node->body = body;
    return node;
  }

};

struct ForDomain {
  enum Kind { IndexSet, Endpoints, Edges };
  Kind kind;

  /// An index set
  class IndexSet indexSet;

  /// A lookup in the index structures of an edge set
  Expr set;
  Var var;

  ForDomain() {}
  ForDomain(class IndexSet indexSet) : kind(IndexSet), indexSet(indexSet) {}
  ForDomain(Expr set, Var var, Kind kind) : kind(kind), set(set), var(var) {
    iassert(kind==Edges || kind==Endpoints);
  }
};

struct For : public StmtNode<For> {
  Var var;
  ForDomain domain;
  Stmt body;

  static Stmt make(Var var, ForDomain domain, Stmt body) {
    For *node = new For;
    node->var = var;
    node->domain = domain;
    node->body = body;
    return node;
  }
};

struct IfThenElse : public StmtNode<IfThenElse> {
  Expr condition;
  Stmt thenBody, elseBody;

  static Stmt make(Expr condition, Stmt thenBody, Stmt elseBody) {
    IfThenElse *node = new IfThenElse;
    node->condition = condition;
    node->thenBody = thenBody;
    node->elseBody = elseBody;
    return node;
  }
};

struct Block : public StmtNode<Block> {
  Stmt first, rest;

  static Stmt make(Stmt first, Stmt rest) {
    iassert(first.defined()) << "Empty block";
    Block *node = new Block;
    node->first = first;
    node->rest = rest;
    return node;
  }

  static Stmt make(std::vector<Stmt> stmts) {
    iassert(stmts.size() > 0) << "Empty block";
    Stmt node;
    for (size_t i=stmts.size(); i>0; --i) {
      node = Block::make(stmts[i-1], node);
    }
    return node;
  }
};

/// Empty statement that is convenient during code development.
struct Pass : public StmtNode<Pass> {
  static Stmt make() {
    Pass *node = new Pass;
    return node;
  }
};


// Operators
bool operator==(const Literal& l, const Literal& r);
bool operator!=(const Literal& l, const Literal& r);

}} // namespace simit::ir

#endif
