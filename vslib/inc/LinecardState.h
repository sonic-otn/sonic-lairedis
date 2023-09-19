#pragma once

extern "C" {
#include "lai.h"
}

#include "LaiAttrWrap.h"
#include "LinecardConfig.h"
#include "OAEmulator.h"

#include "meta/Meta.h"

#include "swss/selectableevent.h"

#include <map>
#include <memory>
#include <thread>
#include <string>
#include <mutex>

namespace laivs
{
    class LinecardState
    {
        public:

            /**
             * @brief AttrHash key is attribute ID, value is actual attribute
             */
            typedef std::map<std::string, std::shared_ptr<LaiAttrWrap>> AttrHash;

            /**
             * @brief ObjectHash is map indexed by object type and then serialized object id.
             */
            typedef std::map<lai_object_type_t, std::map<std::string, AttrHash>> ObjectHash;

        public:

            LinecardState(
                    _In_ lai_object_id_t linecard_id,
                    _In_ std::shared_ptr<LinecardConfig> config);

            virtual ~LinecardState();

        public:

            void setMeta(
                    std::weak_ptr<laimeta::Meta> meta);

        public:

            lai_status_t getStatsExt(
                    _In_ lai_object_type_t obejct_type,
                    _In_ lai_object_id_t object_id,
                    _In_ uint32_t number_of_counters,
                    _In_ const lai_stat_id_t* counter_ids,
                    _In_ lai_stats_mode_t mode,
                    _Out_ lai_stat_value_t *counters);

        public:

            lai_object_id_t getLinecardId() const;

            void setIfNameToPortId(
                    _In_ const std::string& ifname,
                    _In_ lai_object_id_t port_id);

            void removeIfNameToPortId(
                    _In_ const std::string& ifname);

            lai_object_id_t getPortIdFromIfName(
                    _In_ const std::string& ifname) const;

            void setPortIdToTapName(
                    _In_ lai_object_id_t port_id,
                    _In_ const std::string& tapname);

            void removePortIdToTapName(
                    _In_ lai_object_id_t port_id);

            bool  getTapNameFromPortId(
                    _In_ const lai_object_id_t port_id,
                    _Out_ std::string& if_name);

        protected:

            void registerLinkCallback();

            void unregisterLinkCallback();

            void asyncOnLinkMsg(
                    _In_ int nlmsg_type,
                    _In_ struct nl_object *obj);

            std::shared_ptr<laimeta::Meta> getMeta();

        public: // TODO make private

            ObjectHash m_objectHash;

        protected:

            std::map<std::string, std::map<int, uint64_t>> m_countersMap;

            lai_object_id_t m_linecard_id;

        private : // tap device related objects

            std::map<lai_object_id_t, std::string> m_port_id_to_tapname;

            std::map<std::string, lai_object_id_t> m_ifname_to_port_id_map;

            uint64_t m_linkCallbackIndex;

            std::mutex m_mutex;

        protected:

            std::weak_ptr<laimeta::Meta> m_meta;

            std::shared_ptr<LinecardConfig> m_linecardConfig;

        public:

            std::set<std::string> m_otdrOidList;

        public:
            std::shared_ptr<laivs::OAEmulator> OAHandler;

    };
}
