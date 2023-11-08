每个checker大概思路是，我存了一个map: key为cxxRecordDecl id， value是opertor set。
先遍历一遍AST，把遇到的opertor存到对应的map中，
在最后Checker::Run()这个函数中，遍历我存的map，看里面存的operator个数有没有达标。

每个checker 我参考了：
https://isocpp.org/wiki/faq/operator-overloading#overview-op-ov
https://en.cppreference.com/w/cpp/language/operators
https://github.com/google/googletest

根据google test代码 我省去了一些operator overload：
1. stream<< 和 stream>>。因为在阅读代码时，我发现这两个operator可以单独被overload
2. Logical operator, 看代码时没有发现，cppreference 和 https://isocpp.org/wiki/faq/operator-overloading#overview-op-ov
都建议 避免overload这些。

### Binary arithmetic operators
在cppreference中提到+和+=是相关联的，所以就按照这个思路来，每一个binary arithmetic operator
对应的map所存下的operator数量应为2.

### Comparison operators
根据cppreference 和 https://isocpp.org/wiki/faq/operator-overloading#overview-op-ov
```cpp
inline bool operator< (const X& lhs, const X& rhs) { /* do actual comparison */ }
inline bool operator> (const X& lhs, const X& rhs) { return rhs < lhs; }
inline bool operator<=(const X& lhs, const X& rhs) { return !(lhs > rhs); }
inline bool operator>=(const X& lhs, const X& rhs) { return !(lhs < rhs); }
```

```cpp
inline bool operator==(const X& lhs, const X& rhs) { /* do actual comparison */ }
inline bool operator!=(const X& lhs, const X& rhs) { return !(lhs == rhs); }
```
应如此分类

### Array subscript operator
在参考https://isocpp.org/wiki/faq/operator-overloading#overview-op-ov中提到：
array subscript 通常成对出现： 一个用来读取，一个用来写入。所以这个map里，
我期望存下的operator个数是2

### Bitwise arithmetic operators
与 binary arithmetic相似: &和&=相关，以此类推。

### Boolean negation operator
根据 cppreference 和 googletest 的代码。需要同时重载 bool 和 ！。
(这一条有争议，暂时没做)

I did not handle the case like:
```cpp
inline MyClass operator+(int lhs, MyClass rhs) {
    rhs += lhs;
    return rhs;
}
```
if the lhs is a builtin type
