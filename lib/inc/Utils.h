#pragma once

extern "C" {
#include "lai.h"
}

namespace lairedis
{
    class Utils
    {
        private:

            Utils() = delete;
            ~Utils() = delete;

        public:

                /**
                 * @brief Clear OID values.
                 *
                 * For every OID attribute on list will set their oids to
                 * LAI_NULL_OBJECT_ID. This is handy, since when performing GET
                 * operation, user may not clear allocated LIST, and GET also serializes
                 * full list contents, so exact buffer will be deserialized on syncd side.
                 *
                 * This operation is more for a convenience use, but we should
                 * create serialize and deserialize special for GET operation.
                 */
                static void clearOidValues(
                        _In_ lai_object_type_t objectType,
                        _In_ uint32_t attrCount,
                        _Out_ lai_attribute_t *attrList);

                static void clearOidList(
                        _Out_ lai_object_list_t& list);
    };
}
