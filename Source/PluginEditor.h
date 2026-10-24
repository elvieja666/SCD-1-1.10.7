#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

// Interfaz del SCD-1. Todo se dibuja en "unidades de diseno" (ventana de 1696 x 1044) y se escala a la mitad:
// en pantallas retina cada unidad de diseno es exactamente un pixel real.
class PhantomAsymEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PhantomAsymEditor (PhantomAsymProcessor&);
    ~PhantomAsymEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    PhantomAsymProcessor& proc;

    struct Impl;
    std::unique_ptr<Impl> impl;

    float meterShown = -100.0f;
    bool listenShown = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhantomAsymEditor)
};
