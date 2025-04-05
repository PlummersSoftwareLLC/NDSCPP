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
    shared_ptr<ISchedule> _ptrSchedule = nullptr;

public:
    LEDEffectBase(const string& name) : _name(name) {}

    virtual ~LEDEffectBase() = default;

    const string& Name() const override { return _name; }

    // Default implementation for Start does nothing
    void Start(ICanvas& canvas) override 
    {
    }

    // Default implementation for Update does nothing
    void Update(ICanvas& canvas, milliseconds deltaTime) override 
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

