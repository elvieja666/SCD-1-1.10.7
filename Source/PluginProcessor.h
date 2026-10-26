#pragma once
#include <atomic>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>

class PhantomAsymProcessor : public juce::AudioProcessor
{
public:
    PhantomAsymProcessor();
    ~PhantomAsymProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "rmd-1"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> outMeterDb { -100.0f };   // pico del sidechain en el ultimo bloque (dBFS), para el vumetro de la interfaz

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::atomic<float> *pDepth = nullptr, *pMode = nullptr, *pPolarity = nullptr, *pListen = nullptr, *pBypass = nullptr, *pOutput = nullptr;

    float controlState = 0.0f;
    bool cycleOn = false;        // el ciclo actual esta silenciado
    bool nextDuck = true;        // el proximo ciclo se silencia (alterna)
    bool wasPositive = false;    // el sidechain esta en su mitad positiva (detector de cycles)
    float scEnergy[2] = { 0.0f, 0.0f };   // energia suavizada de cada canal del sidechain
    int scLead = 0;                       // canal del sidechain que se usa para detectar ciclos
    bool dcHold = false;         // señal continua (sin cruces): no disparar ciclos hasta que cruce por cero
    float cyclePeak = 0.0f;      // peak del cycle en curso (|sidechain|)
    float prevPeak = 0.0f;       // peak del cycle anterior
    int quietCount = 0;
    float listenState = 0.0f;                 // 1 = Key Listen activo (suavizado)
    float bypassState = 1.0f;                 // 1 = efecto activo, 0 = bypass (suavizado)
    std::vector<uint8_t> activeHistory;       // para desplazar la fase de la ventana
    int histPos = 0;
    float periodSamples = 960.0f;             // periodo estimado del sidechain
    int samplesSinceRise = 0;                 // muestras desde el ultimo inicio de cycle
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhantomAsymProcessor)
};
