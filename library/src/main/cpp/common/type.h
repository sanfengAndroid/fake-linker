#pragma once

template <typename P>
struct ReturnType;

template <typename Return, typename... Args>
struct ReturnType<Return (*)(Args...)> {
  using type = Return;
};

template <typename P>
struct ReturnTypeObject;

template <typename Object, typename Return, typename... Args>
struct ReturnTypeObject<Return (Object::*)(Args...)> {
  using type = Return;
};

template <typename P>
struct MemberType;

template <typename Type, typename Object>
struct MemberType<Type Object::*> {
  using type = Type;
};

template <unsigned int N, typename Object>
struct MemberType<char (Object::*)[N]> {
  using type = const char *;
};

template <unsigned int N, typename Object>
struct MemberType<const char (Object::*)[N]> {
  using type = const char *;
};

template <typename P>
struct MemberRefType;

template <typename Type, typename Object>
struct MemberRefType<Type Object::*> {
  using type = typename std::add_lvalue_reference<Type>::type;
};
