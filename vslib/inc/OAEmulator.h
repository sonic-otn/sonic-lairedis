#pragma once

#include <thread>
#include "LaiAttrWrap.h"
#include "LinecardConfig.h"
#include "RealObjectIdManager.h"
#include "EventPayloadNetLinkMsg.h"

namespace laivs {
    class OAEmulator
    {
        public:

            typedef std::map<std::string, std::shared_ptr<LaiAttrWrap>> AttrHash;

        public:

            OAEmulator();

            virtual ~OAEmulator();

        public:

            void getOAStats(
                    lai_stat_id_t counter_ids,
                    lai_stat_value_t &counters,
                    std::map<int, uint64_t> &localcounter);

            void createOAAttr();

            void setOAAttr(
                    const lai_attr_metadata_t* meta,
                    const lai_attribute_t* attr);

            void getOAAttr();


        public:

//            std::string serializedObjectId;
            lai_object_type_t object_type;


            std::map<std::string, std::shared_ptr<LaiAttrWrap>> attrHash;
            std::map<int, uint64_t> localcounters;

            lai_alarm_type_t OA_alarm_type = LAI_ALARM_TYPE_MAX;
            lai_alarm_info_t OA_alarm_info;

        private:

            lai_oa_amp_mode_t amp_type_name;
            
            int actual_gain_id;
            int actual_output_power_id;
            int actual_input_power_id;
            int actual_gain_tilt_id;

            bool inputInHysteresis;
            bool outputInHysteresis;
            bool gainInHysteresis;


        private:

            void importOaFile(
                    int countid,
                    lai_stat_value_t &counters);

            void importOaAttributes(
                    int countid,
                    lai_stat_value_t &counters);

            void OutputsimulateOaStats(
                    int countid,
                    lai_stat_value_t &counters);

            void handleOaRangeAndAlarms(
                    int countid,
                    lai_stat_value_t &counters);

        private:

            void setObjectHash(const lai_attribute_t *attr);

            void checkBoundedAttrRange(
                    const lai_attr_metadata_t* meta,
                    const lai_attribute_t* attr);

        private:

            int get_oa_stat_id_by_name(std::string name);

            void check_attr_range(
                    std::string max_name,
                    std::string min_name,
                    std::string detection_name);

            double check_stat_range(
                    std::string max_name,
                    std::string min_name,
                    std::string stat_name,
                    double stat_value);

            double actual_gain_in_constant_power_mode(
                    double actual_output_power,
                    double actual_input_power);

            double actual_output_in_constant_gain_mode(
                    double actual_gain,
                    double actual_gain_tilt,
                    double actual_input_power);

            bool set_hysteresis_state(
                    std::string ThresholdName,
                    std::string HysteresisName,
                    double actualvalue,
                    bool& in_hysteresis_name);

            void set_working_state(bool state);

            void set_alarm_info(bool g_alarm_change);
    };
}