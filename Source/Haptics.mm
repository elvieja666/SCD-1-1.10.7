#include "Haptics.h"

#if JUCE_MAC
#import <AppKit/AppKit.h>

void hapticTick()
{
    // "Level change": el clic corto que se siente al pasar por una marca. Solo se siente con el dedo sobre el trackpad.
    [[NSHapticFeedbackManager defaultPerformer] performFeedbackPattern: NSHapticFeedbackPatternLevelChange
                                                       performanceTime: NSHapticFeedbackPerformanceTimeNow];
}
#endif
