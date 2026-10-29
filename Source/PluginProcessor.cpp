#include "PluginProcessor.h"
#ifndef SCD_TEST_BUILD   // las pruebas de audio (Tests/) se compilan sin la interfaz grafica
 #include "PluginEditor.h"
#endif

PhantomAsymProcessor::PhantomAsymProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                        .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "PARAMETERS", createLayout())
{
    pDepth = apvts.getRawParameterValue ("depthdb");   pMode   = apvts.getRawParameterValue ("mode");
    pPolarity = apvts.getRawParameterValue ("polarity"); pListen = apvts.getRawParameterValue ("listen");
    pBypass = apvts.getRawParameterValue ("bypass");   pOutput = apvts.getRawParameterValue ("output");
}

namespace
{
    // Depth y Output comparten el mismo formato de texto: dB con 2 decimales, y -60 dB se muestra como "-inf"
    juce::String dbToText (float v)
    {
        if (v <= -59.99f) return "-inf";
        return juce::String (v, 2);
    }

    float dbFromText (const juce::String& t)
    {
        const auto s = t.trim().toLowerCase();
        if (s.contains ("inf") || s.startsWith ("off") || s.startsWith ("sil") || s.startsWith ("mute"))
            return -60.0f;
        return juce::jlimit (-60.0f, 6.0f, s.getFloatValue());
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout PhantomAsymProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "depthdb", 1 }, "Depth",
        // Perilla: centro = 0 dB, todo a la derecha = +6 dB (lineal, 1 dB por marca), todo a la izquierda = -inf (-60 dB).
        // Mitad izquierda logaritmica: marcas en 0, -3, -6, -12, -24, -48 y -inf (interpolacion lineal en dB entre marcas).
        NormalisableRange<float> (-60.0f, 6.0f,
            [] (float, float, float p)
            {
                static const float db[7] = { -60.0f, -48.0f, -24.0f, -12.0f, -6.0f, -3.0f, 0.0f };
                p = jlimit (0.0f, 1.0f, p);
                if (p >= 0.5f) return (p - 0.5f) * 12.0f;                // 0 .. +6 dB
                const float x = p * 12.0f;                                // 0..6 marcas
                const int k = jmin (5, (int) x);
                return db[k] + (x - (float) k) * (db[k + 1] - db[k]);
            },
            [] (float, float, float v)
            {
                static const float db[7] = { -60.0f, -48.0f, -24.0f, -12.0f, -6.0f, -3.0f, 0.0f };
                if (v >= 6.0f) return 1.0f;
                if (v >= 0.0f) return 0.5f + v / 12.0f;
                if (v <= db[0]) return 0.0f;
                int k = 0;
                while (k < 5 && v > db[k + 1]) ++k;
                return ((float) k + (v - db[k]) / (db[k + 1] - db[k])) / 12.0f;
            },
            [] (float s, float e, float v) { return jlimit (s, e, std::round (v * 100.0f) / 100.0f); }),
        0.0f,
        AudioParameterFloatAttributes()
            .withLabel ("dB")
            .withStringFromValueFunction ([] (float v, int) { return dbToText (v); })
            .withValueFromStringFunction ([] (const String& t) { return dbFromText (t); })));

    params.push_back (std::make_unique<AudioParameterBool> (
        ParameterID { "bypass", 1 }, "Bypass", false));

    params.push_back (std::make_unique<AudioParameterBool> (
        ParameterID { "listen", 1 }, "SC Listen", false));

    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { "mode", 1 }, "Mode",
        StringArray { "Half", "Whole" }, 0));   // 0 = Half (default), 1 = Whole

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "polarity", 1 }, "Polarity",
        NormalisableRange<float> (0.0f, 180.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) -> String
            {
                if (v <= 0.05f)   return "Positive";
                if (v >= 179.95f) return "Negative";
                return String (v, 1) + String::charToString ((juce_wchar) 0x00b0);   // grados: "°"
            })
            .withValueFromStringFunction ([] (const String& t) -> float
            {
                const auto s = t.trim().toLowerCase();
                if (s.startsWith ("pos")) return 0.0f;
                if (s.startsWith ("neg")) return 180.0f;
                return jlimit (0.0f, 180.0f, s.getFloatValue());
            })));

    // Output: fader logaritmico (estilo consola). Marcas equiespaciadas en +6, 0, -6, -12, -24, -48 y -inf (= -60 dB en el parametro),
    // con interpolacion lineal en dB entre marcas: arriba mucha resolucion, abajo se comprime.
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "output", 1 }, "Output",
        NormalisableRange<float> (-60.0f, 6.0f,
            [] (float, float, float p)
            {
                static const float db[7] = { -60.0f, -48.0f, -24.0f, -12.0f, -6.0f, 0.0f, 6.0f };
                const float x = jlimit (0.0f, 1.0f, p) * 6.0f;
                const int k = jmin (5, (int) x);
                return db[k] + (x - (float) k) * (db[k + 1] - db[k]);
            },
            [] (float, float, float v)
            {
                static const float db[7] = { -60.0f, -48.0f, -24.0f, -12.0f, -6.0f, 0.0f, 6.0f };
                if (v <= db[0]) return 0.0f;
                if (v >= db[6]) return 1.0f;
                int k = 0;
                while (k < 5 && v > db[k + 1]) ++k;
                return ((float) k + (v - db[k]) / (db[k + 1] - db[k])) / 6.0f;
            },
            [] (float s, float e, float v) { return jlimit (s, e, std::round (v * 100.0f) / 100.0f); }),
        0.0f,
        AudioParameterFloatAttributes()
            .withLabel ("dB")
            .withStringFromValueFunction ([] (float v, int) { return dbToText (v); })
            .withValueFromStringFunction ([] (const String& t) { return dbFromText (t); })));

    return { params.begin(), params.end() };
}

void PhantomAsymProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    controlState = 0.0f;
    cycleOn = false;
    nextDuck = true;
    wasPositive = false;
    cyclePeak = 0.0f;
    prevPeak = 0.0f;
    quietCount = 0;
    activeHistory.assign ((size_t) (0.08 * sampleRate) + 2, 0);
    histPos = 0;
    periodSamples = (float) (sampleRate / 50.0);
    samplesSinceRise = 0;
    listenState = 0.0f;
    dcHold = false;
    scEnergy[0] = scEnergy[1] = 0.0f;
    scLead = 0;
    bypassState = pBypass->load() > 0.5f ? 0.0f : 1.0f;
}

bool PhantomAsymProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    if (out != layouts.getMainInputChannelSet())
        return false;

    const auto sc = layouts.getChannelSet (true, 1);
    if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void PhantomAsymProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int histSize = (int) activeHistory.size();
    if (histSize <= 0) return;   // el host llamo a processBlock antes de prepareToPlay: el audio pasa sin cambios

    auto mainBuf = getBusBuffer (buffer, true, 0);
    const int numCh      = mainBuf.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    // Sidechain: solo se usa si el bus esta habilitado Y el host entrego realmente esos canales en el buffer
    // (al cambiar el ruteo algunos hosts dejan el bus "habilitado" con un buffer mas chico: leerlo seria leer memoria ajena)
    auto* scBus = getBus (true, 1);
    const int scDeclared = scBus != nullptr && scBus->isEnabled() ? scBus->getNumberOfChannels() : 0;
    const bool hasSidechain = scDeclared > 0 && buffer.getNumChannels() >= numCh + scDeclared;
    auto scBuf = hasSidechain ? getBusBuffer (buffer, true, 1) : juce::AudioBuffer<float>();
    const int scCh = hasSidechain ? juce::jmin (scBuf.getNumChannels(), 2) : 0;

    const float* scIn[2] = { nullptr, nullptr };
    for (int c = 0; c < scCh; ++c) scIn[c] = scBuf.getReadPointer (c);
    const int outCh = juce::jmin (numCh, 2);   // el layout solo admite mono o estereo
    float* out[2] = { nullptr, nullptr };
    for (int c = 0; c < outCh; ++c) out[c] = mainBuf.getWritePointer (c);

    // Transporte: al arrancar la reproduccion o al saltar (loop, cambio de posicion) el detector empieza de cero,
    // asi el primer cycle del loop siempre es el mismo (en Whole no se invierte cual cycle suena y cual no en cada vuelta)
    {
        bool restart = false;
        juce::int64 nextPos = -1;
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
            {
                const bool playing = pos->getIsPlaying();
                const auto t = pos->getTimeInSamples();
                if (playing && ! wasPlaying) restart = true;
                else if (playing && t.hasValue() && expectedPos >= 0 && std::abs (*t - expectedPos) > 2) restart = true;
                wasPlaying = playing;
                if (playing && t.hasValue()) nextPos = *t + numSamples;
            }
        expectedPos = nextPos;
        if (restart)
        {
            cycleOn = false; nextDuck = true; wasPositive = false; dcHold = false;
            cyclePeak = prevPeak = 0.0f; quietCount = 0; samplesSinceRise = 0;
            std::fill (activeHistory.begin(), activeHistory.end(), 0);
        }
    }

    // Parametros (una lectura por bloque)
    const float depthDb      = pDepth->load();
    const float depth        = depthDb <= -59.99f ? 0.0f : juce::Decibels::decibelsToGain (depthDb);   // -inf .. +6 dB
    const bool  whole        = (int) pMode->load() == 1;                                                // Whole = cycle entero, Half = medio
    const float phaseDeg     = pPolarity->load();                                                       // 0 = Positive, 180 = Negative
    const float listenTarget = pListen->load() > 0.5f ? 1.0f : 0.0f;
    const float bypassTarget = pBypass->load() > 0.5f ? 0.0f : 1.0f;
    const float outDb        = pOutput->load();
    const float outGain      = outDb <= -59.99f ? 0.0f : juce::Decibels::decibelsToGain (outDb);        // -inf .. +6 dB

    // Valores fijos internos
    const float sr             = (float) currentSampleRate;
    const float hys            = juce::Decibels::decibelsToGain (-90.0f);   // solo para detectar cycles (practicamente sin threshold)
    const int   resetSamples   = (int) (0.06 * currentSampleRate);          // 60 ms sin señal = reinicio
    const int   noCycleSamples = (int) (0.2 * currentSampleRate);           // 200 ms sin ningun ciclo = señal continua
    const int   minPeriod      = (int) (0.002 * currentSampleRate);
    const float bypassCoef     = std::exp (-1.0f / (0.005f * sr));          // 5 ms (bypass y Key Listen)
    const float smoothCoef     = std::exp (-1.0f / (0.0002f * sr));         // 0.2 ms (ganancia): corte rapido para que la ventana siga a la onda sin retraso audible
    const float energyCoef     = 1.0f - std::exp (-1.0f / (0.02f * sr));    // 20 ms (energia de cada canal del sidechain)

    // Retardo de la ventana por Polarity; solo cambia cuando cambia el periodo estimado
    auto delayFor = [&] { return juce::jlimit (0, histSize - 1, (int) std::lround (phaseDeg / 360.0f * periodSamples)); };
    int delay = delayFor();
    float blockPeak = 0.0f;

    for (int s = 0; s < numSamples; ++s)
    {
        // ---- Señal de control: el canal del sidechain con mas energia (con su signo)
        // (no la suma L+R: se anula o bate con canales desfasados/desafinados, como en un reese; histeresis de 6 dB para no saltar entre canales parejos)
        float v[2] = { 0.0f, 0.0f };
        for (int c = 0; c < scCh; ++c)
        {
            v[c] = std::isfinite (scIn[c][s]) ? scIn[c][s] : 0.0f;   // NaN/Inf del host (p. ej. al cambiar el ruteo): se trata como silencio
            scEnergy[c] += energyCoef * (v[c] * v[c] - scEnergy[c]);
            blockPeak = juce::jmax (blockPeak, std::abs (v[c]));   // el vumetro muestra solo la entrada del sidechain
        }
        if (scCh == 2)
        {
            if (scLead == 0 && scEnergy[1] > 4.0f * scEnergy[0]) scLead = 1;
            else if (scLead == 1 && scEnergy[0] > 4.0f * scEnergy[1]) scLead = 0;
        }
        else scLead = 0;
        const float sc = v[scLead];

        // ---- Detector de cycles
        cyclePeak = juce::jmax (cyclePeak, std::abs (sc));

        if (dcHold && sc <= hys) dcHold = false;   // la señal volvio a cruzar por cero: se acepta de nuevo
        if (! wasPositive && ! dcHold && sc > hys)
        {
            // cruce ascendente = inicio de un cycle
            wasPositive = true;
            cycleOn   = nextDuck;
            nextDuck  = ! nextDuck;
            prevPeak  = cyclePeak;          // peak del cycle que acaba de terminar
            cyclePeak = std::abs (sc);

            // Periodo del sidechain (para el desplazamiento de fase de Polarity)
            if (samplesSinceRise > minPeriod && samplesSinceRise < resetSamples)
            {
                periodSamples = 0.5f * periodSamples + 0.5f * (float) samplesSinceRise;
                delay = delayFor();
            }
            samplesSinceRise = 0;
        }
        else if (wasPositive && sc < -hys)
        {
            wasPositive = false;
        }
        ++samplesSinceRise;

        // Sin señal: reinicia, cada golpe arranca igual
        if (std::abs (sc) < hys) ++quietCount; else quietCount = 0;

        // Señal sin ciclos (continua / DC puro): 200 ms sin cruces -> se libera el ducking y no se vuelve a disparar hasta que cruce por cero
        const bool noCycles = samplesSinceRise > noCycleSamples;
        if (noCycles)
        {
            dcHold = std::abs (sc) > hys;
            samplesSinceRise = noCycleSamples;
        }

        if (quietCount > resetSamples || noCycles)
        {
            cycleOn = false;
            nextDuck = true;   // el primer cycle de cada golpe siempre se silencia
            wasPositive = false;
            cyclePeak = prevPeak = 0.0f;
            quietCount = juce::jmin (quietCount, resetSamples);
        }

        // ---- Cantidad de efecto: sigue la amplitud del sidechain (0 dBFS = Depth completo, hacia -inf dB = sin efecto)
        const float amount = juce::jmin (1.0f, juce::jmax (prevPeak, cyclePeak));
        const bool  active = whole ? cycleOn : wasPositive;

        // ---- Polarity: desplaza donde empieza la ventana de silence dentro del waveform
        // (0 deg = en el cruce ascendente, 180 deg = medio cycle despues)
        activeHistory[(size_t) histPos] = active ? 1 : 0;
        const int readPos = histPos >= delay ? histPos - delay : histPos - delay + histSize;
        const bool delayedActive = activeHistory[(size_t) readPos] != 0;
        if (++histPos == histSize) histPos = 0;

        // ---- Ganancia: solo atenua (nunca > 1, nunca negativa), mas bypass del Depth y salida
        bypassState  = bypassTarget + bypassCoef * (bypassState - bypassTarget);
        const float target = delayedActive ? amount : 0.0f;
        controlState = target + smoothCoef * (controlState - target);
        // Bypass general: con bypassState = 0 la ganancia es exactamente 1 (el audio pasa intacto: sin Depth y sin Output)
        const float duck = juce::jmax (0.0f, 1.0f - depth * controlState);
        const float gain = 1.0f + (duck * outGain - 1.0f) * bypassState;

        listenState = listenTarget + bypassCoef * (listenState - listenTarget);   // fade de 5 ms

        for (int ch = 0; ch < outCh; ++ch)
        {
            // Key Listen: se oye el sidechain (el valor ya esta saneado: NaN * 0 = NaN contaminaria la salida aunque Listen estuviera apagado)
            const float key = scCh > 0 ? v[juce::jmin (ch, scCh - 1)] : 0.0f;
            out[ch][s] = out[ch][s] * gain * (1.0f - listenState) + key * listenState;
        }
    }

    const float blockDb = juce::Decibels::gainToDecibels (blockPeak, -100.0f);
    outMeterDb.store (blockDb);
    if (blockDb > peakHoldDb.load()) peakHoldDb.store (blockDb);   // pico mas alto desde el ultimo reinicio (lo reinicia el editor con un clic en el vumetro)
}

juce::AudioProcessorEditor* PhantomAsymProcessor::createEditor()
{
#ifdef SCD_TEST_BUILD
    return new juce::GenericAudioProcessorEditor (*this);
#else
    return new PhantomAsymEditor (*this);
#endif
}

void PhantomAsymProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void PhantomAsymProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PhantomAsymProcessor();
}
