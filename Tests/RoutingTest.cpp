#include "PluginProcessor.h"
#include <cmath>
#include <functional>
#include <iostream>
#include <iomanip>

using Fn = std::function<float (double t, int ch)>;
static int failures = 0;
static void check (bool ok, const std::string& what)
{
    std::cout << (ok ? "  [OK]   " : "  [FALLA] ") << what << std::endl;
    if (! ok) ++failures;
}

struct Cfg
{
    int mainCh = 2;
    int scCh = 2;            // 0 = bus de sidechain deshabilitado
    double sr = 48000.0;
    int bs = 512;
    float mode = 1.0f;       // 0 Half, 1 Whole
    float depthDb = 0.0f;
    bool bypass = false, listen = false;
    float outDb = 0.0f;
    double seconds = 1.0;
    bool prepare = true;
    Fn mainFn = [] (double t, int) { return 0.5f * (float) std::sin (2.0 * 3.14159265358979 * 1000.0 * t); };
    Fn scFn   = [] (double t, int) { return 0.8f * (float) std::sin (2.0 * 3.14159265358979 * 50.0 * t); };
};

struct Out
{
    bool layoutOk = false, scActive = false, finite = true;
    int dips = 0;
    float minRatio = 1.0f, meanRatio = 1.0f, maxOverInput = 0.0f;
    float keyErr = 0.0f, absMax = 0.0f, absMaxLate = 0.0f;
    int n = 0, latency = 0;   // latency: muestras que el plugin retrasa el audio (y el key)
    std::vector<float> ch0, ch1, in0;
};

static juce::AudioChannelSet setFor (int n) { return n == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo(); }

static void setP (PhantomAsymProcessor& p, const char* id, float real)
{
    auto* prm = p.apvts.getParameter (id); if (prm == nullptr) return;
    prm->setValueNotifyingHost (prm->convertTo0to1 (real));
}

static Out run (const Cfg& c)
{
    Out o;
    PhantomAsymProcessor proc;
    juce::AudioProcessor::BusesLayout L;
    L.inputBuses.add (setFor (c.mainCh));
    L.inputBuses.add (c.scCh == 0 ? juce::AudioChannelSet::disabled() : setFor (c.scCh));
    L.outputBuses.add (setFor (c.mainCh));
    o.layoutOk = proc.checkBusesLayoutSupported (L) && proc.setBusesLayout (L);
    o.scActive = proc.getBus (true, 1) != nullptr && proc.getBus (true, 1)->isEnabled();

    proc.setRateAndBufferSizeDetails (c.sr, c.bs);
    if (c.prepare) proc.prepareToPlay (c.sr, c.bs);

    setP (proc, "depthdb", c.depthDb);
    setP (proc, "bypass", c.bypass ? 1.0f : 0.0f);
    setP (proc, "listen", c.listen ? 1.0f : 0.0f);
    setP (proc, "output", c.outDb);
    setP (proc, "mode", c.mode);

    const int total = (int) (c.sr * c.seconds);
    const int nCh = juce::jmax (proc.getTotalNumInputChannels(), proc.getTotalNumOutputChannels());
    juce::AudioBuffer<float> buf (nCh, c.bs);
    juce::MidiBuffer midi;
    o.ch0.assign ((size_t) total, 0.0f); o.ch1.assign ((size_t) total, 0.0f); o.in0.assign ((size_t) total, 0.0f);

    o.latency = c.prepare ? proc.getLatencySamples() : 0;
    int n = 0;
    while (n + c.bs <= total)
    {
        buf.clear();
        for (int i = 0; i < c.bs; ++i)
        {
            const double t = (double) (n + i) / c.sr;
            for (int ch = 0; ch < c.mainCh; ++ch) buf.setSample (ch, i, c.mainFn (t, ch));
            for (int ch = 0; ch < c.scCh; ++ch)  buf.setSample (c.mainCh + ch, i, c.scFn (t, ch));
            o.in0[(size_t) (n + i)] = (n + i - o.latency) >= 0 ? c.mainFn ((double) (n + i - o.latency) / c.sr, 0) : 0.0f;   // entrada ya retrasada (como sale del plugin)
        }
        proc.processBlock (buf, midi);
        for (int i = 0; i < c.bs; ++i)
        {
            const float a = buf.getSample (0, i), b = buf.getSample (c.mainCh - 1, i);
            o.ch0[(size_t) (n + i)] = a; o.ch1[(size_t) (n + i)] = b;
            if (! std::isfinite (a) || ! std::isfinite (b)) o.finite = false;
            o.absMax = std::max (o.absMax, std::abs (a));
            if (n + i > (int) (0.2 * c.sr)) o.absMaxLate = std::max (o.absMaxLate, std::abs (a));
        }
        n += c.bs;
    }

    o.n = n;
    const int win = (int) (c.sr * 0.001), start = (int) (0.2 * c.sr);
    bool ducked = false; double sum = 0; int cnt = 0;
    for (int w = start; w + win <= n; w += win)
    {
        float ro = 0, ri = 0;
        for (int i = 0; i < win; ++i) { ro += o.ch0[(size_t) (w + i)] * o.ch0[(size_t) (w + i)]; ri += o.in0[(size_t) (w + i)] * o.in0[(size_t) (w + i)]; }
        const float r = std::sqrt (ro / juce::jmax (ri, 1e-9f));
        o.minRatio = std::min (o.minRatio, r); sum += r; ++cnt;
        const bool d = r < 0.6f;
        if (d && ! ducked) ++o.dips;
        ducked = d;
    }
    o.meanRatio = cnt ? (float) (sum / cnt) : 1.0f;
    const double span = (double) (n - start) / c.sr;
    o.dips = (int) std::lround (o.dips / span);
    for (int i = start; i < n; ++i)
        o.maxOverInput = std::max (o.maxOverInput, std::abs (o.ch0[(size_t) i]) - std::abs (o.in0[(size_t) i]));
    return o;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    std::cout << std::fixed << std::setprecision(3);

    // ---------------------------------------------------------------- 1. parametros
    std::cout << "\n== 1. Parametros y orden ==" << std::endl;
    {
        PhantomAsymProcessor p;
        std::string order;
        for (auto* prm : p.getParameters()) order += prm->getName (40).toStdString() + " | ";
        std::cout << "  Orden: " << order << std::endl;
        check (order == "Depth | Bypass | SC Listen | Mode | Fade | Output | ", "orden: Depth, Bypass, SC Listen, Mode, Fade, Output");
        check (order.find ("Key Input") == std::string::npos, "Key Input eliminado");

        auto* d = p.apvts.getParameter ("depthdb");
        check (std::abs (d->convertFrom0to1 (d->getDefaultValue())) < 1e-4f, "Depth por defecto = 0 dB");
        check (d->convertFrom0to1 (1.0f) == 6.0f, "Depth maximo = +6 dB");
        check (d->getText (0.0f, 20) == "-inf", "Depth minimo se muestra como -inf");
        check (std::abs (d->convertFrom0to1 (d->getValueForText ("-inf")) + 60.0f) < 1e-3f, "escribir 'inf' / '-inf' -> minimo (silencio)");
        check (std::abs (d->convertFrom0to1 (d->getValueForText ("-6")) + 6.0f) < 0.02f, "escribir -6 -> -6 dB");
        check (std::abs (d->convertFrom0to1 (d->getValueForText ("3.5")) - 3.5f) < 0.02f, "escribir 3.5 -> 3.5 dB");
        check (std::abs (d->convertFrom0to1 (d->getValueForText ("99")) - 6.0f) < 0.02f, "escribir 99 -> se limita a +6 dB");
        check (std::abs (d->getValue() - 0.50f) < 1e-3f, "Depth por defecto (0 dB) cae en el centro de la perilla");
        check (std::abs (d->convertTo0to1 (6.0f) - 1.0f) < 1e-6f && std::abs (d->convertTo0to1 (-60.0f)) < 1e-6f, "extremos: -inf en 0 %, +6 dB en 100 %");
        check (std::abs (d->convertTo0to1 (3.0f) - 0.75f) < 1e-3f && std::abs (d->convertTo0to1 (-3.0f) - 5.0f / 12.0f) < 1e-3f && std::abs (d->convertTo0to1 (-6.0f) - 4.0f / 12.0f) < 1e-3f
               && std::abs (d->convertTo0to1 (-12.0f) - 3.0f / 12.0f) < 1e-3f && std::abs (d->convertTo0to1 (-24.0f) - 2.0f / 12.0f) < 1e-3f && std::abs (d->convertTo0to1 (-48.0f) - 1.0f / 12.0f) < 1e-3f,
               "Depth: +3 dB al 75 %; marcas -3/-6/-12/-24/-48 en las posiciones 5/4/3/2/1 de 12 (mitad izquierda logaritmica)");
        {
            bool mono = true, round = true; float prev = -1000.0f;
            for (int i = 0; i <= 1000; ++i)
            {
                const float pos = (float) i / 1000.0f, v = d->convertFrom0to1 (pos);
                mono &= v >= prev - 1e-6f; prev = v;
                round &= std::abs (d->convertTo0to1 (v) - pos) < 2e-3f;
            }
            check (mono, "la curva es monotona (sin saltos ni retrocesos)");
            check (round, "ida y vuelta posicion <-> dB consistente");
        }
        auto* out = p.apvts.getParameter ("output");
        check (std::abs (out->convertFrom0to1 (out->getValueForText ("-3.25")) + 3.25f) < 0.02f, "Output: escribir -3.25 -> -3.25 dB");
        check (std::abs (out->getValue() - 5.0f / 6.0f) < 1e-3f, "Output por defecto (0 dB) cae al 83.3 % del slider");
        {
            auto pos = [&] (float db) { return out->convertTo0to1 (db); };
            check (std::abs (pos (6.0f) - 1.0f) < 1e-6f && std::abs (pos (-60.0f)) < 1e-6f
                   && std::abs (pos (0.0f) - 5.0f / 6.0f) < 1e-4f && std::abs (pos (-6.0f) - 4.0f / 6.0f) < 1e-4f && std::abs (pos (-12.0f) - 3.0f / 6.0f) < 1e-4f
                   && std::abs (pos (-24.0f) - 2.0f / 6.0f) < 1e-4f && std::abs (pos (-48.0f) - 1.0f / 6.0f) < 1e-4f,
                   "Output: marcas +6/0/-6/-12/-24/-48/-inf en 100/83/67/50/33/17/0 % (fader logaritmico)");
            check (out->getText (0.0f, 20) == "-inf" && std::abs (out->convertFrom0to1 (out->getValueForText ("-inf")) + 60.0f) < 1e-3f, "Output: minimo se muestra como -inf y se escribe '-inf'");
        }
        {
            bool mono = true, round = true; float prev = -1000.0f;
            for (int i = 0; i <= 1000; ++i)
            {
                const float pos = (float) i / 1000.0f, v = out->convertFrom0to1 (pos);
                mono &= v >= prev - 1e-6f; prev = v;
                round &= std::abs (out->convertTo0to1 (v) - pos) < 2e-3f;
            }
            check (mono && round, "Output: curva monotona y consistente ida y vuelta");
        }
        check (out->getText (out->getValue(), 20) == "0.00" && d->getText (d->getValue(), 20) == "0.00", "Output y Depth se muestran como 0.00 (2 decimales)");
        check (out->getText (out->convertTo0to1 (-3.25f), 20) == "-3.25", "Output -3.25 se muestra como -3.25");
        auto* mode = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter ("mode"));
        check (mode->getCurrentChoiceName() == "Half", "Mode por defecto = Half");
    }

    // ---------------------------------------------------------------- 2. layouts
    std::cout << "\n== 2. Buses soportados ==" << std::endl;
    {
        PhantomAsymProcessor p;
        struct Case { int m; int s; bool expect; };
        for (auto cs : { Case { 2, 2, true }, Case { 2, 1, true }, Case { 2, 0, true }, Case { 1, 1, true }, Case { 1, 2, true }, Case { 1, 0, true } })
        {
            juce::AudioProcessor::BusesLayout L;
            L.inputBuses.add (setFor (cs.m));
            L.inputBuses.add (cs.s == 0 ? juce::AudioChannelSet::disabled() : setFor (cs.s));
            L.outputBuses.add (setFor (cs.m));
            check (p.checkBusesLayoutSupported (L) == cs.expect,
                   "main " + std::string (cs.m == 1 ? "mono" : "stereo") + " + sidechain " + (cs.s == 0 ? "off" : cs.s == 1 ? "mono" : "stereo") + " aceptado");
        }
        juce::AudioProcessor::BusesLayout bad;
        bad.inputBuses.add (juce::AudioChannelSet::create5point1());
        bad.inputBuses.add (juce::AudioChannelSet::disabled());
        bad.outputBuses.add (juce::AudioChannelSet::create5point1());
        check (! p.checkBusesLayoutSupported (bad), "5.1 se rechaza (no hay layouts raros)");
        juce::AudioProcessor::BusesLayout mism;
        mism.inputBuses.add (juce::AudioChannelSet::stereo());
        mism.inputBuses.add (juce::AudioChannelSet::disabled());
        mism.outputBuses.add (juce::AudioChannelSet::mono());
        check (! p.checkBusesLayoutSupported (mism), "entrada stereo / salida mono se rechaza");
        check (p.getBus (true, 1) != nullptr && ! p.getBus (true, 1)->isEnabled(), "sidechain deshabilitado por defecto (el host lo habilita al elegir fuente)");
    }

    // ---------------------------------------------------------------- 3. matriz de ruteo
    std::cout << "\n== 3. Matriz de ruteo (sidechain 50 Hz, audio 1 kHz) ==" << std::endl;
    for (int mainCh : { 1, 2 })
        for (int scCh : { 0, 1, 2 })
            for (float mode : { 0.0f, 1.0f })
            {
                Cfg c; c.mainCh = mainCh; c.scCh = scCh; c.mode = mode;
                auto o = run (c);
                const std::string tag = std::string ("main ") + (mainCh == 1 ? "mono" : "stereo") + ", sidechain " + (scCh == 0 ? "off   " : scCh == 1 ? "mono  " : "stereo") + ", " + (mode > 0.5f ? "Whole" : "Half ");
                std::cout << "  " << tag << ": bus sc activo=" << o.scActive << " silencios/s=" << o.dips << " ratio min=" << o.minRatio << " media=" << o.meanRatio << std::endl;
                check (o.layoutOk && o.finite, tag + ": layout aceptado y salida finita");
                if (scCh == 0) check (o.minRatio > 0.999f, tag + ": sin sidechain el audio pasa intacto");
                else           check (o.minRatio < 0.3f && o.dips >= 15, tag + ": el sidechain SI produce efecto");
                check (o.maxOverInput < 1e-4f, tag + ": la salida nunca supera la entrada");
                if (mainCh == 2) { float dd = 0; for (size_t i = 0; i < (size_t) o.n; ++i) dd = std::max (dd, std::abs (o.ch0[i] - o.ch1[i])); check (dd < 1e-6f, tag + ": L y R se atenuan igual"); }
            }

    // ---------------------------------------------------------------- 4. bloques
    std::cout << "\n== 4. Tamanos de bloque (Whole, esperado ~25 silencios/s) ==" << std::endl;
    for (int bs : { 1, 7, 64, 480, 512, 2048 })
    {
        Cfg c; c.bs = bs; auto o = run (c);
        std::cout << "  bloque " << bs << ": silencios/s=" << o.dips << std::endl;
        check (o.dips >= 23 && o.dips <= 27 && o.finite, "bloque " + std::to_string (bs));
    }

    // ---------------------------------------------------------------- 5. sample rates
    std::cout << "\n== 5. Sample rates ==" << std::endl;
    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
    {
        Cfg c; c.sr = sr; auto o = run (c);
        std::cout << "  " << sr << " Hz: silencios/s=" << o.dips << std::endl;
        check (o.dips >= 23 && o.dips <= 27 && o.finite, "sample rate " + std::to_string ((int) sr));
    }

    // ---------------------------------------------------------------- 6. Depth en dB
    std::cout << "\n== 6. Depth en dB (sidechain 0.8 = -1.9 dBFS) ==" << std::endl;
    {
        Cfg c; c.depthDb = -60.0f; auto a = run (c);
        check (a.minRatio > 0.999f, "Depth -inf -> sin efecto");
        c.depthDb = 0.0f;  auto b = run (c);
        c.depthDb = -6.0f; auto d = run (c);
        c.depthDb = 6.0f;  auto e = run (c);
        std::cout << "  ratio minimo: -inf=" << a.minRatio << "  0dB=" << b.minRatio << "  -6dB=" << d.minRatio << "  +6dB=" << e.minRatio << std::endl;
        check (b.minRatio < 0.25f, "Depth 0 dB -> reduccion casi total con key fuerte");
        check (d.minRatio > b.minRatio + 0.2f, "Depth -6 dB reduce menos que 0 dB");
        check (e.minRatio < 0.01f, "Depth +6 dB -> silencio total");
        // key flojo (-26 dBFS): +6 dB lo duplica
        c.scFn = [] (double t, int) { return 0.05f * (float) std::sin (2.0 * 3.14159265358979 * 50.0 * t); };
        c.depthDb = 0.0f;  auto f = run (c);
        c.depthDb = 6.0f;  auto g = run (c);
        std::cout << "  key a -26 dBFS: ratio minimo con 0 dB=" << f.minRatio << "  con +6 dB=" << g.minRatio << std::endl;
        check (g.minRatio < f.minRatio, "+6 dB aumenta el efecto con key flojo");
        c.bypass = true; c.depthDb = 0.0f; auto h = run (c);
        check (h.minRatio > 0.999f, "Bypass -> sin efecto");
        c.outDb = -12.0f; auto h2 = run (c);
        float maxDiff = 0; for (int i = (int) (0.1 * 48000); i < h2.n; ++i) maxDiff = std::max (maxDiff, std::abs (h2.ch0[(size_t) i] - h2.in0[(size_t) i]));
        std::cout << "  Bypass con Output -12 dB: diferencia maxima salida-entrada = " << maxDiff << std::endl;
        check (maxDiff < 1e-6f, "Bypass general: el audio pasa intacto aunque Output este en -12 dB");
        c.bypass = false; auto h3 = run (c);
        check (h3.meanRatio < 0.3f, "sin Bypass y Output -12 dB: Output SI se aplica");
    }

    // ---------------------------------------------------------------- 7. Key Listen
    std::cout << "\n== 7. Key Listen ==" << std::endl;
    for (int mainCh : { 1, 2 })
        for (int scCh : { 1, 2 })
        {
            Cfg c; c.mainCh = mainCh; c.scCh = scCh; c.listen = true;
            auto o = run (c);
            float err = 0;
            for (size_t i = (size_t) (0.2 * c.sr); i < (size_t) o.n; ++i)
            {
                const double t = (double) ((int) i - o.latency) / c.sr;   // el key tambien sale retrasado
                err = std::max (err, std::abs (o.ch0[i] - c.scFn (t, 0)));
            }
            check (err < 1e-3f, std::string ("main ") + (mainCh == 1 ? "mono" : "stereo") + " + sc " + (scCh == 1 ? "mono" : "stereo") + ": se escucha solo el key (error " + std::to_string (err) + ")");
        }
    {
        Cfg c; c.scCh = 0; c.listen = true; auto o = run (c);
        check (o.absMaxLate < 1e-4f && o.finite, "Key Listen sin sidechain conectado -> silencio (despues del fade de 5 ms)");
    }

    // ---------------------------------------------------------------- 8. robustez
    std::cout << "\n== 8. Robustez ==" << std::endl;
    {
        Cfg c; c.prepare = false; auto o = run (c);
        check (o.finite && o.maxOverInput < 1e-4f, "processBlock antes de prepareToPlay: no crashea, audio intacto");
        Cfg d; d.mainFn = [] (double, int) { return 0.0f; }; auto q = run (d);
        check (q.finite && q.absMax == 0.0f, "audio en silencio + sidechain activo: salida en silencio, sin NaN");
        Cfg e; e.scFn = [] (double, int) { return 0.0f; }; auto r = run (e);
        check (r.minRatio > 0.999f, "sidechain conectado pero en silencio: audio intacto");
        Cfg f; f.scFn = [] (double t, int) { return 4.0f * (float) std::sin (2.0 * 3.14159265358979 * 50.0 * t); }; auto s = run (f);
        check (s.finite && s.maxOverInput < 1e-4f, "sidechain a +12 dBFS (fuera de rango): sin NaN, salida <= entrada");
        Cfg g; g.scFn = [] (double, int) { return 0.5f; }; auto t = run (g);
        std::cout << "  sidechain con DC puro (0.5): ratio min=" << t.minRatio << " (informativo)" << std::endl;
        check (t.finite, "sidechain con DC: sin NaN");
    }
    {
        PhantomAsymProcessor a;
        setP (a, "depthdb", -12.5f); setP (a, "mode", 1.0f); setP (a, "listen", 1.0f); setP (a, "bypass", 1.0f); setP (a, "output", -4.0f);
        juce::MemoryBlock mb; a.getStateInformation (mb);
        PhantomAsymProcessor b; b.setStateInformation (mb.getData(), (int) mb.getSize());
        auto get = [] (PhantomAsymProcessor& p, const char* id) { auto* x = p.apvts.getParameter (id); return x->convertFrom0to1 (x->getValue()); };
        check (std::abs (get (b, "depthdb") + 12.5f) < 0.02f && get (b, "mode") > 0.5f
               && get (b, "listen") > 0.5f && get (b, "bypass") > 0.5f && std::abs (get (b, "output") + 4.0f) < 0.02f, "guardar/restaurar estado del proyecto");
    }

    // ---------------------------------------------------------------- 9. bus completo como sidechain
    std::cout << "\n== 9. Que pasa si mandas un BUS completo (no solo sub) al sidechain ==" << std::endl;
    {
        juce::Random rng (3);
        auto kick = [] (double t) { const double tt = std::fmod (t, 0.5); return 0.8 * std::exp (-tt / 0.15) * std::sin (2.0 * 3.14159265358979 * 50.0 * tt); };
        Cfg sub; sub.seconds = 2.0; sub.scFn = [&] (double t, int) { return (float) kick (t); };
        auto a = run (sub);
        Cfg bus; bus.seconds = 2.0;
        bus.scFn = [&] (double t, int) { return (float) (kick (t) + 0.25 * std::sin (2.0 * 3.14159265358979 * 3000.0 * t) + 0.1 * (rng.nextFloat() * 2.0f - 1.0f)); };
        auto b = run (bus);
        Cfg quiet; quiet.seconds = 2.0; quiet.scFn = [&] (double t, int) { return (float) (0.06 * kick (t)); };
        auto q = run (quiet);
        std::cout << "  solo kick/sub     : silencios/s=" << a.dips << "  ratio min=" << a.minRatio << "  ratio medio=" << a.meanRatio << std::endl;
        std::cout << "  bus con agudos    : silencios/s=" << b.dips << "  ratio min=" << b.minRatio << "  ratio medio=" << b.meanRatio << std::endl;
        std::cout << "  kick muy bajo     : silencios/s=" << q.dips << "  ratio min=" << q.minRatio << "  ratio medio=" << q.meanRatio << std::endl;
    }

    // ---------------------------------------------------------------- 10. GUI
    std::cout << "\n== 10. Interfaz ==" << std::endl;
    {
        PhantomAsymProcessor p;
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        std::vector<juce::Slider*> sl; std::vector<juce::ToggleButton*> tg; int combos = 0;
        std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
        {
            for (auto* ch : c.getChildren())
            {
                if (auto* s = dynamic_cast<juce::Slider*> (ch)) sl.push_back (s);
                if (auto* t = dynamic_cast<juce::ToggleButton*> (ch)) tg.push_back (t);
                if (dynamic_cast<juce::ComboBox*> (ch) != nullptr) ++combos;
                walk (*ch);
            }
        };
        walk (*ed);
        bool editable = true;
        for (auto* s : sl) { editable &= s->isTextBoxEditable(); }
        std::cout << "  sliders=" << sl.size() << " toggles=" << tg.size() << " combos=" << combos << std::endl;
        check (sl.size() == 3, "3 sliders (Depth, Fade, Output)");
        check (editable, "caja numerica editable en todos los sliders");
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        juce::File f ("/tmp/claude-0/-home-claude/b63ef87a-c6fc-52aa-b338-1a6790b01f08/scratchpad/ui_final.png");
        f.deleteFile(); juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
    }

    // ---------------------------------------------------------------- 11. UI estilo referencia
    std::cout << "\n== 10b. Output logaritmico en el audio ==" << std::endl;
    {
        Cfg c; c.depthDb = -60.0f; c.scCh = 0; c.mode = 0.0f;   // Depth en -inf: el audio pasa sin ducking
        auto level = [&] (float outDb) { c.outDb = outDb; auto o = run (c); return o.absMaxLate; };
        const float l0 = level (0.0f), l24 = level (-24.0f), l6 = level (6.0f), linf = level (-60.0f);
        std::cout << "  pico con Output 0 dB=" << l0 << "  -24 dB=" << l24 << "  +6 dB=" << l6 << "  -inf=" << linf << std::endl;
        check (std::abs (l0 - 0.5f) < 0.01f, "Output 0 dB: ganancia unidad");
        check (std::abs (20.0f * std::log10 (l24 / l0) + 24.0f) < 0.3f, "Output -24 dB: baja 24 dB");
        check (std::abs (20.0f * std::log10 (l6 / l0) - 6.0f) < 0.3f, "Output +6 dB: sube 6 dB");
        check (linf == 0.0f, "Output -inf: silencio total");
    }

    // Host que manda en el sidechain la propia señal de la pista (Logic con "None"): el plugin lo ignora y el audio pasa intacto
    {
        Cfg c; c.mode = 0.0f; c.depthDb = 6.0f;
        c.mainFn = [] (double t, int) { return 0.5f * (float) std::sin (2.0 * 3.14159265358979 * 50.0 * t); };
        c.scFn = c.mainFn;
        const Out o = run (c);
        check (o.layoutOk && o.finite && o.minRatio > 0.999f, "sidechain identico a la entrada (host sin fuente): el audio pasa intacto");
    }

    std::cout << "\nRESULTADO: " << (failures == 0 ? "TODO OK" : std::to_string (failures) + " FALLAS") << std::endl;
    return failures == 0 ? 0 : 1;
}
