/**
 *  @file header.c
 *  @brief drumlogue SDK unit header - FM Kickdrum
 *
 *  Copyright (c) 2023 Your Name
 *
 */

#include "unit.h"  // Note: Include common definitions for all units

// ---- Unit header definition --------------------------------------------------------------------

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),                  // leave as is, size of this header
    .target = UNIT_TARGET_PLATFORM | k_unit_module_synth,  // target platform and module for this unit
    .api = UNIT_API_VERSION,                               // logue sdk API version against which unit was built
    .dev_id = 0x21435587U,                                // developer identifier
    .unit_id = 0x0001U,                                    // Id for this unit, should be unique within the scope of a given dev_id
    .version = 0x00010000U,                                // This unit's version: major.minor.patch (major<<16 minor<<8 patch)
    .name = "KICKZ Drum",                                 // Name for this unit, will be displayed on device
    .num_presets = 5,                                      // Number of internal presets this unit has
    .num_params = 23,                                      // Jetzt 23 Parameter
    .params = {
        // Format: min, max, center, default, type, fractional, frac. type, <reserved>, name

        // Page 1 - Basic parameters
        {40, 150, 40, 60, k_unit_param_type_hertz, 0, 0, 0, {"PITCH"}},
        {10, 500, 10, 150, k_unit_param_type_msec, 0, 0, 0, {"DECAY"}},
        {0, 100, 0, 50, k_unit_param_type_percent, 0, 0, 0, {"BODY"}},
        {0, 100, 0, 40, k_unit_param_type_percent, 0, 0, 0, {"DRIVE"}},
        
        // Page 2 - Envelope parameters
        {0, 50, 0, 2, k_unit_param_type_msec, 0, 0, 0, {"ATTACK"}},
        {10, 1000, 10, 300, k_unit_param_type_msec, 0, 0, 0, {"RELEASE"}},
        {0, 100, 0, 50, k_unit_param_type_percent, 0, 0, 0, {"P.CURVE"}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}}, // Leerparameter
        
        // Page 3 - Click parameters (NEU)
        {0, 100, 0, 50, k_unit_param_type_percent, 0, 0, 0, {"CLICK LVL"}},
        {50, 400, 100, 200, k_unit_param_type_hertz, 0, 0, 0, {"CLICK FREQ"}},
        {1, 100, 10, 20, k_unit_param_type_msec, 0, 0, 0, {"CLICK DCY"}},
        {0, 100, 0, 60, k_unit_param_type_percent, 0, 0, 0, {"CLICK TONE"}},
        
        // Page 4 - Filter parameters (NEU)
        {0, 1, 0, 0, k_unit_param_type_onoff, 0, 0, 0, {"FILTER ON"}},
        {0, 100, 0, 70, k_unit_param_type_percent, 0, 0, 0, {"CUTOFF"}},
        {0, 100, 0, 20, k_unit_param_type_percent, 0, 0, 0, {"RESONANCE"}},
        {0, 1, 0, 0, k_unit_param_type_onoff, 0, 0, 0, {"24DB MODE"}},
        
        // Page 5 - OSC2 parameters
        {0, 1, 0, 1, k_unit_param_type_onoff, 0, 0, 0, {"OSC2 ON"}},
        {0, 4, 0, 0, k_unit_param_type_bitmaps, 0, 0, 0, {"OSC2 WAVE"}},
        {5, 60, 5, 20, k_unit_param_type_none, 1, 1, 0, {"OSC2 PITCH"}},
        {0, 100, 0, 50, k_unit_param_type_percent, 0, 0, 0, {"OSC2 LEVEL"}},
        
        // Page 6 - FM parameters
        {0, 100, 0, 0, k_unit_param_type_percent, 0, 0, 0, {"FM AMOUNT"}},
        {5, 50, 5, 20, k_unit_param_type_none, 1, 1, 0, {"FM RATIO"}},
        {10, 500, 10, 100, k_unit_param_type_msec, 0, 0, 0, {"OSC2 DECAY"}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}}}};
