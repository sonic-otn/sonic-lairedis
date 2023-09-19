#include <thread>
#include <chrono>
#include <inttypes.h>

#include "OAEmulator.h"
#include "swss/logger.h"
#include "meta/lai_serialize.h"
#include "EventPayloadNotification.h"
#include "lib/inc/NotificationLinecardStateChange.h"

#include <string.h>
#include <fstream>

using namespace laivs;
using namespace std;

OAEmulator::OAEmulator()
{
    object_type = LAI_OBJECT_TYPE_OA;

    SWSS_LOG_ENTER();

    createOAAttr();

    actual_gain_id = get_oa_stat_id_by_name("LAI_OA_STAT_ACTUAL_GAIN");
    actual_output_power_id = get_oa_stat_id_by_name("LAI_OA_STAT_OUTPUT_POWER_TOTAL");
    actual_input_power_id = get_oa_stat_id_by_name("LAI_OA_STAT_INPUT_POWER_TOTAL");
    actual_gain_tilt_id = get_oa_stat_id_by_name("LAI_OA_STAT_ACTUAL_GAIN_TILT");

    inputInHysteresis = false;
    outputInHysteresis = false;
    gainInHysteresis = false;
}

OAEmulator::~OAEmulator()
{
    SWSS_LOG_ENTER();
}


void OAEmulator::getOAStats(
    lai_stat_id_t counter_ids,
    lai_stat_value_t &counters,
    std::map<int, uint64_t> &localcounter)
{
    SWSS_LOG_ENTER();
    int countid = counter_ids;
    this->localcounters = localcounter;

    importOaFile(countid, counters);

    importOaAttributes(countid, counters);

    OutputsimulateOaStats(countid, counters);

    handleOaRangeAndAlarms(countid, counters);
}


void OAEmulator::createOAAttr()
{
    SWSS_LOG_ENTER();
    lai_attribute_t attr;
    std::ifstream inputFile("../../vslib/vsObjectConfFile/OA_create.txt");
    std::string attrStr;
    std::string attrName;

    while (inputFile >> attrName)
    {
        auto meta = lai_metadata_get_attr_metadata_by_attr_id_name(attrName.c_str());
        if (meta != NULL)
        {
            attr.id = meta->attrid;
            switch (meta->attrvaluetype)
            {
                case LAI_ATTR_VALUE_TYPE_BOOL:
                    inputFile >> attrStr;
                    attr.value.booldata = (attrStr == "true") ? true : false;
                    setObjectHash(&attr);
                    break;

                case LAI_ATTR_VALUE_TYPE_UINT8:
                    inputFile >> attr.value.u8;
                    setObjectHash(&attr);
                    break;

                case LAI_ATTR_VALUE_TYPE_INT8:
                    inputFile >> attr.value.s8;
                    setObjectHash(&attr);
                    break;

                case LAI_ATTR_VALUE_TYPE_UINT16:
                    inputFile >> attr.value.u16;
                    setObjectHash(&attr);
                    break;

                case LAI_ATTR_VALUE_TYPE_INT16:
                    inputFile >> attr.value.s16;
                    setObjectHash(&attr);
                    break;

                case LAI_ATTR_VALUE_TYPE_UINT32:
                    inputFile >> attr.value.u32;
                    setObjectHash(&attr);
                    break;

                case LAI_ATTR_VALUE_TYPE_INT32:
                    inputFile >> attr.value.s32;
                    setObjectHash(&attr);
                    break;

                case LAI_ATTR_VALUE_TYPE_UINT64:
                    inputFile >> attr.value.u64;
                    setObjectHash(&attr);
                    break;

                case LAI_ATTR_VALUE_TYPE_INT64:
                    inputFile >> attr.value.s64;
                    setObjectHash(&attr);
                    break;

                case LAI_ATTR_VALUE_TYPE_DOUBLE:
                    inputFile >> attr.value.d64;
                    setObjectHash(&attr);
                    break;

                case LAI_ATTR_VALUE_TYPE_CHARDATA:
                    inputFile >> attrStr;
                    strncpy(attr.value.chardata, attrStr.c_str(), sizeof(attr.value.chardata)-1);
                    attr.value.chardata[sizeof(attr.value.chardata) - 1] = '0';
                    setObjectHash(&attr);
                    break;
                default:
                    break;
            }
        }
    }
    const lai_attr_metadata_t* gain_meta = lai_metadata_get_attr_metadata_by_attr_id_name("LAI_OA_ATTR_TARGET_GAIN");
    const lai_attribute_t* gain_attr = attrHash["LAI_OA_ATTR_TARGET_GAIN"]->getAttr();
    checkBoundedAttrRange(gain_meta, gain_attr);
    const lai_attr_metadata_t* output_meta = lai_metadata_get_attr_metadata_by_attr_id_name("LAI_OA_ATTR_TARGET_OUTPUT_POWER");
    const lai_attribute_t* output_attr = attrHash["LAI_OA_ATTR_TARGET_OUTPUT_POWER"]->getAttr();
    checkBoundedAttrRange(output_meta, output_attr);
}


void OAEmulator::setOAAttr(
        const lai_attr_metadata_t* meta,
        const lai_attribute_t* attr)
{
    SWSS_LOG_ENTER();
    std::string attrname = meta->attridname;
    setObjectHash(attr);

    if (attrname == "LAI_OA_ATTR_TARGET_GAIN" || attrname == "LAI_OA_ATTR_TARGET_OUTPUT_POWER")
    {
        checkBoundedAttrRange(meta, attr);
    }
}

//-------------------------------------------------------------------------------------

void OAEmulator::importOaFile(
        int countid,
        lai_stat_value_t &counters)
{
    auto amp_mode_value = attrHash["LAI_OA_ATTR_AMP_MODE"]->getAttr()->value.s32;
    amp_type_name = lai_oa_amp_mode_t(amp_mode_value);
    std::ifstream inputFile("../../vslib/vsObjectConfFile/OA_input.txt");
    std::string statName;
    while (inputFile >> statName){
        auto t_meta = lai_metadata_get_stat_metadata_by_stat_id_name(statName.c_str());
        if (t_meta != NULL){
            int stat_id = t_meta->statid;
            if (countid == stat_id){
                inputFile >> counters.d64;
                localcounters[countid] = counters.d64;
            }
        }
    }
}

void OAEmulator::importOaAttributes(
        int countid,
        lai_stat_value_t &counters)
{
    SWSS_LOG_ENTER();
    if (countid == actual_gain_id){
        counters.d64 = attrHash["LAI_OA_ATTR_TARGET_GAIN"]->getAttr()->value.d64;
        localcounters[actual_gain_id] = counters.d64;
    } else if (countid == actual_output_power_id){
        counters.d64 = attrHash["LAI_OA_ATTR_TARGET_OUTPUT_POWER"]->getAttr()->value.d64;
        localcounters[actual_output_power_id] = counters.d64;
    }
}

void OAEmulator::OutputsimulateOaStats(
        int countid,
        lai_stat_value_t &counters)
{
    if (countid == actual_gain_id && amp_type_name == LAI_OA_AMP_MODE_CONSTANT_POWER){
        counters.d64 = actual_gain_in_constant_power_mode(localcounters[actual_output_power_id],
                                                         localcounters[actual_input_power_id]);
        gainInHysteresis = set_hysteresis_state("LAI_OA_ATTR_GAIN_LOW_THRESHOLD",
                                                "LAI_OA_ATTR_GAIN_LOW_HYSTERESIS",
                                                counters.d64,
                                                gainInHysteresis);
    }
    if (countid == actual_output_power_id && amp_type_name == LAI_OA_AMP_MODE_CONSTANT_GAIN){
        counters.d64 = actual_output_in_constant_gain_mode(localcounters[actual_gain_id],
                                                          localcounters[actual_gain_tilt_id],
                                                          localcounters[actual_input_power_id]);
        outputInHysteresis = set_hysteresis_state("LAI_OA_ATTR_OUTPUT_LOS_THRESHOLD",
                                                  "LAI_OA_ATTR_OUTPUT_LOS_HYSTERESIS",
                                                  counters.d64,
                                                  outputInHysteresis);
    }

}

void OAEmulator::handleOaRangeAndAlarms(
        int countid,
        lai_stat_value_t &counters)
{

    if (countid == actual_input_power_id){
        double min_input_power_value = attrHash["LAI_OA_ATTR_INPUT_LOW_THRESHOLD"]->getAttr()->value.d64;
        double actual_input_power_value = counters.d64;
        if (actual_input_power_value < min_input_power_value){
            counters.d64 = min_input_power_value;
            SWSS_LOG_INFO("Actual input power is too low");
        }
        inputInHysteresis = set_hysteresis_state("LAI_OA_ATTR_INPUT_LOS_THRESHOLD",
                                               "LAI_OA_ATTR_INPUT_LOS_HYSTERESIS",
                                               counters.d64,
                                               inputInHysteresis);
    }
    double result = 0.0;
    if (countid == actual_gain_id){
        result = check_stat_range("LAI_OA_ATTR_MAX_GAIN",
                                   "LAI_OA_ATTR_MIN_GAIN",
                                   "Actual gain",
                                    counters.d64);
    }
    if (countid == actual_output_power_id){
        result = check_stat_range("LAI_OA_ATTR_MAX_OUTPUT_POWER",
                                   "LAI_OA_ATTR_OUTPUT_LOW_THRESHOLD",
                                   "Actual output power",
                                   counters.d64);
    }
    int result_int = (int)result;
    if (result_int){
        counters.d64 = result;
        result = 0.0;
    }

    if (!inputInHysteresis && !outputInHysteresis && !gainInHysteresis){
        set_working_state(true);
    }
    else {
        set_working_state(false);
    }
}

//----------------------------------------------------------------------------

void OAEmulator::setObjectHash(
        const lai_attribute_t *attr)
{
    auto a = std::make_shared<LaiAttrWrap>(object_type, attr);
    attrHash[a->getAttrMetadata()->attridname] = a;
}

void OAEmulator::checkBoundedAttrRange(
        const lai_attr_metadata_t* meta,
        const lai_attribute_t* attr)
{
    std::string attrname = meta->attridname;
    auto amp_mode_value = attrHash["LAI_OA_ATTR_AMP_MODE"]->getAttr()->value.s32;
    amp_type_name = lai_oa_amp_mode_t(amp_mode_value);
    if (amp_type_name == LAI_OA_AMP_MODE_CONSTANT_GAIN && attrname == "LAI_OA_ATTR_TARGET_GAIN"){
        check_attr_range("LAI_OA_ATTR_MAX_GAIN", "LAI_OA_ATTR_MIN_GAIN", "LAI_OA_ATTR_TARGET_GAIN");
    }
    if (amp_type_name == LAI_OA_AMP_MODE_CONSTANT_POWER && attrname == "LAI_OA_ATTR_TARGET_OUTPUT_POWER"){
        check_attr_range("LAI_OA_ATTR_MAX_OUTPUT_POWER", "LAI_OA_ATTR_OUTPUT_LOW_THRESHOLD", "LAI_OA_ATTR_TARGET_OUTPUT_POWER");
    }
}

//------------------------------------------------------------------------------------

void OAEmulator::check_attr_range(
        std::string max_name,
        std::string min_name,
        std::string detection_name)
{
    SWSS_LOG_ENTER();

    auto target_attr = attrHash[detection_name]->getAttr();

    double max_value = attrHash[max_name]->getAttr()->value.d64;
    double min_value = attrHash[min_name]->getAttr()->value.d64;
    double target_value = target_attr->value.d64;

    if (target_value > max_value){
        SWSS_LOG_INFO("%s is too large",
                      detection_name.c_str());
        set_alarm_info(true);
    }
    else if (target_value < min_value){
        SWSS_LOG_INFO("%s is too low",
                      detection_name.c_str());
        set_alarm_info(true);
    }
    else {
        set_alarm_info(false);
    }
}

int OAEmulator::get_oa_stat_id_by_name(std::string name)
{
    auto s_meta = lai_metadata_get_stat_metadata_by_stat_id_name(name.c_str());
    int index = s_meta->statid;
    return index;
}

double OAEmulator::check_stat_range(
        std::string max_name,
        std::string min_name,
        std::string stat_name,
        double stat_value)
{
    SWSS_LOG_ENTER();
    double max_value = attrHash[max_name]->getAttr()->value.d64;
    double min_value = attrHash[min_name]->getAttr()->value.d64;
    double target_value = stat_value;
    if (target_value > max_value){
        SWSS_LOG_INFO("%s is too large",
                      stat_name.c_str());
        return max_value;
    }
    else if (target_value < min_value){
        SWSS_LOG_INFO("%s is too low",
                      stat_name.c_str());
        return stat_value;
    }
    return 0.0;
}

double OAEmulator::actual_gain_in_constant_power_mode(
        double actual_output_power,
        double actual_input_power)
{
    SWSS_LOG_ENTER();
    double calculate_gain;
    calculate_gain = actual_output_power - actual_input_power;
    return calculate_gain;
}

double OAEmulator::actual_output_in_constant_gain_mode(
        double actual_gain,
        double actual_gain_tilt,
        double actual_input_power)
{
    SWSS_LOG_ENTER();
    double calculate_output_power;
    calculate_output_power = actual_gain * actual_gain_tilt + actual_input_power;
    return calculate_output_power;
}

bool OAEmulator::set_hysteresis_state(
        std::string ThresholdName,
        std::string HysteresisName,
        double actualvalue,
        bool& in_hysteresis_name)
{
    SWSS_LOG_ENTER();
    double thresholdValue = attrHash[ThresholdName]->getAttr()->value.d64;
    double hysteresisValue = attrHash[HysteresisName]->getAttr()->value.d64;
    if (!in_hysteresis_name && actualvalue < thresholdValue) {
        in_hysteresis_name = true;
    }
    else if (in_hysteresis_name && actualvalue >= thresholdValue + hysteresisValue) {
            in_hysteresis_name = false;
    }
    return in_hysteresis_name;
}

void OAEmulator::set_working_state(bool state){
    const lai_attr_metadata_t *meta = lai_metadata_get_attr_metadata_by_attr_id_name("LAI_OA_ATTR_WORKING_STATE");
    lai_object_type_t objectType = meta->objecttype;
    lai_attribute_t new_attr;
    lai_attribute_t *ptr_new_attr;
    ptr_new_attr = &new_attr;
    new_attr.id = meta->attrid;
    if (state){
        new_attr.value.s32 = 0;
    }
    else{
        new_attr.value.s32 = 1;
    }
    auto a = std::make_shared<LaiAttrWrap>(objectType, ptr_new_attr);
    attrHash[a->getAttrMetadata()->attridname] = a;
}

void OAEmulator::set_alarm_info(bool g_alarm_change) {
    if (g_alarm_change) {
        OA_alarm_type = LAI_ALARM_TYPE_EDFA_GAIN_LOW;
        OA_alarm_info.time_created = (uint64_t) chrono::duration_cast<chrono::nanoseconds>(
                chrono::system_clock::now().time_since_epoch()).count();
        string text = "rx los";
        OA_alarm_info.text.count = text.size();
        OA_alarm_info.text.list = (int8_t *) text.c_str();

        OA_alarm_info.resource_oid = 0x20000000e;
        OA_alarm_info.severity = LAI_ALARM_SEVERITY_MAJOR;

        OA_alarm_info.status = LAI_ALARM_STATUS_ACTIVE;
    } else {
        OA_alarm_type = LAI_ALARM_TYPE_EDFA_GAIN_LOW;
        OA_alarm_info.status = LAI_ALARM_STATUS_INACTIVE;
    }
        g_alarm_change = false;
}
