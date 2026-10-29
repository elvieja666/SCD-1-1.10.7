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
    std::atomic<float> peakHoldDb { -100.0f };   // pico mas alto del sidechain (dBFS), se queda hasta que el editor lo reinicia
    std::atomic<float> outMeterDb { -100.0f };   // pico del sidechain en el ultimo bloque (dBFS), para el vumetro de la interfaz

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::atomic<float> *pDepth = nullptr, *pMode = nullptr, *pFade = nullptr, *pListen = nullptr, *pBypass = nullptr, *pOutput = nullptr;

    float controlState = 0.0f;
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
    // Polarity: la ventana de silencio se corre programando sus flancos (abrir / cerrar) para un poco mas tarde
    struct Edge { int wait; bool open; int len; };   // wait = muestras hasta que arranca la rampa; len = duracion de la rampa (0 = salto)
    Edge edges[64] {};                         // flancos pendientes
    int numEdges = 0;
    float ctrl = 0.0f;                        // ventana de silencio (0 = abierta, 1 = cerrada), con la rampa del Fade ya aplicada
    float rampFrom = 0.0f, rampTo = 0.0f; int rampPos = 0, rampLen = 0;
    bool prepared = false;
    std::vector<float> audioDelay[2], keyDelay[2];   // retardo fijo de audio y key: la ganancia se calcula un poco antes de que llegue el audio
    int dlySize = 1, dlyPos = 0;
    bool wasPlaying = false;                  // estado del transporte en el bloque anterior
    juce::int64 expectedPos = -1;             // posicion (en muestras) que se esperaba al empezar este bloque; si no coincide, el transporte salto (loop)
    int samplesSinceFall = 0;                 // muestras desde el ultimo cruce descendente
    int samplesSinceRise = 0;                 // muestras desde el ultimo inicio de cycle
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhantomAsymProcessor)
};
