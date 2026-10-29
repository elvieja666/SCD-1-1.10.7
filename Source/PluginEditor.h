#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

// Interfaz del rmd-1. Todo se dibuja en "unidades de diseno" (arte de 1696 x 1044, recortado por el marco a 1648 x 996)
// y se escala por kScale (ver PluginEditor.cpp). Las imagenes estan hechas al doble de resolucion que la ventana.
class PhantomAsymEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PhantomAsymEditor (PhantomAsymProcessor&);
    ~PhantomAsymEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;

    struct Impl;   // (publico solo para que el .cpp pueda recorrer los controles)

private:
    void timerCallback() override;
    juce::String meterText() const;
    void updateFaderTip();

    PhantomAsymProcessor& proc;

    std::unique_ptr<Impl> impl;

    float scale = 321.0f / 996.0f;
    float meterShown = -100.0f;   // nivel mostrado en la barra (con caida suave)
    bool listenShown = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhantomAsymEditor)
};
