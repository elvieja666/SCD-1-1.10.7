// Cambios de ruteo en caliente del sidechain (sin reiniciar el procesador)
#include "PluginProcessor.h"
#include <cmath>
#include <cstring>
#include <functional>
#include <iostream>
#include <iomanip>

using Fn = std::function<float (double t, int ch)>;
static int failures = 0;
static unsigned long long goldenHash = 1469598103934665603ULL;
static void hashIn (float v) { unsigned int b; std::memcpy (&b, &v, 4); goldenHash = (goldenHash ^ b) * 1099511628211ULL; }
static void check (bool ok, const std::string& what) { std::cout << (ok ? "  [OK]   " : "  [FALLA] ") << what << std::endl; if (! ok) ++failures; }
static juce::AudioChannelSet setFor (int n) { return n == 0 ? juce::AudioChannelSet::disabled() : n == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo(); }
static void setP (PhantomAsymProcessor& p, const char* id, float real) { auto* prm = p.apvts.getParameter (id); if (prm == nullptr) return; prm->setValueNotifyingHost (prm->convertTo0to1 (real)); }

struct Stream
{
    PhantomAsymProcessor proc;
    double sr = 48000.0; int bs = 512; int mainCh = 2, scCh = 2; double t0 = 0.0;
    float lastMean = 1.0f, lastMin = 1.0f; bool finite = true;

    void layout (int main, int sc, bool prepare)
    {
        mainCh = main; scCh = sc;
        juce::AudioProcessor::BusesLayout L;
        L.inputBuses.add (setFor (main)); L.inputBuses.add (setFor (sc)); L.outputBuses.add (setFor (main));
        if (! (proc.checkBusesLayoutSupported (L) && proc.setBusesLayout (L))) std::cout << "  (layout no soportado " << main << "/" << sc << ")" << std::endl;
        proc.setRateAndBufferSizeDetails (sr, bs);
        if (prepare) proc.prepareToPlay (sr, bs);
    }
    // procesa 'sec' segundos con el sidechain dado; mide la relacion salida/entrada en la 2da mitad
    void run (double sec, Fn scFn, bool nanBlock = false)
    {
        const int nCh = juce::jmax (proc.getTotalNumInputChannels(), proc.getTotalNumOutputChannels());
        juce::AudioBuffer<float> buf (nCh, bs); juce::MidiBuffer midi;
        const int total = (int) (sec * sr) / bs * bs;
        std::vector<float> o, in;
        for (int n = 0; n < total; n += bs)
        {
            buf.clear();
            for (int i = 0; i < bs; ++i)
            {
                const double t = t0 + (double) (n + i) / sr;
                const float m = 0.5f * (float) std::sin (2.0 * 3.14159265358979 * 1000.0 * t);
                for (int ch = 0; ch < mainCh; ++ch) buf.setSample (ch, i, m);
                for (int ch = 0; ch < scCh; ++ch) buf.setSample (mainCh + ch, i, scFn (t, ch));
                in.push_back (m);
            }
            if (nanBlock && n == 0) for (int ch = 0; ch < scCh; ++ch) buf.setSample (mainCh + ch, 10, std::numeric_limits<float>::quiet_NaN());
            proc.processBlock (buf, midi);
            for (int i = 0; i < bs; ++i) { hashIn (buf.getSample (0, i)); hashIn (buf.getSample (mainCh - 1, i)); o.push_back (buf.getSample (0, i)); if (! std::isfinite (buf.getSample (0, i))) finite = false; }
        }
        t0 += (double) total / sr;
        const int win = (int) (sr * 0.001); double sum = 0; int cnt = 0; lastMin = 1.0f;
        for (int w = total / 2; w + win <= total; w += win)
        {
            float ro = 0, ri = 0;
            for (int i = 0; i < win; ++i) { ro += o[(size_t) (w + i)] * o[(size_t) (w + i)]; ri += in[(size_t) (w + i)] * in[(size_t) (w + i)]; }
            const float r = std::sqrt (ro / std::max (ri, 1e-9f)); sum += r; ++cnt; lastMin = std::min (lastMin, r);
        }
        lastMean = cnt ? (float) (sum / cnt) : 1.0f;
    }
};

static const double PI2 = 2.0 * 3.14159265358979;
static Fn sine (float a, double f, float dc = 0.0f) { return [=] (double t, int) { return dc + a * (float) std::sin (PI2 * f * t); }; }
static Fn silence() { return [] (double, int) { return 0.0f; }; }
static bool ducking (const Stream& s, float amp = 0.8f) { return s.lastMin < (1.0f - amp) + 0.1f && s.lastMean < 0.98f; }
static void rep (const Stream& s, const std::string& what) { std::cout << "     " << what << ": mean=" << s.lastMean << " min=" << s.lastMin << std::endl; }

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    std::cout << std::fixed << std::setprecision(3);

    for (bool prep : { true, false })
    {
        const std::string tag = prep ? " (con prepareToPlay)" : " (SIN prepareToPlay nuevo)";
        std::cout << "\n== A. activar/cambiar/quitar el bus de sidechain en caliente" << tag << " ==" << std::endl;
        Stream s; s.layout (2, 0, true); setP (s.proc, "mode", 1.0f); setP (s.proc, "depthdb", 0.0f);
        s.run (0.5, sine (0.8f, 50.0));                      rep (s, "sin sidechain");  check (! ducking (s), "sin bus de sidechain: el audio pasa");
        s.layout (2, 2, prep); s.run (0.8, sine (0.8f, 50.0)); rep (s, "sidechain estereo"); check (ducking (s), "bus habilitado en caliente (estereo): hace ducking" + tag);
        s.layout (2, 1, prep); s.run (0.8, sine (0.8f, 50.0)); rep (s, "sidechain mono");   check (ducking (s), "cambio estereo -> mono: hace ducking" + tag);
        s.layout (2, 0, prep); s.run (0.5, sine (0.8f, 50.0)); rep (s, "bus off");         check (! ducking (s), "bus deshabilitado: el audio pasa" + tag);
        s.layout (2, 2, prep); s.run (0.8, sine (0.8f, 50.0)); rep (s, "bus on otra vez"); check (ducking (s), "bus habilitado otra vez: hace ducking" + tag);
        s.layout (1, 2, prep); s.run (0.8, sine (0.8f, 50.0)); rep (s, "main mono");        check (ducking (s), "main pasa a mono, sc estereo: hace ducking" + tag);
        s.layout (2, 2, prep); s.run (0.8, sine (0.8f, 50.0)); rep (s, "main estereo");     check (ducking (s), "main vuelve a estereo: hace ducking" + tag);
    }

    std::cout << "\n== B. cambiar la FUENTE del sidechain (mismo ruteo) ==" << std::endl;
    {
        Stream s; s.layout (2, 2, true); setP (s.proc, "mode", 1.0f);
        s.run (0.8, sine (0.8f, 50.0));  check (ducking (s), "fuente A (50 Hz): ducking");
        s.run (0.5, silence());          check (! ducking (s), "fuente en silencio: sin ducking");
        s.run (0.8, sine (0.5f, 80.0));  rep (s, "80 Hz amp 0.5"); check (ducking (s, 0.5f), "fuente B (80 Hz, amp 0.5): ducking (profundidad = amplitud)");
        s.run (0.8, sine (0.05f, 40.0)); rep (s, "fuente suave"); check (ducking (s, 0.05f), "fuente C (40 Hz, -26 dB): ducking suave (solo ~0.4 dB, por diseno)");
        s.run (0.8, sine (0.8f, 50.0, 0.2f)); rep (s, "con DC +0.2"); check (ducking (s), "fuente con offset DC +0.2: sigue ducking");
        s.run (0.8, sine (0.8f, 50.0));  rep (s, "vuelve sin DC"); check (ducking (s), "vuelve la fuente sin DC: recupera el ducking");
        s.run (0.8, [] (double, int) { return 0.3f; }); rep (s, "DC puro 0.3"); check (! ducking (s), "DC puro (sin ciclos): NO deberia quedar atenuado");
        s.run (0.8, sine (0.8f, 50.0));  rep (s, "tras DC puro"); check (ducking (s), "tras DC puro, la fuente normal recupera el ducking");
        s.run (0.8, [] (double, int) { return -0.3f; }); rep (s, "DC puro -0.3"); check (! ducking (s), "DC negativo puro: sin ducking");
        s.run (0.8, sine (0.8f, 50.0));  check (ducking (s), "tras DC negativo, recupera");
    }

    std::cout << "\n== C. valores raros en el sidechain ==" << std::endl;
    {
        Stream s; s.layout (2, 2, true); setP (s.proc, "mode", 1.0f);
        s.run (0.5, sine (0.8f, 50.0), true);
        check (s.finite, "un NaN en el sidechain NO contamina la salida (listen apagado)");
        s.finite = true;
        s.run (0.8, sine (0.8f, 50.0)); rep (s, "tras el NaN"); check (ducking (s) && s.finite, "tras el NaN el plugin sigue funcionando");
    }

    std::cout << "\n== D. sidechain estereo con canales en contrafase ==" << std::endl;
    {
        Stream s; s.layout (2, 2, true); setP (s.proc, "mode", 1.0f);
        s.run (0.8, [] (double t, int ch) { return (ch == 0 ? 1.0f : -1.0f) * 0.8f * (float) std::sin (PI2 * 50.0 * t); });
        rep (s, "L = -R"); check (ducking (s), "L = -R: sigue haciendo ducking (ya no se anula)");
        s.run (0.8, [] (double t, int ch) { return ch == 0 ? 0.8f * (float) std::sin (PI2 * 50.0 * t) : 0.0f; });
        rep (s, "solo L"); check (ducking (s), "solo canal izquierdo: ducking a profundidad completa");
        s.run (0.8, [] (double t, int ch) { return ch == 1 ? 0.8f * (float) std::sin (PI2 * 50.0 * t) : 0.0f; });
        rep (s, "solo R"); check (ducking (s), "solo canal derecho: ducking a profundidad completa");
    }


    std::cout << "\n== F. reese bass estereo vs bajo mono (2 saws desafinadas, LFO de amplitud) ==" << std::endl;
    {
        auto saw = [] (double ph) { ph -= std::floor (ph); return (float) (2.0 * ph - 1.0); };
        auto reese = [&] (double f, double det1, double det2, double ph0) { return [=] (double t, int) { (void) ph0; return 0.0f; }; };
        (void) reese;
        // mono: una sola saw 55 Hz (con LFO) ; estereo: L = saw(55*(1-d)) + saw(55*(1+d)), R = saw(55*(1-2d)+fase) + saw(55*(1+2d))
        auto lfo = [] (double t) { return 0.65f + 0.35f * (float) std::sin (PI2 * 1.5 * t); };
        Fn mono = [=] (double t, int) { return 0.7f * lfo (t) * saw (55.0 * t); };
        Fn st   = [=] (double t, int ch) { const double d = 0.012;
                    return 0.4f * lfo (t) * (ch == 0 ? (saw (55.0 * (1 - d) * t) + saw (55.0 * (1 + d) * t))
                                                     : (saw (55.0 * (1 - 2 * d) * t + 0.3) + saw (55.0 * (1 + 2 * d) * t + 0.6))); };
        for (int mode : { 0, 1 })
        {
            auto dips = [&] (Fn f, const char* name)
            {
                Stream s; s.layout (2, 2, true); setP (s.proc, "mode", (float) mode);
                s.run (0.5, f);
                // contar bajadas en 2 s
                const int nCh = 4; juce::AudioBuffer<float> buf (nCh, s.bs); juce::MidiBuffer midi; int cnt = 0; bool dn = false; double sum = 0; long n = 0;
                for (int b = 0; b < (int) (2.0 * s.sr / s.bs); ++b)
                {
                    buf.clear();
                    for (int i = 0; i < s.bs; ++i) { const double t = s.t0 + (double) (b * s.bs + i) / s.sr; buf.setSample (0, i, 0.5f); buf.setSample (1, i, 0.5f); buf.setSample (2, i, f (t, 0)); buf.setSample (3, i, f (t, 1)); }
                    s.proc.processBlock (buf, midi);
                    for (int i = 0; i < s.bs; ++i) { const bool d = buf.getSample (0, i) < 0.25f; if (d && ! dn) ++cnt; dn = d; sum += buf.getSample (0, i) / 0.5f; ++n; }
                }
                std::cout << "     " << (mode ? "Whole" : "Half ") << " " << name << ": bajadas/s = " << cnt / 2.0 << ", ganancia media = " << sum / (double) n << std::endl;
            };
            dips (mono, "bajo mono (saw 55 Hz)");
            dips (st, "reese estereo        ");
        }
    }

    std::cout << "\n== H. fuzz: parametros, ruteo, tamaños de bloque, sample rates y sidechain al azar ==" << std::endl;
    {
        juce::Random rnd (12345);
        int bad = 0, blocks = 0;
        for (int it = 0; it < 60; ++it)
        {
            float prevInPeak = 0.0f, prevKeyPeak = 0.0f;   // el plugin retrasa audio y key unas muestras: lo que sale puede venir del bloque anterior
            PhantomAsymProcessor proc;
            const double rates[] = { 22050.0, 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };
            const double sr = rates[rnd.nextInt (6)];
            int mainCh = 1 + rnd.nextInt (2), scCh = rnd.nextInt (3);
            auto lay = [&] (int m, int s)
            {
                juce::AudioProcessor::BusesLayout L; L.inputBuses.add (setFor (m)); L.inputBuses.add (setFor (s)); L.outputBuses.add (setFor (m));
                return proc.checkBusesLayoutSupported (L) && proc.setBusesLayout (L);
            };
            lay (mainCh, scCh);
            const int maxBs = 4096; proc.setRateAndBufferSizeDetails (sr, maxBs);
            if (rnd.nextBool()) proc.prepareToPlay (sr, maxBs);
            juce::MidiBuffer midi;
            double t = 0.0;
            for (int b = 0; b < 40; ++b)
            {
                if (rnd.nextInt (6) == 0) { mainCh = 1 + rnd.nextInt (2); scCh = rnd.nextInt (3); lay (mainCh, scCh); if (rnd.nextBool()) proc.prepareToPlay (sr, maxBs); }
                if (rnd.nextInt (3) == 0)
                {
                    for (const char* id : { "depthdb", "output", "mode", "fade", "bypass", "listen" })
                        setP (proc, id, std::string (id) == "depthdb" || std::string (id) == "output" ? -60.0f + 66.0f * rnd.nextFloat()
                                        : std::string (id) == "fade" ? 100.0f * rnd.nextFloat() : (float) rnd.nextInt (2));
                }
                const int bs = 1 + rnd.nextInt (maxBs);
                const int nCh = juce::jmax (proc.getTotalNumInputChannels(), proc.getTotalNumOutputChannels());
                juce::AudioBuffer<float> buf (nCh, bs);
                const int kind = rnd.nextInt (6);   // tipo de sidechain: seno, ruido, DC, silencio, inf/NaN esporadicos, impulsos
                float inPeak = 0.0f, keyPeak = 0.0f;
                for (int i = 0; i < bs; ++i, t += 1.0 / sr)
                {
                    const float m = 0.5f * (float) std::sin (PI2 * 440.0 * t);
                    for (int c = 0; c < mainCh; ++c) buf.setSample (c, i, m);
                    inPeak = std::max (inPeak, std::abs (m));
                    for (int c = 0; c < scCh; ++c)
                    {
                        float v = 0.0f;
                        switch (kind)
                        {
                            case 0: v = 0.8f * (float) std::sin (PI2 * (30.0 + 40.0 * c) * t); break;
                            case 1: v = rnd.nextFloat() * 2.0f - 1.0f; break;
                            case 2: v = 0.4f; break;
                            case 3: v = 0.0f; break;
                            case 4: v = (rnd.nextInt (50) == 0) ? (rnd.nextBool() ? std::numeric_limits<float>::infinity() : std::numeric_limits<float>::quiet_NaN()) : 0.5f * (float) std::sin (PI2 * 50.0 * t); break;
                            default: v = (i % 700 == 0) ? 1.0f : 0.0f; break;
                        }
                        buf.setSample (mainCh + c, i, v);
                        if (std::isfinite (v)) keyPeak = std::max (keyPeak, std::abs (v));
                    }
                }
                proc.processBlock (buf, midi); ++blocks;
                bool ok = true; float outPeak = 0.0f;
                for (int c = 0; c < mainCh; ++c) for (int i = 0; i < bs; ++i)
                { const float v = buf.getSample (c, i); if (! std::isfinite (v)) ok = false; outPeak = std::max (outPeak, std::abs (v)); }
                // cota: Output +6 dB (x1.995) sobre la entrada; con Key Listen puede salir el sidechain (x1) en su lugar
                if (outPeak > std::max (std::max (inPeak, prevInPeak) * 2.0f, std::max (keyPeak, prevKeyPeak) * 1.01f) + 1e-4f) ok = false;
                prevInPeak = inPeak; prevKeyPeak = keyPeak;
                if (! ok) { ++bad; if (bad <= 5) std::cout << "     FALLO it=" << it << " bloque=" << b << " bs=" << bs << " sr=" << sr << " main=" << mainCh << " sc=" << scCh << " tipo=" << kind << " pico salida=" << outPeak << std::endl; }
            }
        }
        std::cout << "     bloques procesados: " << blocks << std::endl;
        check (bad == 0, "fuzz: salida siempre finita y acotada, sin crashes ni errores de memoria (ASan/UBSan)");
    }
    std::cout << "\n== E. Key Listen con valores raros ==" << std::endl;
    {
        Stream s; s.layout (2, 2, true);
        s.run (0.3, silence(), true);
        check (s.finite, "NaN en el sidechain con Listen apagado: salida finita");
    }


    std::cout << "\n== G. huella de salida con todos los parametros en movimiento ==" << std::endl;
    {
        Stream s; s.layout (2, 2, true);
        Fn rs = [] (double t, int ch) { const double d = 0.012; auto saw = [] (double ph) { ph -= std::floor (ph); return (float) (2.0 * ph - 1.0); };
            return 0.4f * (0.65f + 0.35f * (float) std::sin (PI2 * 1.5 * t)) * (ch == 0 ? saw (55.0 * (1 - d) * t) + saw (55.0 * (1 + d) * t) : saw (55.0 * (1 - 2 * d) * t + 0.3) + saw (55.0 * (1 + 2 * d) * t + 0.6)); };
        int step = 0;
        for (float mode : { 0.0f, 1.0f }) for (float pol : { 0.0f, 45.0f, 180.0f }) for (float dep : { 0.0f, -6.0f, 3.0f, -60.0f })
        {
            setP (s.proc, "mode", mode); setP (s.proc, "polarity", pol); setP (s.proc, "depthdb", dep);
            setP (s.proc, "bypass", (step % 5 == 4) ? 1.0f : 0.0f); setP (s.proc, "listen", (step % 7 == 6) ? 1.0f : 0.0f);
            setP (s.proc, "output", (step % 3 == 0) ? 0.0f : -3.5f);
            s.run (0.25, rs); ++step;
        }
        s.layout (1, 1, true); s.run (0.4, rs); s.layout (2, 0, true); s.run (0.2, rs);
    }
    std::cout << "\nHUELLA: " << std::hex << goldenHash << std::dec << std::endl;
    std::cout << "\nRESULTADO: " << (failures == 0 ? "TODO OK" : std::to_string (failures) + " FALLAS") << std::endl;
    return failures == 0 ? 0 : 1;
}
