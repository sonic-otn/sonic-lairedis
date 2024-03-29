#include "Channel.h"

#include "lairedis.h"

#include "swss/logger.h"

using namespace lairedis;

Channel::Channel(
        _In_ Callback callback):
    m_callback(callback),
    m_responseTimeoutMs(LAI_REDIS_DEFAULT_SYNC_OPERATION_RESPONSE_TIMEOUT)
{
    SWSS_LOG_ENTER();

    // empty
}

Channel::~Channel()
{
    SWSS_LOG_ENTER();

    // empty
}

void Channel::setResponseTimeout(
        _In_ uint64_t responseTimeout)
{
    SWSS_LOG_ENTER();

    m_responseTimeoutMs = responseTimeout;
}

uint64_t Channel::getResponseTimeout() const
{
    SWSS_LOG_ENTER();

    return m_responseTimeoutMs;
}
