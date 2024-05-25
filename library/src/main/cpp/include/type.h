#pragma once

#include <type_traits>

template <typename P>
struct return_type_trait;

template <typename Return, typename... Args>
struct return_type_trait<Return (*)(Args...)> {
  using type = Return;
};

template <typename P>
struct return_type_object_trait;

template <typename Object, typename Return, typename... Args>
struct return_type_object_trait<Return (Object::*)(Args...)> {
  using type = Return;
};

template <typename P>
struct member_type_trait;

template <typename Type, typename Object>
struct member_type_trait<Type Object::*> {
  using type = Type;
};

template <unsigned int N, typename Object>
struct member_type_trait<char (Object::*)[N]> {
  using type = const char *;
};

template <unsigned int N, typename Object>
struct member_type_trait<const char (Object::*)[N]> {
  using type = const char *;
};

template <typename P>
struct member_ref_type_trait;

template <typename Type, typename Object>
struct member_ref_type_trait<Type Object::*> {
  using type = typename std::add_lvalue_reference<Type>::type;
};
