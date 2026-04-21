#pragma once
using namespace std;

// LEDEffectBase
// 
// A helper class that implements the ILEDEffect interface.  

#include "interfaces.h"
#include "schedule.h"

class LEDEffectBase : public ILEDEffect
{
protected:
    string _name;
    string _type;
    shared_ptr<ISchedule> _ptrSchedule = nullptr;

public:
    LEDEffectBase(const string& name, const string& type = "LEDEffectBase") : _name(name), _type(type) {}

    virtual ~LEDEffectBase() = default;

    const string& Name() const override { return _name; }
    string Type() const override { return _type; }

    // Default implementation for Start does nothing
    void Start(ICanvas& canvas) override 
    {
    }

    // Default implementation for Update does nothing
    void Update(ICanvas& canvas, microseconds deltaTime) override 
    {
    }

    void SetSchedule(const shared_ptr<ISchedule> schedule) override
    {
        _ptrSchedule = schedule;
    }

    const shared_ptr<ISchedule> GetSchedule() const override
    {
        return _ptrSchedule;
    }
};

