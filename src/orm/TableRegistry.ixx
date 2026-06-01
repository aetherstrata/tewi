export module tewi:registry;

import std;
// ============================================================================
//  TableRegistry  - maps Row types to their Table descriptors
// ============================================================================

export namespace tewi::detail
{
template <typename RowType>
struct TableRegistry
{
    using TableType = void;
};

template <typename RowType>
concept HasRegisteredTable = !std::is_void_v<typename TableRegistry<RowType>::TableType>;
} // namespace tewi::detail
