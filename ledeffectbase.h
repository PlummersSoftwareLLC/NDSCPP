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
    unique_ptr<reference_wrapper<const ISchedule>> _ptrSchedule = nullptr;

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

    void SetSchedule(const ISchedule & schedule) override
    {
        _ptrSchedule = make_unique<reference_wrapper<const ISchedule>>(schedule);
    }

    const ISchedule * GetSchedule() override
    {
        return &_ptrSchedule->get();
    }
};

