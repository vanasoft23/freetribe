#include "dsp_common.h"
#include "effect.h"

const char *g_effect_name_str[NUM_EFFECTS] = {
    "Waveshaper",
    "Overdrive",
    "Fuzz",
    "Glitch",
    "Flanger",
    "",
    "Chorus",
    "Phaser",
    "Delay 1/4",
    "Delay 3/16",
    "Delay 1/8",
    "Delay 1/16",
    "Dense'verb",
    "Disso'verb",
    "Limiter",
    "OTT",
};

const char *g_effect_param_str[NUM_EFFECTS][2] = {
    { "Intensity"   , "HPF Freq"   },
    { "Intensity"   , "BPF Freq"   },
    { "Intensity"   , "HPF Freq"   },
    { "Slice Length", "Randomness" },
    { "Time"        , "Intensity"  },
    { ""            , ""           },
    { "Time"        , "Intensity"  },
    { "Frequency"   , "Feedback"   },
    { "Dry/Wet"     , "Feedback"   },
    { "Dry/Wet"     , "Feedback"   },
    { "Dry/Wet"     , "Feedback"   },
    { "Dry/Wet"     , "Feedback"   },
    { "Dry/Wet"     , "Decay"      },
    { "Dry/Wet"     , "Decay"      },
    { "Threshold"   , "Release"    },
    { "Intensity"   , "Time"       },
};

