#include <functional>
#include <iostream>
#include <memory>
#include <utility>

class MoveOnlyFunction { // 对外使用的接口
  struct Interface {  // 内部操作接口
    virtual int call(int x) = 0;
    virtual ~Interface() = default;
  };

  template<typename F>
  struct Model : Interface {  // 负责存储可调用对象
    F fn;
    explicit Model(F f) : fn(std::move(f)) {}
    int call(int x) override { return fn(x); }
  };

  std::unique_ptr<Interface> impl_;

public:
  template<typename F>
  explicit MoveOnlyFunction(F f)
    : impl_(std::make_unique<Model<F>>(std::move(f))) {}

  int operator()(int x) { return impl_->call(x); }
  ~MoveOnlyFunction() = default;

  MoveOnlyFunction(const MoveOnlyFunction&) = delete;
  MoveOnlyFunction& operator=(const MoveOnlyFunction&) = delete;
  MoveOnlyFunction(MoveOnlyFunction&&) noexcept = default;
  MoveOnlyFunction& operator=(MoveOnlyFunction&&) noexcept = default;
};

int add_one(int x) { return x+1; }

struct Multiply {
  int factor;
  int operator()(int x) { return x * factor; }
};

struct Calculator {
  int offset;

  static int square(int x) { return x * x; }

  int add(int x) const { return x + offset; }
};

int main() {
// 普通函数
MoveOnlyFunction a(add_one);

// 函数指针
int (*ptr)(int) = &add_one;
MoveOnlyFunction b(ptr);

// 函数对象
MoveOnlyFunction c(Multiply(3));

// 静态成员函数
MoveOnlyFunction d(&Calculator::square);

// 非静态成员
Calculator calculator(5);
MoveOnlyFunction e(std::bind(&Calculator::add, calculator, std::placeholders::_1));

// 带捕获的lambda
MoveOnlyFunction f([offset = 6](int x){ return x + offset; });

// 不可复制的lambda
MoveOnlyFunction g([p = std::make_unique<int>(7)](int x){ return x + *p; });

// 移动构造，g变空
MoveOnlyFunction h(std::move(g));

// 移动赋值：销毁a原来的目标，接管h的目标
a = std::move(h);

// 下面两行不能编译，因为禁止复制
// MoveOnlyFunction copied(a);
// b = a;
}
