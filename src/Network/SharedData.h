#ifndef CO2_SHARED_DATA_H
#define CO2_SHARED_DATA_H

#include "Fmutex.h"

struct GreenhouseData {
    float co2_ppm      = 0;      // field1 - measured CO2 level
    float humidity     = 0;      // field2 - relative humidity (%)
    float temperature  = 0;      // field3 - temperature (C)
    float fan_percent  = 0;      // field4 - ventilation fan speed 0-100%
    float setpoint_ppm = 0;      // field5 - CO2 set point
    bool  valid        = false;  // true once at least one measurement exists
};

class Shared_data {
public:
    SharedData() = default;

    // --- Whole-snapshot access (used by the network task) ---
    GreenhouseData get() {
        mutex.lock();
        GreenhouseData copy = data;
        mutex.unlock();
        return copy;
    }

    // --- Controller task updates the measured environment ---
    void set_measurements(float co2, float rh, float temp, float fan) {
        mutex.lock();
        data.co2_ppm = co2;
        data.humidity_rh = rh;
        data.temperature = temp;
        data.fan_percent = fan;
        data.valid = true;
        mutex.unlock();
    }

    // --- Setpoint is read/written from several places (UI, cloud) ---
    void set_setpoint(float sp) {
        mutex.lock();
        data.setpoint_ppm = sp;
        mutex.unlock();
    }

    float get_setpoint() {
        mutex.lock();
        float sp = data.setpoint_ppm;
        mutex.unlock();
        return sp;
    }

private:
    GreenhouseData data;
    Fmutex mutex;
};

#endif // CO2_SHARED_DATA_H
