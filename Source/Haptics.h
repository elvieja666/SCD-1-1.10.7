#pragma once
#include <juce_core/juce_core.h>

// Vibracion del trackpad de la MacBook (como hace macOS al pasar por una marca). En otros sistemas no hace nada.
#if JUCE_MAC
void hapticTick();
#else
inline void hapticTick() {}
#endif
