#include "NetMsgRegistrar.h"
#include "LinecardState.h"
#include "RealObjectIdManager.h"
#include "LinecardStateBase.h"
#include "OAEmulator.h"
#include "RealObjectIdManager.h"
#include "EventPayloadNetLinkMsg.h"

#include "swss/logger.h"

#include "meta/lai_serialize.h"

#include <netlink/route/link.h>
#include <netlink/route/addr.h>
#include <linux/if.h>

using namespace laivs;

#define VS_COUNTERS_COUNT_MSB (0x80000000)

LinecardState::LinecardState(
        _In_ lai_object_id_t linecard_id,
        _In_ std::shared_ptr<LinecardConfig> config):
    m_linecard_id(linecard_id),
    m_linkCallbackIndex(-1),
    m_linecardConfig(config)
{
    SWSS_LOG_ENTER();

    if (RealObjectIdManager::objectTypeQuery(linecard_id) != LAI_OBJECT_TYPE_LINECARD)
    {
        SWSS_LOG_THROW("object %s is not LINECARD, its %s",
                lai_serialize_object_id(linecard_id).c_str(),
                lai_serialize_object_type(RealObjectIdManager::objectTypeQuery(linecard_id)).c_str());
    }

    for (int i = LAI_OBJECT_TYPE_NULL; i < (int)LAI_OBJECT_TYPE_EXTENSIONS_MAX; ++i)
    {
        /*
         * Populate empty maps for each object to avoid checking if
         * objecttype exists.
         */

        m_objectHash[(lai_object_type_t)i] = { };
    }

    /*
     * Create linecard by default, it will require special treat on
     * creating.
     */

    m_objectHash[LAI_OBJECT_TYPE_LINECARD][lai_serialize_object_id(linecard_id)] = {};
    lai_attribute_t attr;
    
    attr.id = LAI_LINECARD_ATTR_SOFTWARE_VERSION;
    strncpy(attr.value.chardata, "1.1.1", sizeof(attr.value.chardata) - 1);
    auto a = std::make_shared<LaiAttrWrap>(LAI_OBJECT_TYPE_LINECARD, &attr);
    m_objectHash[LAI_OBJECT_TYPE_LINECARD][lai_serialize_object_id(linecard_id)][a->getAttrMetadata()->attridname] = a;

    attr.id = LAI_LINECARD_ATTR_OPER_STATUS;
    attr.value.s32 = LAI_OPER_STATUS_ACTIVE;
    auto b = std::make_shared<LaiAttrWrap>(LAI_OBJECT_TYPE_LINECARD, &attr);
    m_objectHash[LAI_OBJECT_TYPE_LINECARD][lai_serialize_object_id(linecard_id)][b->getAttrMetadata()->attridname] = b;
    attr.id = LAI_LINECARD_ATTR_UPGRADE_STATE;
    attr.value.s32 = LAI_LINECARD_UPGRADE_STATE_IDLE;
    auto c = std::make_shared<LaiAttrWrap>(LAI_OBJECT_TYPE_LINECARD, &attr);
    m_objectHash[LAI_OBJECT_TYPE_LINECARD][lai_serialize_object_id(linecard_id)][c->getAttrMetadata()->attridname] = c;

    if (m_linecardConfig->m_useTapDevice)
    {
        m_linkCallbackIndex = NetMsgRegistrar::getInstance().registerCallback(
                std::bind(&LinecardState::asyncOnLinkMsg, this, std::placeholders::_1, std::placeholders::_2));
    }

    if (m_linecardConfig->m_resourceLimiter)
    {
        SWSS_LOG_NOTICE("resource limiter is SET on linecard %s",
                lai_serialize_object_id(linecard_id).c_str());
    }
}

LinecardState::~LinecardState()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("begin");

    if (m_linecardConfig->m_useTapDevice)
    {
        NetMsgRegistrar::getInstance().unregisterCallback(m_linkCallbackIndex);

        // if unregister ended then no new messages will arrive for this class
        // so there is no need to protect this using mutex
    }

    SWSS_LOG_NOTICE("linecard %s",
            lai_serialize_object_id(m_linecard_id).c_str());

    SWSS_LOG_NOTICE("end");
}

void LinecardState::setMeta(
        std::weak_ptr<laimeta::Meta> meta)
{
    SWSS_LOG_ENTER();

    m_meta = meta;
}

lai_object_id_t LinecardState::getLinecardId() const
{
    SWSS_LOG_ENTER();

    return m_linecard_id;
}

void LinecardState::setIfNameToPortId(
        _In_ const std::string& ifname,
        _In_ lai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    m_ifname_to_port_id_map[ifname] = port_id;
}


void LinecardState::removeIfNameToPortId(
        _In_ const std::string& ifname)
{
    SWSS_LOG_ENTER();

    m_ifname_to_port_id_map.erase(ifname);
}

lai_object_id_t LinecardState::getPortIdFromIfName(
        _In_ const std::string& ifname) const
{
    SWSS_LOG_ENTER();

    auto it = m_ifname_to_port_id_map.find(ifname);

    if (it == m_ifname_to_port_id_map.end())
    {
        return LAI_NULL_OBJECT_ID;
    }

    return it->second;
}

void LinecardState::setPortIdToTapName(
        _In_ lai_object_id_t port_id,
        _In_ const std::string& tapname)
{
    SWSS_LOG_ENTER();

    m_port_id_to_tapname[port_id] = tapname;
}

void LinecardState::removePortIdToTapName(
        _In_ lai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    m_port_id_to_tapname.erase(port_id);
}

bool LinecardState::getTapNameFromPortId(
        _In_ const lai_object_id_t port_id,
        _Out_ std::string& if_name)
{
    SWSS_LOG_ENTER();

    if (m_port_id_to_tapname.find(port_id) != m_port_id_to_tapname.end())
    {
        if_name = m_port_id_to_tapname[port_id];

        return true;
    }

    return false;
}

void LinecardState::asyncOnLinkMsg(
        _In_ int nlmsg_type,
        _In_ struct nl_object *obj)
{
    SWSS_LOG_ENTER();

    switch (nlmsg_type)
    {
        case RTM_NEWLINK:
        case RTM_DELLINK:
            break;

        default:

        SWSS_LOG_WARN("unsupported nlmsg_type: %d", nlmsg_type);
        return;
    }

    struct rtnl_link *link = (struct rtnl_link *)obj;

    int             if_index = rtnl_link_get_ifindex(link);
    unsigned int    if_flags = rtnl_link_get_flags(link); // IFF_LOWER_UP and IFF_RUNNING
    const char*     if_name  = rtnl_link_get_name(link);

    SWSS_LOG_NOTICE("received %s ifname: %s, ifflags: 0x%x, ifindex: %d",
            (nlmsg_type == RTM_NEWLINK ? "RTM_NEWLINK" : "RTM_DELLINK"),
            if_name,
            if_flags,
            if_index);

    auto payload = std::make_shared<EventPayloadNetLinkMsg>(m_linecard_id, nlmsg_type, if_index, if_flags, if_name);

    m_linecardConfig->m_eventQueue->enqueue(std::make_shared<Event>(EVENT_TYPE_NET_LINK_MSG, payload));
}


lai_status_t LinecardState::getStatsExt(
        _In_ lai_object_type_t object_type,
        _In_ lai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const lai_stat_id_t* counter_ids,
        _In_ lai_stats_mode_t mode,
        _Out_ lai_stat_value_t *counters)
{
    SWSS_LOG_ENTER();

    bool perform_set = false;

    auto info = lai_metadata_get_object_type_info(object_type);

    bool enabled = false;

    auto meta = m_meta.lock();

    if (meta)
    {
        enabled = meta->meta_unittests_enabled();
    }
    else
    {
        SWSS_LOG_WARN("meta pointer expired");
    }

    if (enabled && (number_of_counters & VS_COUNTERS_COUNT_MSB ))
    {
        number_of_counters &= ~VS_COUNTERS_COUNT_MSB;

        SWSS_LOG_NOTICE("unittests are enabled and counters count MSB is set to 1, performing SET on %s counters (%s)",
                lai_serialize_object_id(object_id).c_str(),
                info->statenum->name);

        perform_set = true;
    }

    auto str_object_id = lai_serialize_object_id(object_id);

    auto mapit = m_countersMap.find(str_object_id);

    if (mapit == m_countersMap.end())
        m_countersMap[str_object_id] = { };

    auto& localcounters = m_countersMap[str_object_id];

    for (uint32_t i = 0; i < number_of_counters; ++i)
    {
        int32_t id = counter_ids[i];

        auto stat_metadata  = lai_metadata_get_stat_metadata(object_type, id);

        if (stat_metadata->statvaluetype == LAI_STAT_VALUE_TYPE_UINT64) {
            if (perform_set)
            {
                localcounters[ id ] = counters[i].u64;
            }
            else
            {
                auto it = localcounters.find(id);

                if (it == localcounters.end())
                {
                    // if counter is not found on list, just return 0
                    counters[i].u64 = 1;
                    localcounters[id] = 1;
                }
                else
                {
                    counters[i].u64 = it->second;
                }

                if (mode == LAI_STATS_MODE_READ_AND_CLEAR)
                {
                    localcounters[ id ] = 0;
                }
            }
        } else if (stat_metadata->statvaluetype == LAI_STAT_VALUE_TYPE_DOUBLE) {
            auto it = localcounters.find(id);
            if (it == localcounters.end())
            {
                localcounters[id] = 1;
                counters[i].d64 = 1.0;
            }
            else
            {
                counters[i].d64 = (double)(it->second);
            }
        }

        if (object_type == LAI_OBJECT_TYPE_OA)
        {
            OAHandler->getOAStats(counter_ids[i], counters[i], localcounters);
            localcounters = OAHandler->localcounters;
        }
    }

    return LAI_STATUS_SUCCESS;
}

std::shared_ptr<laimeta::Meta> LinecardState::getMeta()
{
    SWSS_LOG_ENTER();

    auto meta = m_meta.lock();

    if (!meta)
    {
        SWSS_LOG_WARN("meta pointer expired");
    }

    return meta;
}
