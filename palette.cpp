#include "palette.h"
#include "pixeltypes.h"

// Palette.cpp
//
// Contains the implementation of the Palette class defined in palette.h.

const std::vector<CRGB> Palette::Rainbow = 
{
    CRGB(255, 0, 0),   // Red
    CRGB(255, 165, 0), // Orange
    CRGB(0, 255, 0),   // Green
    CRGB(0, 255, 255), // Cyan
    CRGB(0, 0, 255),   // Blue
    CRGB(128, 0, 128)  // Purple
};

const std::vector<CRGB> Palette::ChristmasLights = 
{
    CRGB::Red,    // Red
    CRGB::Green,  // Green
    CRGB::Blue,   // Blue
    CRGB::Orange, // Orange
    CRGB::Purple  // Purple
};

const std::vector<CRGB> Palette::RainbowStripes = 
{
    CRGB::Black, 
    CRGB::Red,
    CRGB::Black, 
    CRGB::Orange,   
    CRGB::Black, 
    CRGB::Yellow,   
    CRGB::Black, 
    CRGB::Green,
    CRGB::Black, 
    CRGB::Cyan,
    CRGB::Black, 
    CRGB::Blue,
    CRGB::Black, 
    CRGB::Purple,   
    CRGB::Black, 
    CRGB::Green
};