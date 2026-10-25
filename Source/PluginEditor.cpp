#include "PluginEditor.h"
#include "BinaryData.h"

using namespace juce;

namespace
{
    // ---- geometria en unidades de diseno (ventana 1696 x 1044) ----
    constexpr float kDepthCx = 410.0f, kDepthCy = 522.0f, kDepthR = 270.0f;
    constexpr float kPolCx = 908.0f, kPolCy = 252.0f, kPolR = 112.0f;
    constexpr float kModeCx = 908.0f, kModeCy = 792.0f, kModeR = 112.0f;
    constexpr float kDefaultScale = 0.32f;                        // tamano de la ventana (0.5 = 100%, 0.32 = 64%)
    constexpr float kArtW = 1648.0f, kArtH = 996.0f;              // arte ya recortado
    constexpr int   kCrop = 24;                                   // el recorte de la ventana: se corta por la linea oscura del marco
    constexpr int   kFaderX = 1314, kTop = 140, kBottom = 792;       // pista del fader: de y=140 (+6 dB) a y=792 (-inf)
    constexpr float kBtnCy = 889.25f, kBtnHit = 38.0f;               // centro vertical de los botones y radio (cuadrado) donde responden al clic
    constexpr int   kNumFrames = 136, kAtlasCols = 12;               // un cuadro del puntero cada 2 grados
    const Colour kText (240, 240, 240);    // texto sobre el fondo oscuro

    // zona (en pixeles logicos) que ocupa el vumetro: se repinta solo eso cuando cambia
    Rectangle<int> meterArea (float kScale)
    {
        return { roundToInt (1464.0f * kScale), roundToInt (116.0f * kScale), roundToInt (64.0f * kScale) + 2, roundToInt (652.0f * kScale) + 4 };
    }

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

    // perilla rotativa (Depth y Polarity): cuerpo fijo + cuadro del puntero con la luz siempre desde arriba
    struct Knob : Ctl
    {
        Knob (RangedAudioParameter& p, Image bodyImg, Image atlasImg, float cx_, float cy_, float r_, float rMid_,
              int half_, Rectangle<int> box, Point<int> bodyPos_, int textBase_, float textSize_, bool isDepth_, Typeface::Ptr tf_)
            : Ctl (p), body (bodyImg), atlas (atlasImg), cx (cx_), cy (cy_), r (r_), rMid (rMid_), half (half_),
              bodyPos (bodyPos_), textBase (textBase_), textSize (textSize_), isDepth (isDepth_), tf (tf_)
        {
            setDesign (box);
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

        // doble clic: aparece una cajita sobre el valor para escribirlo con el teclado
        void startEdit()
        {
            ed.setFont (Font (FontOptions (tf).withPointHeight (textSize * scale)));
            const float lx = (cx - 90.0f - (float) kCrop) * scale - (float) getX(), ly = ((float) textBase - 30.0f - (float) kCrop) * scale - (float) getY();
            ed.setBounds ((int) lx, (int) ly, roundToInt (180.0f * scale), roundToInt (40.0f * scale));
            String t = isDepth ? (value() <= -59.99f ? String ("-inf") : String (value(), 2)) : par.getText (norm(), 32);
            ed.setText (t, false);
            ed.setVisible (true);
            ed.grabKeyboardFocus();
            ed.selectAll();
        }
        void applyEdit()
        {
            const String txt = ed.getText().trim().toLowerCase();
            ed.setVisible (false);
            // texto que no se parece a un valor (vacio, letras sueltas): se ignora en vez de mandar el valor a 0
            const bool valido = txt.containsAnyOf ("0123456789") || txt.contains ("inf") || txt.startsWith ("pos") || txt.startsWith ("neg")
                                || txt.startsWith ("off") || txt.startsWith ("sil") || txt.startsWith ("mute");
            if (! valido) return;
            const float n = par.getValueForText (txt);
            att.setValueAsCompleteGesture (par.convertFrom0to1 (jlimit (0.0f, 1.0f, n)));
        }

        bool hitTest (int x, int y) override
        {
            const auto p = designOf (x, y);
            return p.getDistanceFrom ({ cx, cy }) <= r
                || (std::abs (p.x - cx) <= 100.0f && p.y >= (float) textBase - 30.0f && p.y <= (float) textBase + 10.0f);
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

        void mouseDown (const MouseEvent& e) override { if (altReset (e)) return; att.beginGesture(); last = e.position; }
        void mouseDrag (const MouseEvent& e) override
        {
            if (resetting) return;
            const auto d = e.position - last; last = e.position;
            const float k = e.mods.isShiftDown() ? 0.1f : 1.0f;
            setNorm (norm() + k * (d.x - d.y) / (isDepth ? 300.0f : 260.0f));
        }
        void mouseUp (const MouseEvent&) override { if (! resetting) att.endGesture(); }
        void mouseDoubleClick (const MouseEvent& e) override { if (! e.mods.isAltDown()) startEdit(); }
        void mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w) override
        {
            att.beginGesture(); setNorm (norm() - w.deltaY * 0.1f); att.endGesture();   // rueda invertida
        }

        Image body, atlas; float cx, cy, r, rMid; int half; Point<int> bodyPos; int textBase; float textSize; bool isDepth; Typeface::Ptr tf;
        Point<float> last;
        TextEditor ed;
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
            att.beginGesture(); setNorm (norm() + w.deltaY * 0.08f); att.endGesture();
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
          pol (*p.apvts.getParameter ("polarity"),
               img (BinaryData::ui_pol_body_png, BinaryData::ui_pol_body_pngSize),
               img (BinaryData::ui_pol_ptr_png, BinaryData::ui_pol_ptr_pngSize),
               kPolCx, kPolCy, kPolR, (0.40f / 0.775f + 1.1f) / 2.0f * kPolR, 56,
               { 772, 116, 272, 336 }, { 773, 117 }, 427, 26.0f, false, tfReg),
          mode (*p.apvts.getParameter ("mode"),
                img (BinaryData::ui_mode_half_png, BinaryData::ui_mode_half_pngSize),
                img (BinaryData::ui_mode_whole_png, BinaryData::ui_mode_whole_pngSize)),
          fader (*p.apvts.getParameter ("output"), tfReg),
          bypass (*p.apvts.getParameter ("bypass"), 1314),
          listen (*p.apvts.getParameter ("listen"), 1521)
    {}

    Image bg; Typeface::Ptr tfReg;
    Knob depth, pol; ModeKnob mode; Fader fader; ToggleBtn bypass, listen;
};

template <typename F>
static void forEachControl (PhantomAsymEditor::Impl& i, F f)
{
    for (DComp* c : { (DComp*) &i.depth, (DComp*) &i.pol, (DComp*) &i.mode, (DComp*) &i.fader, (DComp*) &i.bypass, (DComp*) &i.listen })
        f (*c);
}

PhantomAsymEditor::PhantomAsymEditor (PhantomAsymProcessor& p)
    : AudioProcessorEditor (&p), proc (p), impl (std::make_unique<Impl> (p))
{
    setOpaque (true);
    scale = kDefaultScale;
    forEachControl (*impl, [this] (DComp& c) { c.setScale (scale); addAndMakeVisible (c); });
    setResizable (false, false);
    setSize (roundToInt (kArtW * scale), roundToInt (kArtH * scale));
    addMouseListener (this, true);                   // para enterarse de los clics sobre los controles (cierra la cajita de valor)
    startTimerHz (30);
}

PhantomAsymEditor::~PhantomAsymEditor() = default;

void PhantomAsymEditor::resized() {}

void PhantomAsymEditor::paint (Graphics& g)
{
    g.fillAll (Colour (40, 40, 41));
    g.addTransform (AffineTransform::translation ((float) -kCrop, (float) -kCrop).scaled (scale));
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImageAt (impl->bg, 0, 0);

    // medidor de salida estereo: dos columnas de barritas finas (L a la izquierda, R a la derecha), misma escala que el fader
    constexpr int kSeg = 81;
    constexpr float kPitch = 8.0f, kBar = 4.5f, kColW = 21.0f, kColX[2] = { 1498.0f, 1523.0f };
    const bool blue = proc.apvts.getRawParameterValue ("listen")->load() >= 0.5f;   // Input Listen prendido: barritas azules
    const float yBase = (float) kBottom - 4.0f;
    for (int c = 0; c < 2; ++c)
    {
        const int lit = roundToInt (dbToFaderNorm (meterShown[c]) * (float) kSeg);
        for (int i = 0; i < kSeg; ++i)
        {
            const float y = yBase - (float) (i + 1) * kPitch + (kPitch - kBar) * 0.5f;
            if (i < lit)
            {
                g.setColour (blue ? Colour (62, 128, 245) : Colour (170, 170, 172));
                g.fillRect (kColX[c], y, kColW, kBar);
                g.setColour (blue ? Colour (150, 200, 255) : Colour (214, 214, 216));
                g.fillRect (kColX[c], y, kColW, 1.2f);
            }
            else
            {
                g.setColour (Colour (46, 46, 48));
                g.fillRect (kColX[c], y, kColW, kBar);
            }
        }
    }
}

void PhantomAsymEditor::timerCallback()
{
    const bool lis = proc.apvts.getRawParameterValue ("listen")->load() >= 0.5f;
    if (lis != listenShown) { listenShown = lis; repaint (meterArea (scale)); }
    bool changed = false;
    for (int c = 0; c < 2; ++c)
    {
        const float db = proc.outMeterDb[c].load();
        const float next = db > meterShown[c] ? db : jmax (db, meterShown[c] - 1.5f);   // sube al instante, baja suave
        if (std::abs (next - meterShown[c]) > 0.05f) { meterShown[c] = next; changed = true; }
    }
    if (changed) repaint (meterArea (scale));
}

void PhantomAsymEditor::mouseDown (const MouseEvent& e)
{
    // un clic en cualquier otra parte del plugin cierra la cajita de valor (los clics dentro de la cajita no)
    if (dynamic_cast<TextEditor*> (e.originalComponent) == nullptr)
        forEachControl (*impl, [] (DComp& c) { c.closeEdit(); });

    if (e.eventComponent != this) return;
    // clic en las palabras "Half" / "Whole" elige el modo
    const auto p = e.position / scale + Point<float> ((float) kCrop, (float) kCrop);
    if (p.y >= 935.0f && p.y <= 1000.0f)
    {
        if (p.x >= 760.0f && p.x <= 840.0f)       impl->mode.setWhole (false);
        else if (p.x >= 980.0f && p.x <= 1060.0f) impl->mode.setWhole (true);
    }
}
