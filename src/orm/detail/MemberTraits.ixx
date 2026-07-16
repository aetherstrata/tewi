module tewi:member_traits;

import :type_helpers;

import std;
// ============================================================================
//  MemberPtrTraits - decompose pointer-to-member types
// ============================================================================

namespace tewi::detail
{
template <auto MP, typename... Ts>
struct mp_column;

template <auto MP>
struct mp_column<MP>
{
    using type = void;
};

template <auto MP, typename Col, typename... Rest>
struct mp_column<MP, Col, Rest...>
{
    static consteval bool match()
    {
        if constexpr (requires { Col::member == MP; })
        {
            return Col::member == MP;
        }
        else return false;
    }

    using type = std::conditional_t<match(), Col, typename mp_column<MP, Rest...>::type>;
};

// MemberPtrs... -> std::tuple<FieldType1, FieldType2, ...>
template <auto... MemberPtrs>
using MembersTuple =
    std::tuple<typename member_ptr<std::remove_cv_t<decltype(MemberPtrs)>>::FieldType...>;

// Extract the object type from a single member pointer type.
template <auto MP>
using ObjectOf = member_ptr<std::remove_cv_t<decltype(MP)>>::ObjectType;

// All MPs share the same object type - works for 1 or more MPs.
template <auto... MPs>
concept HomogeneousMemberPtrs =
    sizeof...(MPs) > 0 && (std::is_same_v<ObjectOf<MPs>, ObjectOf<firstOf<MPs...>>> && ...);

template <typename First, typename... Rest>
consteval bool unique_member_ptrs()
{
    if constexpr (sizeof...(Rest) == 0)
    {
        return true;
    }
    else
    {
        return ([]<typename Other>()
        {
            if constexpr (requires { First::member == Other::member; })
            {
                return First::member != Other::member;
            }
            else
            {
                return true;
            }
        }.template operator()<Rest>() && ...)
        && unique_member_ptrs<Rest...>();
    }
}

template <typename... Cols>
concept UniqueMemberPointers = unique_member_ptrs<Cols...>();

template <auto MP>
using FieldOf = member_ptr<decltype(MP)>::FieldType;

template <auto... MP>
using FieldsOf = std::tuple<FieldOf<MP>...>;

template <auto... MemberPtrs>
struct projection_result;

template <auto MP>
struct projection_result<MP>
{
    using type = FieldOf<MP>;
};

template <auto MP1, auto MP2, auto... Rest>
struct projection_result<MP1, MP2, Rest...>
{
    using type = std::tuple<FieldOf<MP1>, FieldOf<MP2>, FieldOf<Rest>...>;
};

template <auto... MemberPtrs>
using ProjectionResult = typename projection_result<MemberPtrs...>::type;
} // namespace tewi::detail
