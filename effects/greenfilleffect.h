#pragma once
using namespace std;
using namespace std::chrono;

// GreenFillEffect
// 
// A simple effect that fills the entire canvas with green for testing purposes.

#include "../interfaces.h"
#include "../ledeffectbase.h"
#include "../pixeltypes.h"

class GreenFillEffect : public LEDEffectBase
{
public:

    GreenFillEffect(const string& name) : LEDEffectBase(name) 
    {
    }

    void Update(ICanvas& canvas, milliseconds deltaTime) override
    {
        canvas.Graphics().Clear(CHSV(HUE_GREEN));
    }
};
