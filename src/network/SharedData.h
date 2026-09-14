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

class SharedData {
public:
    SharedData() = default;

    GreenhouseData get() {
        mutex.lock();
        GreenhouseData copy = data;
        mutex.unlock();
        return copy;
    }

    void set_measurements(float co2_ppm, float humidity, float temperature, float fan_percent) {
        mutex.lock();
        data.co2_ppm = co2_ppm;
        data.humidity = humidity;
        data.temperature = temperature;
        data.fan_percent = fan_percent;
        data.valid = true;
        mutex.unlock();
    }

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
