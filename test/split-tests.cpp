#include "test.h"
#include "test_tensors.h"

#include "taco/util/name_generator.h"
#include "taco/tensor.h"

using namespace taco;

TEST(split, elmul) {
  TensorBase a("a", Float64(), {8}, Sparse);
  TensorBase b = d8a("b", Sparse);
  TensorBase c = d8b("c", Sparse);
  b.pack();
  c.pack();

  IndexVar i("i");
  IndexExpr mul = b(i) * c(i);
  a(i) = mul;

  IndexVar ileft("ileft"), iright("iright");
//  mul.splitOperator(i, ileft, iright);

  a.evaluate();

  Tensor<double> e("e", {8}, Sparse);
  e.insert({0}, 10.0);
  e.insert({2}, 60.0);
  e.pack();
  ASSERT_TENSOR_EQ(e,a);
}

/*
TEST(split, spmspm) {
  TensorBase A("A", Float(64), {3,3}, Format({Dense,Sparse}));
  TensorBase B = d33a("B", Format({Dense,Sparse}));
  TensorBase C = d33b("C", Format({Dense,Sparse}));
  B.pack();
  C.pack();

  IndexVar i, j, k;
  IndexExpr matmul = B(i,k) * C(k,j);
  A(i,j) = matmul;

  A.evaluate();

  std::cout << A << std::endl;
  Tensor<double> E("e", {3,3}, Format({Dense,Sparse}));
  E.insert({2,0}, 30.0);
  E.insert({2,1}, 180.0);
  E.pack();
  ASSERT_TENSOR_EQ(E,A);
}
*/
