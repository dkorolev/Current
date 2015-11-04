#ifndef CURRENT_REFLECTION_STRUCT_H
#define CURRENT_REFLECTION_STRUCT_H

#include <memory>
#include <type_traits>

#include "base.h"

#include "../Bricks/template/variadic_indexes.h"

namespace current {
namespace reflection {

template <typename INSTANTIATION_TYPE, typename T>
struct BaseTypeHelperImpl {
  struct type {};
};

template <typename T>
struct BaseTypeHelperImpl<DeclareFields, T> {
  typedef T type;
};

template <typename INSTANTIATION_TYPE, typename T>
using BaseTypeHelper = typename BaseTypeHelperImpl<INSTANTIATION_TYPE, T>::type;

// Fields declaration and counting.
template <typename INSTANTIATION_TYPE, typename T>
struct FieldImpl;

template <typename T>
struct FieldImpl<DeclareFields, T> {
  typedef T type;
};

template <typename T>
struct FieldImpl<CountFields, T> {
  // TODO: Read on padding.
  typedef CountFieldsImplementationType type;
};

template <typename INSTANTIATION_TYPE, typename T>
using Field = typename FieldImpl<INSTANTIATION_TYPE, T>::type;

// Check if `CURRENT_REFLECTION(f, Index<FieldType, i>)` can be called for i = [0, N).
template <typename T, int N>
struct CurrentStructFieldsConsistency {
  struct Dummy {};
  constexpr static bool CheckField(char) { return false; }
  constexpr static auto CheckField(int)
      -> decltype(T::CURRENT_REFLECTION(Dummy(), Index<FieldType, N>()), bool()) {
    return true;
  }
  constexpr static bool Check() { return CheckField(0) && CurrentStructFieldsConsistency<T, N - 1>::Check(); }
};

template <typename T>
struct CurrentStructFieldsConsistency<T, -1> {
  constexpr static bool Check() { return true; }
};

}  // namespace reflection
}  // namespace current

#define CURRENT_EXPAND_MACRO_2(x) x
#define CURRENT_EXPAND_MACRO(x) CURRENT_EXPAND_MACRO_2(x)

// Macros for structure definition.
#define CURRENT_STRUCT_SWITCH(_1, _2, F, ...) F
#define CURRENT_STRUCT(...) \
  CURRENT_STRUCT_SWITCH(__VA_ARGS__, CURRENT_STRUCT_DERIVED, CURRENT_STRUCT_NOT_DERIVED)(__VA_ARGS__)

#define CURRENT_STRUCT_HELPERS(s)                                                           \
  template <typename INSTANTIATION_TYPE>                                                    \
  struct CURRENT_STRUCT_IMPL_##s;                                                           \
  using s = CURRENT_STRUCT_IMPL_##s<::current::reflection::DeclareFields>;                  \
  template <typename T>                                                                     \
  struct CURRENT_REFLECTION_HELPER;                                                         \
  template <>                                                                               \
  struct CURRENT_REFLECTION_HELPER<s> {                                                     \
    constexpr static const char* name() { return #s; }                                      \
    constexpr static size_t index_base = __COUNTER__;                                       \
    typedef CURRENT_STRUCT_IMPL_##s<::current::reflection::CountFields> field_count_struct; \
  }

#define CURRENT_STRUCT_NOT_DERIVED(s)    \
  CURRENT_STRUCT_HELPERS(s);             \
  template <typename INSTANTIATION_TYPE> \
  struct CURRENT_STRUCT_IMPL_##s         \
      : CURRENT_REFLECTION_HELPER<s>,    \
        ::current::reflection::BaseTypeHelper<INSTANTIATION_TYPE, ::current::reflection::CurrentBaseType>

#define CURRENT_STRUCT_DERIVED(s, base)                                               \
  static_assert(std::is_base_of<::current::reflection::CurrentBaseType, base>::value, \
                #base " must be derived from `CurrentBaseType`.");                    \
  CURRENT_STRUCT_HELPERS(s);                                                          \
  template <typename INSTANTIATION_TYPE>                                              \
  struct CURRENT_STRUCT_IMPL_##s : CURRENT_REFLECTION_HELPER<s>, BaseTypeHelper<INSTANTIATION_TYPE, base>

#define CURRENT_FIELD_SWITCH(_1, _2, _3, F, ...) F
#define CURRENT_FIELD(...) \
  CURRENT_FIELD_SWITCH(__VA_ARGS__, CURRENT_FIELD_WITH_VALUE, CURRENT_FIELD_NO_VALUE)(__VA_ARGS__)

#define CURRENT_FIELD_NO_VALUE(type, name)                     \
  ::current::reflection::Field<INSTANTIATION_TYPE, type> name; \
  CURRENT_FIELD_REFLECTION(CURRENT_EXPAND_MACRO(__COUNTER__) - index_base - 1, type, name)

#define CURRENT_FIELD_WITH_VALUE(type, name, value)                    \
  ::current::reflection::Field<INSTANTIATION_TYPE, type> name = value; \
  CURRENT_FIELD_REFLECTION(CURRENT_EXPAND_MACRO(__COUNTER__) - index_base - 1, type, name)

#define CURRENT_FIELD_REFLECTION(idx, type, name)                                                              \
  template <class F>                                                                                           \
  static void CURRENT_REFLECTION(F&& f, ::current::reflection::Index<::current::reflection::FieldType, idx>) { \
    f(::current::reflection::TypeWrapper<type>());                                                             \
  }                                                                                                            \
  template <class F>                                                                                           \
  static void CURRENT_REFLECTION(F&& f, ::current::reflection::Index<::current::reflection::FieldName, idx>) { \
    f(#name);                                                                                                  \
  }                                                                                                            \
  template <class F>                                                                                           \
  static void CURRENT_REFLECTION(F&& f,                                                                        \
                                 ::current::reflection::Index<::current::reflection::FieldTypeAndName, idx>) { \
    f(::current::reflection::TypeWrapper<type>(), #name);                                                      \
  }                                                                                                            \
  template <class F>                                                                                           \
  void CURRENT_REFLECTION(F&& f, ::current::reflection::Index<::current::reflection::FieldValue, idx>) const { \
    f(name);                                                                                                   \
  }                                                                                                            \
  template <class F>                                                                                           \
  void CURRENT_REFLECTION(                                                                                     \
      F&& f, ::current::reflection::Index<::current::reflection::FieldNameAndImmutableValueReference, idx>)    \
      const {                                                                                                  \
    f(#name, name);                                                                                            \
  }                                                                                                            \
  template <class F>                                                                                           \
  void CURRENT_REFLECTION(                                                                                     \
      F&& f, ::current::reflection::Index<::current::reflection::FieldNameAndMutableValueReference, idx>) {    \
    f(#name, name);                                                                                            \
  }

#define CURRENT_STRUCT_IS_VALID(s)                                                                      \
  ::current::reflection::CurrentStructFieldsConsistency<s,                                              \
                                                        ::current::reflection::FieldCounter<s>::value - \
                                                            1>::Check()

namespace current {
namespace reflection {

template <typename T>
struct FieldCounter {
  static_assert(std::is_base_of<CurrentBaseType, T>::value,
                "Template argument must be derived from `CurrentBaseType`.");
  enum {
    value = (sizeof(typename T::template CURRENT_REFLECTION_HELPER<T>::field_count_struct) /
             sizeof(CountFieldsImplementationType))
  };
};

template <typename T, typename INDEX_TYPE>
struct EnumFields {
  static_assert(std::is_base_of<CurrentBaseType, T>::value,
                "Template argument must be derived from `CurrentBaseType`.");
  typedef bricks::variadic_indexes::generate_indexes<FieldCounter<T>::value> NUM_INDEXES;

  template <typename F>
  void operator()(const F& f) {
    static NUM_INDEXES all_indexes;
    Impl(f, all_indexes);
  }

 private:
  template <typename F, int N, int... NS>
  void Impl(const F& f, bricks::variadic_indexes::indexes<N, NS...>) {
    static bricks::variadic_indexes::indexes<NS...> remaining_indexes;
    T::CURRENT_REFLECTION(f, Index<INDEX_TYPE, N>());
    Impl(f, remaining_indexes);
  }

  template <typename F>
  void Impl(const F&, bricks::variadic_indexes::indexes<>) {}
};

}  // namespace reflection
}  // namespace current

#endif  // CURRENT_REFLECTION_STRUCT_H
