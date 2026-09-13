#ifndef CO2_SHARED_DATA_H
#define CO2_SHARED_DATA_H

#include "Fmutex.h"

struct GreenhouseData {
    double co2_ppm      = 0;      // field1 - measured CO2 level
    double humidity     = 0;      // field2 - relative humidity (%)
    double temperature  = 0;      // field3 - temperature (C)
    double fan_percent  = 0;      // field4 - ventilation fan speed 0-100%
    double setpoint_ppm = 0;      // field5 - CO2 set point
    bool  valid         = false;  // true once at least one measurement exists
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

    void set_measurements(double co2_ppm, double humidity, double temperature, double fan_percent) {
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

    double get_setpoint() {
        mutex.lock();
        double sp = data.setpoint_ppm;
        mutex.unlock();
        return sp;
    }

private:
    GreenhouseData data;
    Fmutex mutex;
};

#endif // CO2_SHARED_DATA_H
