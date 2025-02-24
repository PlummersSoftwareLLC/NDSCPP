#pragma once
using namespace std;

#include <optional>
#include <string>
#include <cstdint>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>

#include "global.h"
#include "interfaces.h"

class Schedule : public ISchedule
{
public:

    Schedule() = default;
    ~Schedule() = default;

    // Setters
    void SetDaysOfWeek(uint8_t days) override { daysOfWeek = days; } 
    void SetStartTime(const string& time) override { startTime = time; }
    void SetStopTime(const string& time) override { stopTime = time; }
    void SetStartDate(const string& date) override { startDate = date; }
    void SetStopDate(const string& date) override { stopDate = date; }

    // Getters
    optional<uint8_t> GetDaysOfWeek()  const override { return daysOfWeek; }
    optional<string>  GetStartTime()   const override { return startTime; }
    optional<string>  GetStopTime()    const override { return stopTime; }
    optional<string>  GetStartDate()   const override { return startDate; }
    optional<string>  GetStopDate()    const override { return stopDate; }


    // Methods to manipulate individual days.
    // If the daysOfWeek is not yet set, it initializes it to 0.
    void AddDay(DayOfWeek day) override {
        if (!daysOfWeek.has_value()) {
            daysOfWeek = 0;
        }
        daysOfWeek = daysOfWeek.value() | day;
    }
    
    void RemoveDay(DayOfWeek day) override {
        if (daysOfWeek.has_value()) {
            daysOfWeek = daysOfWeek.value() & ~day;
        }
    }

    bool HasDay(DayOfWeek day) const override {
        return daysOfWeek.has_value() && ((daysOfWeek.value() & day) != 0);
    }

    // Determines if the schedule is currently active.
    // Optional fields are considered a match if not present.
    // For daysOfWeek, if not set, the schedule matches all days.

    bool IsActive() const override
    {
        // Get current time in system local time
        auto now = system_clock::now();
        time_t now_c = chrono::system_clock::to_time_t(now);
        tm *localTime = localtime(&now_c);

        // Check day-of-week: if daysOfWeek is set, today's bit must be on.
        // Note: localTime->tm_wday: Sunday == 0, Monday == 1, etc.
        if (daysOfWeek.has_value()) {
            uint8_t todayBit = 1 << localTime->tm_wday;
            if (!(daysOfWeek.value() & todayBit)) {
                return false;
            }
        }

        // Format current date and time as strings in "YYYY-MM-DD" and "HH:MM:SS" formats.
        ostringstream dateStream, timeStream;
        dateStream << put_time(localTime, "%Y-%m-%d");
        timeStream << put_time(localTime, "%H:%M:%S");
        string currentDate = dateStream.str();
        string currentTime = timeStream.str();

        // Check start and stop dates if set.
        if (startDate.has_value() && currentDate < startDate.value()) {
            return false;
        }
        if (stopDate.has_value() && currentDate > stopDate.value()) {
            return false;
        }

        // Check start and stop times if set.
        if (startTime.has_value() && currentTime < startTime.value()) {
            return false;
        }
        if (stopTime.has_value() && currentTime > stopTime.value()) {
            return false;
        }

        return true;
    }

private:
    optional<uint8_t> daysOfWeek;  // Bitmask for days of week
    optional<string>  startTime;   // Format: "HH:MM:SS"
    optional<string>  stopTime;    // Format: "HH:MM:SS"
    optional<string>  startDate;   // Format: "YYYY-MM-DD"
    optional<string>  stopDate;    // Format: "YYYY-MM-DD"
};


// Global to_json and from_json functions for Schedule.
// Only properties that are set will be serialized.
inline void to_json(nlohmann::json &j, const ISchedule &s) 
{
    if (s.GetDaysOfWeek().has_value()) {
        j["daysOfWeek"] = s.GetDaysOfWeek().value();
    }
    if (s.GetStartTime().has_value()) {
        j["startTime"] = s.GetStartTime().value();
    }
    if (s.GetStopTime().has_value()) {
        j["stopTime"] = s.GetStopTime().value();
    }
    if (s.GetStartDate().has_value()) {
        j["startDate"] = s.GetStartDate().value();
    }
    if (s.GetStopDate().has_value()) {
        j["stopDate"] = s.GetStopDate().value();
    }
}

inline void from_json(const nlohmann::json &j, shared_ptr<ISchedule> &s) 
{
    s = make_shared<Schedule>();

    if (j.contains("daysOfWeek")) {
        s->SetDaysOfWeek(j.at("daysOfWeek").get<uint8_t>());
    }
    if (j.contains("startTime")) {
        s->SetStartTime(j.at("startTime").get<string>());
    }
    if (j.contains("stopTime")) {
        s->SetStopTime(j.at("stopTime").get<string>());
    }
    if (j.contains("startDate")) {
        s->SetStartDate(j.at("startDate").get<string>());
    }
    if (j.contains("stopDate")) {
        s->SetStopDate(j.at("stopDate").get<string>());
    }
}
