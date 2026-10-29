#include "PluginEditor.h"
#include "BinaryData.h"

using namespace juce;

namespace
{
    // ---- geometria en unidades de diseno (ventana 1696 x 1044) ----
    constexpr float kDepthCx = 410.0f, kDepthCy = 522.0f, kDepthR = 270.0f;
    constexpr float kFadeCx = 908.0f, kFadeCy = 252.0f, kFadeR = 112.0f;
    constexpr float kModeCx = 908.0f, kModeCy = 792.0f, kModeR = 112.0f;
    constexpr float kDefaultScale = 321.0f / 996.0f;               // tamano de la ventana: 321 de alto (y 531 de ancho)
    constexpr float kArtW = 1648.0f, kArtH = 996.0f;              // arte ya recortado
    constexpr int   kCrop = 24;                                   // el recorte de la ventana: se corta por la linea oscura del marco
    constexpr int   kFaderX = 1314, kTop = 140, kBottom = 753;       // pista del fader: de y=140 (+6 dB) a y=753 (-inf); a 43.5 del valor de abajo, igual que las perillas
    constexpr float kBtnCy = 889.25f, kBtnHit = 38.0f;               // centro vertical de los botones y radio (cuadrado) donde responden al clic
    constexpr int   kNumFrames = 136, kAtlasCols = 12;               // un cuadro del puntero cada 2 grados
    const Colour kText (240, 240, 240);    // texto sobre el fondo oscuro

    // zona (en pixeles logicos) que ocupa el vumetro: se repinta solo eso cuando cambia
    Rectangle<int> meterArea (float kScale)
    {
        return { roundToInt (1464.0f * kScale), roundToInt (116.0f * kScale), roundToInt (64.0f * kScale) + 2, roundToInt (652.0f * kScale) + 4 };
    }

    constexpr float kMeterPitch = (float) (kBottom - 4 - (kTop + 4)) / 78.0f;   // alto de cada barrita del vumetro (78 en total, 5 pixeles de pantalla retina cada una); la de arriba queda separada del marco igual que la de abajo
    constexpr float kValueBase = 816.0f;                                  // linea de base de los valores bajo el fader y el vumetro (misma distancia que el valor bajo Depth)

    Image img (const void* d, int n) { return ImageCache::getFromMemory (d, n); }

    // marcas del fader / medidor: +6, 0, -6, -12, -24, -48, -inf equiespaciadas (igual que el parametro Output)
    float dbToFaderNorm (float v)
    {
        static const float db[7] = { -60.0f, -48.0f, -24.0f, -12.0f, -6.0f, 0.0f, 6.0f };
        if (v <= db[0]) return 0.0f;
        if (v >= db[6]) return 1.0f;
        int k = 0;
        while (k < 5 && v > db[k + 1]) ++k;
        return ((float) k + (v - db[k]) / (db[k + 1] - db[k])) / 6.0f;
    }

    String dbText (float v)
    {
        if (v <= -59.99f) return "-inf dB";
        return String (v, 2) + " dB";
    }

    // Componente que dibuja en unidades de diseno (la mitad en pixeles logicos)
    struct DComp : Component
    {
        void setDesign (Rectangle<int> r) { design = r; applyBounds(); }
        void setScale (float s) { scale = s; applyBounds(); }
        void applyBounds()
        {
            const auto& r = design;
            const int x0 = (int) std::floor ((float) (r.getX() - kCrop) * scale), y0 = (int) std::floor ((float) (r.getY() - kCrop) * scale);
            const int x1 = (int) std::ceil ((float) (r.getRight() - kCrop) * scale), y1 = (int) std::ceil ((float) (r.getBottom() - kCrop) * scale);
            setBounds (x0, y0, x1 - x0, y1 - y0);
            repaint();
        }
        virtual void closeEdit() {}   // cierra la cajita de valor si esta abierta
        float scale = kDefaultScale;
        Rectangle<int> design;
        void beginDesign (Graphics& g) const
        {
            g.addTransform (AffineTransform::translation ((float) -kCrop, (float) -kCrop).scaled (scale).translated ((float) -getX(), (float) -getY()));
            g.setImageResamplingQuality (Graphics::highResamplingQuality);
        }
        Point<float> toDesign (Point<float> local) const { return (local + getPosition().toFloat()) / scale + Point<float> ((float) kCrop, (float) kCrop); }
        Point<float> designOf (int x, int y) const { return toDesign ({ (float) x, (float) y }); }
    };

    struct Ctl : DComp
    {
        explicit Ctl (RangedAudioParameter& p) : par (p), att (p, [this] (float) { repaint(); })
        {
            att.sendInitialUpdate();
        }
        float norm() const { return par.getValue(); }
        float value() const { return par.convertFrom0to1 (norm()); }
        void setNorm (float n) { att.setValueAsPartOfGesture (par.convertFrom0to1 (jlimit (0.0f, 1.0f, n))); }
        // Option (Alt) + clic: vuelve al valor por defecto. Devuelve true si lo hizo (y hay que ignorar el resto del gesto)
        bool altReset (const MouseEvent& e)
        {
            resetting = e.mods.isAltDown() || e.mods.isPopupMenu();   // clic derecho: lo atiende el menu del editor
            if (e.mods.isAltDown()) att.setValueAsCompleteGesture (defaultValue());
            return resetting;
        }
        bool resetting = false;
        float defaultValue() const { return par.convertFrom0to1 (par.getDefaultValue()); }

        RangedAudioParameter& par;
        ParameterAttachment att;
    };

    // perilla rotativa (Depth y Fade): cuerpo fijo + cuadro del puntero con la luz siempre desde arriba
    struct Knob : Ctl, Timer
    {
        Knob (RangedAudioParameter& p, Image bodyImg, Image atlasImg, float cx_, float cy_, float r_, float rMid_,
              int half_, Rectangle<int> box, Point<int> bodyPos_, int textBase_, float textSize_, bool isDepth_, Typeface::Ptr tf_, float steps_ = 12.0f)
            : Ctl (p), body (bodyImg), atlas (atlasImg), cx (cx_), cy (cy_), r (r_), rMid (rMid_), half (half_),
              bodyPos (bodyPos_), textBase (textBase_), textSize (textSize_), isDepth (isDepth_), tf (tf_), nSteps (steps_)
        {
            setDesign (box);
        }

        bool hitTest (int x, int y) override
        {
            const auto p = designOf (x, y);
            return p.getDistanceFrom ({ cx, cy }) <= r
                || onTickRing (p)
                || (std::abs (p.x - cx) <= 100.0f && p.y >= (float) textBase - 30.0f && p.y <= (float) textBase + 10.0f);
        }

        // anillo de ticks y numeros alrededor de la perilla: un clic ahi la lleva directo a ese valor
        float ringDegrees (Point<float> p) const { return radiansToDegrees (std::atan2 (p.x - cx, cy - p.y)); }   // 0 = arriba, positivo = horario
        bool onTickRing (Point<float> p) const
        {
            const float d = p.getDistanceFrom ({ cx, cy });
            return d > r && d <= r + (isDepth ? 75.0f : 40.0f) && std::abs (ringDegrees (p)) <= 138.0f;
        }
        void jumpTo (Point<float> p)
        {
            const float n = snapTick ((ringDegrees (p) + 135.0f) / 270.0f);
            att.beginGesture();
            if (std::abs (n - norm()) > 1e-4f) setTick (n);
            att.endGesture();
        }

        void paint (Graphics& g) override
        {
            beginDesign (g);
            g.drawImageAt (body, bodyPos.x, bodyPos.y);
            const int idx = jlimit (0, kNumFrames - 1, roundToInt (norm() * (float) (kNumFrames - 1)));
            const int cell = half * 2;
            const float a = degreesToRadians (-135.0f + 2.0f * (float) idx);
            const int dx = roundToInt (cx + rMid * std::sin (a) - (float) half);
            const int dy = roundToInt (cy - rMid * std::cos (a) - (float) half);
            g.drawImage (atlas, dx, dy, cell, cell, (idx % kAtlasCols) * cell, (idx / kAtlasCols) * cell, cell, cell);

            g.setColour (kText);
            g.setFont (Font (FontOptions (tf).withPointHeight (textSize)));
            g.drawSingleLineText (isDepth ? dbText (value()) : par.getText (norm(), 32), roundToInt (cx), textBase, Justification::horizontallyCentred);
        }

        // Las dos perillas saltan de tick en tick, incluidos los intermedios: Depth 25 posiciones (-inf .. +6), Fade 11 (0 .. 100 %, de a 10)
        float steps() const { return isDepth ? 24.0f : nSteps; }
        float snapTick (float n) const { return std::round (jlimit (0.0f, 1.0f, n) * steps()) / steps(); }
        void syncAcc() { if (std::abs (norm() - lastSet) > 1e-3f) acc = lastSet = norm(); }   // si el valor cambio por otro lado, se sigue desde ahi
        void setTick (float n) { setNorm (n); lastSet = norm(); }                      // un tick: cambia el valor

        void mouseDown (const MouseEvent& e) override
        {
            if (altReset (e)) return;
            const auto pd = toDesign (e.position);
            if (onTickRing (pd)) { resetting = true; jumpTo (pd); return; }   // clic en un tick o numero: salta ahi (el resto del gesto se ignora)
            att.beginGesture(); last = e.position; acc = norm(); lastSet = acc;
        }
        void mouseDrag (const MouseEvent& e) override
        {
            if (resetting) return;
            const auto d = e.position - last; last = e.position;
            acc = jlimit (0.0f, 1.0f, acc + (d.x - d.y) / (isDepth ? 300.0f : 260.0f));
            const float n = snapTick (acc);
            if (std::abs (n - norm()) > 1e-4f) setTick (n);
        }
        void mouseUp (const MouseEvent&) override { if (! resetting) att.endGesture(); }
        // Rueda / dos dedos: la perilla sigue al gesto y salta de tick en tick. En el trackpad, cuando macOS termina de mandar
        // la inercia del gesto, la perilla sigue un poco mas por su cuenta (velocidad que se va apagando) para que se sienta fluida
        void mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w) override
        {
            syncAcc();
            applyWheel (w.deltaY);
            const auto now = Time::getMillisecondCounter();
            if (w.isSmooth)
            {
                const float dt = (float) jlimit (4u, 50u, now - lastWheel);
                vel = 0.5f * vel + 0.5f * (w.deltaY / dt);           // velocidad del gesto (deltaY por ms)
                startTimer (16);
            }
            else vel = 0.0f;
            lastWheel = now;
        }
        void applyWheel (float deltaY)
        {
            acc = jlimit (0.0f, 1.0f, acc - deltaY * 0.1f);        // rueda invertida
            const float n = snapTick (acc);
            if (std::abs (n - norm()) < 1e-4f) return;
            att.beginGesture();
            setTick (n);
            att.endGesture();
        }
        void timerCallback() override
        {
            if (Time::getMillisecondCounter() - lastWheel < 40) return;   // todavia llegan eventos (la inercia de macOS): no se mete
            if (std::abs (vel) < 2e-4f) { vel = 0.0f; stopTimer(); return; }
            syncAcc();
            applyWheel (vel * 16.0f * 1.5f);   // 1.5 = cuanto empuja la inercia propia
            vel *= 0.94f;                                              // la velocidad se apaga de a poco
        }

        Image body, atlas; float cx, cy, r, rMid; int half; Point<int> bodyPos; int textBase; float textSize; bool isDepth; Typeface::Ptr tf; float nSteps;
        Point<float> last; float acc = 0.0f, lastSet = 0.0f, vel = 0.0f; uint32 lastWheel = 0;
    };

    struct ModeKnob : Ctl
    {
        ModeKnob (RangedAudioParameter& p, Image h, Image w) : Ctl (p), halfImg (h), wholeImg (w)
        {
            setDesign ({ 772, 656, 272, 272 });
        }
        bool hitTest (int x, int y) override { return designOf (x, y).getDistanceFrom ({ kModeCx, kModeCy }) <= kModeR; }
        void paint (Graphics& g) override
        {
            beginDesign (g);
            g.drawImageAt (norm() >= 0.5f ? wholeImg : halfImg, 773, 657);
        }
        void setWhole (bool w) { att.setValueAsCompleteGesture (w ? 1.0f : 0.0f); }
        // clic y arrastre: hacia la derecha/abajo = Whole, hacia la izquierda/arriba = Half (un clic solo no cambia nada)
        void mouseDown (const MouseEvent& e) override { if (altReset (e)) return; start = e.position; topHalf = toDesign (e.position).y < kModeCy; att.beginGesture(); }
        void mouseDrag (const MouseEvent& e) override
        {
            if (resetting) return;
            const auto d = e.position - start;
            const float v = (topHalf ? -d.x : d.x) + d.y;   // en la mitad de arriba el movimiento horizontal va al reves
            if (v > 12.0f && norm() < 0.5f)       att.setValueAsPartOfGesture (1.0f);
            else if (v < -12.0f && norm() >= 0.5f) att.setValueAsPartOfGesture (0.0f);
        }
        void mouseUp (const MouseEvent&) override { if (! resetting) att.endGesture(); }
        // rueda: mismo sentido que las perillas y el fader (hacia un lado Whole, hacia el otro Half)
        void mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w) override
        {
            if (w.deltaY < 0.0f && norm() < 0.5f)       setWhole (true);
            else if (w.deltaY > 0.0f && norm() >= 0.5f) setWhole (false);
        }
        Point<float> start; bool topHalf = false;
        Image halfImg, wholeImg;
    };

    struct Fader : Ctl
    {
        Fader (RangedAudioParameter& p, Typeface::Ptr tf_)
            : Ctl (p),
              slot (img (BinaryData::ui_fader_slot_png, BinaryData::ui_fader_slot_pngSize)),
              capImg (img (BinaryData::ui_fader_cap_png, BinaryData::ui_fader_cap_pngSize)), tf (tf_)
        {
            setDesign ({ 1234, 112, 160, 736 });
            ed.setJustification (Justification::centred);
            ed.setSelectAllWhenFocused (true);
            ed.setColour (TextEditor::backgroundColourId, Colour (24, 24, 26));
            ed.setColour (TextEditor::textColourId, kText);
            ed.setColour (TextEditor::outlineColourId, kText);
            ed.setColour (TextEditor::focusedOutlineColourId, kText);
            ed.setColour (CaretComponent::caretColourId, kText);
            ed.onReturnKey = [this] { applyEdit(); };
            ed.onEscapeKey = [this] { ed.setVisible (false); };
            ed.onFocusLost = [this] { ed.setVisible (false); };
            addChildComponent (ed);
        }

        void closeEdit() override { ed.setVisible (false); }

        // doble clic: aparece una cajita junto a la perilla del fader para escribir el valor con el teclado
        void startEdit()
        {
            ed.setFont (Font (FontOptions (tf).withPointHeight (26.0f * scale)));
            const float cy = yOf (norm());
            const float lx = ((float) kFaderX - 75.0f - (float) kCrop) * scale - (float) getX(), ly = (cy - 20.0f - (float) kCrop) * scale - (float) getY();
            ed.setBounds ((int) lx, (int) ly, roundToInt (150.0f * scale), roundToInt (40.0f * scale));
            ed.setText (value() <= -59.99f ? String ("-inf") : String (value(), 2), false);
            ed.setVisible (true);
            ed.grabKeyboardFocus();
            ed.selectAll();
        }
        void applyEdit()
        {
            const String txt = ed.getText().trim().toLowerCase();
            ed.setVisible (false);
            const bool valido = txt.containsAnyOf ("0123456789") || txt.contains ("inf") || txt.startsWith ("off") || txt.startsWith ("sil") || txt.startsWith ("mute");
            if (! valido) return;
            att.setValueAsCompleteGesture (par.convertFrom0to1 (jlimit (0.0f, 1.0f, par.getValueForText (txt))));
        }
        bool hitTest (int x, int y) override { const auto p = designOf (x, y); return p.y <= (float) kBottom + 24.0f; }
        float yOf (float n) const { return (float) kBottom - n * (float) (kBottom - kTop); }
        void paint (Graphics& g) override
        {
            beginDesign (g);
            const int cy = roundToInt (yOf (norm()));
            g.drawImageAt (slot, kFaderX - slot.getWidth() / 2, kTop - 25);            // recorrido
            g.drawImageAt (capImg, kFaderX - capImg.getWidth() / 2, cy - capImg.getHeight() / 2);   // perilla
        }
        void setFromY (float y) { setNorm (((float) kBottom - (y - offset)) / (float) (kBottom - kTop)); }
        void mouseDown (const MouseEvent& e) override
        {
            if (altReset (e)) return;
            // solo se arrastra si el clic cae sobre la perilla del fader; en cualquier otro lado no pasa nada
            const auto p = toDesign (e.position);
            const float c = yOf (norm());
            dragging = std::abs (p.y - c) <= 38.0f && std::abs (p.x - (float) kFaderX) <= 72.0f;
            if (! dragging) return;
            att.beginGesture();
            offset = p.y - c;
        }
        void mouseDrag (const MouseEvent& e) override { if (! resetting && dragging) setFromY (toDesign (e.position).y); }
        void mouseUp (const MouseEvent&) override { if (! resetting && dragging) att.endGesture(); dragging = false; }
        void mouseDoubleClick (const MouseEvent& e) override { if (! e.mods.isAltDown() && ! e.mods.isPopupMenu()) startEdit(); }
        void mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w) override
        {
            att.beginGesture(); setNorm (norm() - w.deltaY * 0.08f); att.endGesture();   // rueda invertida (igual que las perillas)
        }
        Image slot, capImg; float offset = 0.0f; bool dragging = false; Typeface::Ptr tf;
        TextEditor ed;
    };

    struct ToggleBtn : Ctl
    {
        ToggleBtn (RangedAudioParameter& p, int cx)
            : Ctl (p), centreX (cx),
              offImg (img (BinaryData::ui_btn_off_png, BinaryData::ui_btn_off_pngSize)),
              onImg (img (BinaryData::ui_btn_on_png, BinaryData::ui_btn_on_pngSize))
        {
            setDesign ({ cx - 60, 828, 120, 124 });
        }
        void paint (Graphics& g) override
        {
            beginDesign (g);
            g.drawImageAt (norm() >= 0.5f ? onImg : offImg, centreX - 60, 829);
        }
        // solo responde sobre el boton (no sobre el resplandor que lo rodea)
        bool hitTest (int x, int y) override
        {
            const auto p = designOf (x, y);
            return std::abs (p.x - (float) centreX) <= kBtnHit && std::abs (p.y - kBtnCy) <= kBtnHit;
        }
        void mouseDown (const MouseEvent& e) override { if (e.mods.isPopupMenu()) return; att.setValueAsCompleteGesture (norm() >= 0.5f ? 0.0f : 1.0f); }
        int centreX; Image offImg, onImg;
    };
}

// Capa superior: dibuja los valores fijos que van debajo del fader (Output) y del vumetro (pico del sidechain).
// Es una capa encima de todo (no recibe clics), asi se ve por encima del fader.
struct ValueOverlay : Component
{
    explicit ValueOverlay (Typeface::Ptr t) : tf (t) { setInterceptsMouseClicks (false, false); }
    static Rectangle<float> meterZone() { return { 1492.0f, 132.0f, 60.0f, 621.0f }; }   // en unidades de diseno

    void paint (Graphics& g) override
    {
        {   // valores fijos bajo el fader y el vumetro (mismo tamano de letra que el valor de Depth)
            Graphics::ScopedSaveState ss (g);
            g.addTransform (AffineTransform::translation ((float) -kCrop, (float) -kCrop).scaled (scale));
            g.setColour (kText);
            g.setFont (Font (FontOptions (tf).withPointHeight (26.0f)));
            g.drawSingleLineText (faderValText, kFaderX, roundToInt (kValueBase), Justification::horizontallyCentred);
            g.drawSingleLineText (peakValText, 1521, roundToInt (kValueBase), Justification::horizontallyCentred);
        }
    }
    String faderValText = "0.00 dB", peakValText = "-inf dB";
    Rectangle<int> valuesArea() const { return { roundToInt ((1230.0f - (float) kCrop) * scale), roundToInt ((780.0f - (float) kCrop) * scale), roundToInt (380.0f * scale), roundToInt (50.0f * scale) }; }
    float peakDb = -100.0f; float scale = kDefaultScale; Typeface::Ptr tf;
};

struct PhantomAsymEditor::Impl
{
    Impl (PhantomAsymProcessor& p)
        : bg (img (BinaryData::ui_bg_png, BinaryData::ui_bg_pngSize)),
          tfReg (Typeface::createSystemTypefaceFor (BinaryData::InterRegular_ttf, (size_t) BinaryData::InterRegular_ttfSize)),
          depth (*p.apvts.getParameter ("depthdb"),
                 img (BinaryData::ui_depth_body_png, BinaryData::ui_depth_body_pngSize),
                 img (BinaryData::ui_depth_ptr_png, BinaryData::ui_depth_ptr_pngSize),
                 kDepthCx, kDepthCy, kDepthR, (0.475f + 0.85f) / 2.0f * kDepthR, 84,
                 { 80, 192, 660, 692 }, { 85, 197 }, 855, 26.0f, true, tfReg),
          fade (*p.apvts.getParameter ("fade"),
               img (BinaryData::ui_fade_body_png, BinaryData::ui_fade_body_pngSize),
               img (BinaryData::ui_fade_ptr_png, BinaryData::ui_fade_ptr_pngSize),
               kFadeCx, kFadeCy, kFadeR, (0.40f / 0.775f + 1.1f) / 2.0f * kFadeR, 56,
               { 772, 116, 272, 336 }, { 773, 117 }, 427, 26.0f, false, tfReg, 10.0f),
          mode (*p.apvts.getParameter ("mode"),
                img (BinaryData::ui_mode_half_png, BinaryData::ui_mode_half_pngSize),
                img (BinaryData::ui_mode_whole_png, BinaryData::ui_mode_whole_pngSize)),
          fader (*p.apvts.getParameter ("output"), tfReg),
          bypass (*p.apvts.getParameter ("bypass"), 1314),
          listen (*p.apvts.getParameter ("listen"), 1521),
          values (tfReg)
    {}

    Image bg; Typeface::Ptr tfReg;
    Knob depth, fade; ModeKnob mode; Fader fader; ToggleBtn bypass, listen; ValueOverlay values;
};

template <typename F>
static void forEachControl (PhantomAsymEditor::Impl& i, F f)
{
    for (DComp* c : { (DComp*) &i.depth, (DComp*) &i.fade, (DComp*) &i.mode, (DComp*) &i.fader, (DComp*) &i.bypass, (DComp*) &i.listen })
        f (*c);
}

PhantomAsymEditor::PhantomAsymEditor (PhantomAsymProcessor& p)
    : AudioProcessorEditor (&p), proc (p), impl (std::make_unique<Impl> (p))
{
    setOpaque (true);
    scale = kDefaultScale;
    forEachControl (*impl, [this] (DComp& c) { c.setScale (scale); addAndMakeVisible (c); });
    setResizable (false, false);
    impl->values.scale = scale;
    addAndMakeVisible (impl->values);
    setSize (roundToInt (kArtW * scale), roundToInt (kArtH * scale));
    addMouseListener (this, true);                   // para enterarse de los clics sobre los controles (cierra la cajita de valor)
    startTimerHz (30);
}

PhantomAsymEditor::~PhantomAsymEditor() = default;

void PhantomAsymEditor::resized() { if (impl != nullptr) impl->values.setBounds (getLocalBounds()); }

void PhantomAsymEditor::paint (Graphics& g)
{
    g.fillAll (Colour (40, 40, 41));
    g.addTransform (AffineTransform::translation ((float) -kCrop, (float) -kCrop).scaled (scale));
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImageAt (impl->bg, 0, 0);

    // medidor de salida: barritas finas horizontales (estilo segmentado), misma escala que el fader
    constexpr int kSeg = 78;
    constexpr float kPitch = kMeterPitch;
    // las barritas se alinean a los pixeles de la pantalla (retina, 2x) para que todas se vean iguales y parejas
    auto snap = [this] (float yDesign) { const float k = scale * 2.0f; return std::round ((yDesign - (float) kCrop) * k) / k + (float) kCrop; };
    const int lit = roundToInt (dbToFaderNorm (meterShown) * (float) kSeg);
    const bool blue = proc.apvts.getRawParameterValue ("listen")->load() >= 0.5f;   // SC Listen prendido: barritas azules
    const float yBase = (float) kBottom - 4.0f;
    for (int i = 0; i < kSeg; ++i)
    {
        const float yTop = snap (yBase - (float) (i + 1) * kPitch + kPitch * 0.2f), yBot = snap (yBase - (float) i * kPitch - kPitch * 0.2f);
        const float y = yTop, kBar = yBot - yTop, kHi = 1.0f / (scale * 2.0f);
        if (i < lit)
        {
            g.setColour (blue ? Colour (62, 128, 245) : Colour (170, 170, 172));
            g.fillRect (1498.0f, y, 46.0f, kBar);
            g.setColour (blue ? Colour (150, 200, 255) : Colour (214, 214, 216));
            g.fillRect (1498.0f, y, 46.0f, kHi);
        }
        else
        {
            g.setColour (Colour (46, 46, 48));
            g.fillRect (1498.0f, y, 46.0f, kBar);
        }
    }
}

void PhantomAsymEditor::timerCallback()
{
    const bool lis = proc.apvts.getRawParameterValue ("listen")->load() >= 0.5f;
    if (lis != listenShown) { listenShown = lis; repaint (meterArea (scale)); }
    const float pk = proc.peakHoldDb.load();                                                    // el valor bajo el vumetro sigue el pico mas alto y se queda
    if (std::abs (pk - impl->values.peakDb) > 1e-3f) { impl->values.peakDb = pk; impl->values.peakValText = meterText(); impl->values.repaint (impl->values.valuesArea()); }
    const String fv = impl->fader.value() <= -59.99f ? String (CharPointer_UTF8 ("-inf dB")) : dbText (impl->fader.value());
    if (fv != impl->values.faderValText) { impl->values.faderValText = fv; impl->values.repaint (impl->values.valuesArea()); }
    const float db = proc.outMeterDb.load();
    const float next = db > meterShown ? db : jmax (db, meterShown - 1.5f);   // sube al instante, baja suave
    if (std::abs (next - meterShown) > 0.05f)
    {
        meterShown = next;
        repaint (meterArea (scale));
    }
}

String PhantomAsymEditor::meterText() const
{
    const float pk = proc.peakHoldDb.load();
    return pk <= -99.0f ? String (CharPointer_UTF8 ("-inf dB")) : String (pk, 1) + " dB";
}

void PhantomAsymEditor::mouseDown (const MouseEvent& e)
{
    // un clic en cualquier otra parte del plugin cierra la cajita de valor (los clics dentro de la cajita no)
    if (dynamic_cast<TextEditor*> (e.originalComponent) == nullptr)
        forEachControl (*impl, [] (DComp& c) { c.closeEdit(); });

    // un clic sobre el vumetro reinicia el pico que muestra el valor de abajo
    if (ValueOverlay::meterZone().contains (e.getEventRelativeTo (this).position / scale + Point<float> ((float) kCrop, (float) kCrop)))
    {
        proc.peakHoldDb.store (-100.0f);
        impl->values.peakDb = -100.0f; impl->values.peakValText = meterText(); impl->values.repaint (impl->values.valuesArea());
    }

    if (e.eventComponent != this) return;
    // clic en las palabras "Half" / "Whole" elige el modo
    const auto p = e.position / scale + Point<float> ((float) kCrop, (float) kCrop);
    if (p.y >= 935.0f && p.y <= 1000.0f)
    {
        if (p.x >= 760.0f && p.x <= 840.0f)       impl->mode.setWhole (false);
        else if (p.x >= 980.0f && p.x <= 1060.0f) impl->mode.setWhole (true);
    }
}
