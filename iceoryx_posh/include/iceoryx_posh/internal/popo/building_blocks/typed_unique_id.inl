// Copyright (c) 2020 by Robert Bosch GmbH. All rights reserved.
// Copyright (c) 2022 by Apex.AI Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
#ifndef IOX_POSH_POPO_BUILDING_BLOCKS_TYPED_UNIQUE_ID_INL
#define IOX_POSH_POPO_BUILDING_BLOCKS_TYPED_UNIQUE_ID_INL

namespace iox
{
namespace popo
{
// start with 1 to prevent accidentally generating an invalid ID when unique roudi ID is 0
template <typename T>
std::atomic<uint64_t> TypedUniqueId<T>::globalIDCounter{1U};

template <typename T>
inline TypedUniqueId<T>::TypedUniqueId() noexcept
    : ThisType(cxx::newtype::internal::ProtectedConstructor,
               (static_cast<uint64_t>(internal::getUniqueRouDiId()) << UNIQUE_ID_BIT_LENGTH)
                   + ((globalIDCounter.fetch_add(1u, std::memory_order_relaxed) << ROUDI_ID_BIT_LENGTH)
                      >> ROUDI_ID_BIT_LENGTH))
{
    internal::finalizeSetUniqueRouDiId();

    if (globalIDCounter.load() >= (static_cast<uint64_t>(1u) << UNIQUE_ID_BIT_LENGTH))
    {
        errorHandler(Error::kPOPO__TYPED_UNIQUE_ID_OVERFLOW, nullptr, ErrorLevel::FATAL);
    }
}

template <typename T>
inline TypedUniqueId<T>::TypedUniqueId(InvalidId_t) noexcept
    /// we have to cast INVALID_UNIQUE_ID with static_cast<uint64_t> otherwise it will not link
    /// with gcc-7.x - gcc-10.x. Who knows why?!
    : ThisType(cxx::newtype::internal::ProtectedConstructor, static_cast<uint64_t>(INVALID_UNIQUE_ID))
{
    // finalizeSetUniqueRouDiId intentionally not called since the
}

template <typename T>
inline bool TypedUniqueId<T>::isValid() const noexcept
{
    return TypedUniqueId<T>(InvalidId) != *this;
}
} // namespace popo
} // namespace iox
#endif
