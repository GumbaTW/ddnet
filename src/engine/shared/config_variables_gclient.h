// This file can be included several times.

#ifndef MACRO_CONFIG_INT
#error "The config macros must be defined"
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Save, Desc) ;
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Save, Desc) ;
#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Save, Desc) ;
#endif

// Predictive input (visual only; does not change server input)
MACRO_CONFIG_INT(GcFastInput, gc_fast_input, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Uses input for prediction before the next tick")
MACRO_CONFIG_INT(GcFastInputAmount, gc_fast_input_amount, 20, 1, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How many milliseconds classic fast input will apply")
MACRO_CONFIG_INT(GcFastInputOthers, gc_fast_input_others, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply fast input to other tees")
MACRO_CONFIG_INT(GcFastInputMode, gc_fast_input_mode, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Fast input style (0 = classic ms, 1 = aggressive ticks)")
MACRO_CONFIG_INT(GcFastInputTicks, gc_fast_input_ticks, 100, 0, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Aggressive fast input amount in 0.01 ticks (100 = 1 tick)")
