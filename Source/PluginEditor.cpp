#include "PluginEditor.h"
#include "BinaryData.h"

using namespace juce;

namespace
{
    // ---- geometria en unidades de diseno (ventana 1671 x 1044) ----
    constexpr float kDepthCx = 378.0f, kDepthCy = 522.0f, kDepthR = 249.0f;
    constexpr float kFadeCx = 877.0f, kFadeCy = 273.0f, kFadeR = 125.0f;
    constexpr float kModeCx = 877.0f, kModeCy = 771.0f, kModeR = 125.0f;
    constexpr float kDefaultScale = 321.0f / 996.0f;               // tamano de la ventana: 321 de alto (y 523 de ancho)
    constexpr float kArtW = 1682.5f, kArtH = 996.0f;              // arte ya recortado
    constexpr float kCropX = 4.5f;                                // recorte izquierdo (el marco izquierdo esta mas a la izquierda para que borde->perilla = ancho/4 de la perilla)
    constexpr int   kCrop = 24;                                   // el recorte de la ventana: se corta por la linea oscura del marco
    constexpr float kFaderX = 1312.0f;
    constexpr float kTop = 148.5f, kBottom = 708.75f;       // pista del fader: de y=129 (+6 dB) a y=742 (-inf); a 43.5 del valor de abajo, igual que las perillas
    constexpr float kBtnCy = 864.25f, kBtnHit = 31.0f;               // centro vertical de los botones y radio (cuadrado) donde responden al clic
    constexpr int   kAtlasCols = 12;                                  // un cuadro del puntero cada 2 grados (151 cuadros, -150..150)
    const Colour kText (240, 240, 240);    // texto sobre el fondo oscuro

    // zona (en pixeles logicos) que ocupa el vumetro: se repinta solo eso cuando cambia
    Rectangle<int> meterArea (float kScale)
    {
        return { roundToInt (1493.0f * kScale), roundToInt (124.0f * kScale), roundToInt (63.0f * kScale) + 2, roundToInt (599.75f * kScale) + 4 };
    }

    constexpr float kMeterPitch = (kBottom - 2.5f - (kTop + 2.5f)) / 77.0f;   // alto de cada barrita del vumetro (77 en total = 11 por cada tramo de la escala del fader, 10 pixeles retina cada una); la de arriba queda separada del marco igual que la de abajo
    constexpr float kValueBase = 783.7f;                                  // linea de base de los valores bajo el fader y el vumetro (misma distancia que el valor bajo Depth)

    Image img (const void* d, int n) { return ImageCache::getFromMemory (d, n); }

    // Texto centrado dibujado como contorno (sin el engrosado que le agrega el suavizado de letras del sistema),
    // para que se vea igual de fino que los textos del fondo.

    // coloca el editor justo sobre el numero: el editor mide 170 x 52 unidades de diseno y se escala con una transformacion
    // (asi la posicion es exacta, sin redondear a pixeles enteros) y su linea de base cae en la del numero fijo
    void placeInPlaceEditor (TextEditor& t, Typeface::Ptr tf, float cxDesign, float baseDesign, float xTweak, float scale, Component& parent)
    {
        const Font f (FontOptions (tf).withPointHeight (38.0f));
        t.setFont (f);
        constexpr float w = 170.0f, h = 52.0f;
        const float topDesign = baseDesign - ((h - f.getHeight()) * 0.5f + f.getAscent());   // la linea de base queda donde esta el numero
        const float lx = (cxDesign - w * 0.5f + xTweak - kCropX) * scale - (float) parent.getX();
        const float ly = (topDesign - (float) kCrop) * scale - (float) parent.getY();
        t.setBounds (0, 0, roundToInt (w), roundToInt (h));
        t.setTransform (AffineTransform::scale (scale).translated (lx, ly));
    }

    // editor de texto sin caja: fondo transparente, sin borde ni margenes, centrado (se escribe justo sobre el numero)
    void styleInPlaceEditor (TextEditor& t)
    {
        t.setJustification (Justification::centred);
        t.setSelectAllWhenFocused (true);
        t.setIndents (0, 0);
        t.setBorder (BorderSize<int> (0));
        t.setColour (TextEditor::backgroundColourId, Colours::transparentBlack);
        t.setColour (TextEditor::outlineColourId, Colours::transparentBlack);
        t.setColour (TextEditor::focusedOutlineColourId, Colours::transparentBlack);
        t.setColour (TextEditor::textColourId, kText);
        t.setColour (TextEditor::highlightColourId, Colour (62, 128, 245).withAlpha (0.45f));
        t.setColour (TextEditor::highlightedTextColourId, kText);
        t.setColour (CaretComponent::caretColourId, kText);
    }

    void drawLightText (Graphics& g, const String& text, Typeface::Ptr tf, float pointHeight, float cx, float baseline)
    {
        const Font font (FontOptions (tf).withPointHeight (pointHeight));
        GlyphArrangement ga;
        ga.addLineOfText (font, text, 0.0f, 0.0f);
        Path p;
        ga.createPath (p);
        g.fillPath (p, AffineTransform::translation (cx - p.getBounds().getCentreX(), baseline));   // centrado por la tinta, no por el avance de las letras
    }

    // marcas del fader / medidor: +6, +3, 0, -3, -6, -12, -24, -inf equiespaciadas (igual que el parametro Output)
    float dbToFaderNorm (float v)
    {
        static const float db[8] = { -60.0f, -24.0f, -12.0f, -6.0f, -3.0f, 0.0f, 3.0f, 6.0f };
        if (v <= db[0]) return 0.0f;
        if (v >= db[7]) return 1.0f;
        int k = 0;
        while (k < 6 && v > db[k + 1]) ++k;
        return ((float) k + (v - db[k]) / (db[k + 1] - db[k])) / 7.0f;
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
            const int x0 = (int) std::floor ((float) (r.getX() - kCropX) * scale), y0 = (int) std::floor ((float) (r.getY() - kCrop) * scale);
            const int x1 = (int) std::ceil ((float) (r.getRight() - kCropX) * scale), y1 = (int) std::ceil ((float) (r.getBottom() - kCrop) * scale);
            setBounds (x0, y0, x1 - x0, y1 - y0);
            repaint();
        }
        virtual void closeEdit() {}   // cierra la cajita de valor si esta abierta
        float scale = kDefaultScale;
        Rectangle<int> design;
        void beginDesign (Graphics& g) const
        {
            g.addTransform (AffineTransform::translation (-kCropX, (float) -kCrop).scaled (scale).translated ((float) -getX(), (float) -getY()));
            g.setImageResamplingQuality (Graphics::highResamplingQuality);
        }
        Point<float> toDesign (Point<float> local) const { return (local + getPosition().toFloat()) / scale + Point<float> (kCropX, (float) kCrop); }
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
            if (! isDepth)   // Fade: doble clic para escribir el valor exacto (la cajita tiene el mismo tamano que la del fader)
            {
                ed.setJustification (Justification::centred);
                ed.setSelectAllWhenFocused (true);
                ed.setInputRestrictions (12);
                styleInPlaceEditor (ed);   // el valor se edita en su mismo lugar, sin caja
                ed.onReturnKey = [this] { applyEdit(); };
                ed.onEscapeKey = [this] { endEdit(); };
                ed.onFocusLost = [this] { endEdit(); };
                addChildComponent (ed);
            }
        }

        void closeEdit() override { endEdit(); }
        void endEdit() { if (ed.isVisible()) { ed.setVisible (false); repaint(); } }

        // doble clic: el valor de abajo se vuelve editable en su mismo lugar (solo Fade)
        void startEdit()
        {
            placeInPlaceEditor (ed, tf, cx, (float) textBase, 2.3f, scale, *this);
            ed.setText (par.getText (norm(), 32), false);   // el mismo texto que se ve (ej. "40%")
            ed.setVisible (true); repaint();   // mientras se edita, el numero fijo no se dibuja (queda solo el editable)
            ed.grabKeyboardFocus();
            ed.selectAll();
        }
        void applyEdit()
        {
            const String txt = ed.getText().trim().replaceCharacter (',', '.');
            endEdit();
            if (! txt.containsAnyOf ("0123456789")) return;   // un texto que no es un valor se ignora
            att.setValueAsCompleteGesture (par.convertFrom0to1 (jlimit (0.0f, 1.0f, par.getValueForText (txt))));
        }
        void mouseDoubleClick (const MouseEvent& e) override { if (! isDepth && ! e.mods.isAltDown() && ! e.mods.isPopupMenu()) startEdit(); }

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
            return d > r && d <= r + (isDepth ? 60.0f : 32.0f) && std::abs (ringDegrees (p)) <= halfSpan() + 3.0f;
        }
        void jumpTo (Point<float> p)
        {
            const float n = snapTick ((ringDegrees (p) + halfSpan()) / (2.0f * halfSpan()));
            att.beginGesture();
            if (std::abs (n - norm()) > 1e-4f) setTick (n);
            att.endGesture();
        }

        void paint (Graphics& g) override
        {
            beginDesign (g);
            g.drawImageAt (body, bodyPos.x, bodyPos.y);
            // Los cuadros del puntero estan cada 2 grados; se toma el mas cercano y el resto (menos de 1 grado) se corrige girando el cuadro,
            // asi el puntero queda exactamente sobre su tick (p. ej. Fade 50 % = recto hacia arriba)
            const float exactDeg = -halfSpan() + 2.0f * halfSpan() * jlimit (0.0f, 1.0f, norm());
            const int idx = jlimit (0, numFrames() - 1, roundToInt ((exactDeg + halfSpan()) / 2.0f));
            const int cell = half * 2;
            const float a = degreesToRadians (exactDeg);
            const float resid = degreesToRadians (exactDeg - (-halfSpan() + 2.0f * (float) idx));
            const AffineTransform place = AffineTransform::rotation (resid, (float) half, (float) half)
                                              .translated (cx + rMid * std::sin (a) - (float) half, cy - rMid * std::cos (a) - (float) half);
            g.saveState();
            g.addTransform (place);
            g.setImageResamplingQuality (Graphics::highResamplingQuality);
            g.drawImage (atlas, 0, 0, cell, cell, (idx % kAtlasCols) * cell, (idx / kAtlasCols) * cell, cell, cell);
            g.restoreState();

            g.setColour (kText);
            if (isDepth || ! ed.isVisible())
                drawLightText (g, isDepth ? dbText (value()) : par.getText (norm(), 32), tf, textSize, cx, (float) textBase);
        }

        // Depth salta de tick en tick, incluidos los intermedios (25 posiciones, -inf .. +6); Fade va de a 1 % (el clic en el anillo salta a uno de los 11 ticks, de a 10)
        // Fade: al arrastrar o usar la rueda se mueve de a 1 % (valores exactos); un clic en un tick o numero del anillo salta a ese tick (de a 10)
        // recorrido de las dos perillas: 300 grados (-150..150), un cuadro del puntero cada 2 grados (151 cuadros)
        static constexpr float halfSpan() { return 150.0f; }
        static constexpr int numFrames() { return 151; }
        float steps() const { return isDepth ? 24.0f : nSteps; }
        float snapTick (float n) const { return std::round (jlimit (0.0f, 1.0f, n) * steps()) / steps(); }
        float snapFine (float n) const { const float s = isDepth ? 24.0f : 100.0f; return std::round (jlimit (0.0f, 1.0f, n) * s) / s; }
        void syncAcc() { if (std::abs (norm() - lastSet) > 1e-3f) acc = lastSet = norm(); }   // si el valor cambio por otro lado, se sigue desde ahi
        void setTick (float n) { setNorm (n); lastSet = norm(); }                      // un tick: cambia el valor

        void mouseDown (const MouseEvent& e) override
        {
            if (altReset (e)) return;
            const auto pd = toDesign (e.position);
            if (! isDepth && std::abs (pd.x - cx) <= 100.0f && pd.y >= (float) textBase - 30.0f && pd.y <= (float) textBase + 10.0f)
            { resetting = true; valueClick = true; return; }   // un solo clic sobre el valor de abajo: se edita (se abre al soltar el mouse)
            if (onTickRing (pd)) { resetting = true; jumpTo (pd); return; }   // clic en un tick o numero: salta ahi (el resto del gesto se ignora)
            att.beginGesture(); last = e.position; acc = norm(); lastSet = acc;
        }
        void mouseDrag (const MouseEvent& e) override
        {
            if (resetting) return;
            const auto d = e.position - last; last = e.position;
            acc = jlimit (0.0f, 1.0f, acc + (d.x - d.y) / (isDepth ? 300.0f : 260.0f));
            const float n = snapFine (acc);
            if (std::abs (n - norm()) > 1e-4f) setTick (n);
        }
        void mouseUp (const MouseEvent& e) override
        {
            if (valueClick) { valueClick = false; if (! e.mouseWasDraggedSinceMouseDown()) startEdit(); return; }
            if (! resetting) att.endGesture();
        }
        bool valueClick = false;
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
            const float n = snapFine (acc);
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
        TextEditor ed;
        Point<float> last; float acc = 0.0f, lastSet = 0.0f, vel = 0.0f; uint32 lastWheel = 0;
    };

    struct ModeKnob : Ctl
    {
        ModeKnob (RangedAudioParameter& p, Image h, Image w) : Ctl (p), halfImg (h), wholeImg (w)
        {
            setDesign ({ 724, 618, 306, 306 });
        }
        bool hitTest (int x, int y) override { return designOf (x, y).getDistanceFrom ({ kModeCx, kModeCy }) <= kModeR; }
        void paint (Graphics& g) override
        {
            beginDesign (g);
            g.drawImageAt (norm() >= 0.5f ? wholeImg : halfImg, 726, 620);
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
            setDesign ({ 1220, 101, 184, 736 });
            ed.setJustification (Justification::centred);
            ed.setSelectAllWhenFocused (true);
            ed.setInputRestrictions (12);
            styleInPlaceEditor (ed);   // el valor se edita en su mismo lugar, sin caja
            ed.onReturnKey = [this] { applyEdit(); };
            ed.onEscapeKey = [this] { endEdit(); };
            ed.onFocusLost = [this] { endEdit(); };
            addChildComponent (ed);
        }

        void closeEdit() override { endEdit(); }
        void endEdit() { if (ed.isVisible()) { ed.setVisible (false); if (onEditing) onEditing (false); } }
        std::function<void (bool)> onEditing;   // avisa a la capa de valores para que oculte el numero fijo mientras se edita

        // doble clic: el valor de abajo del fader se vuelve editable en su mismo lugar
        void startEdit()
        {
            placeInPlaceEditor (ed, tf, kFaderX, kValueBase, 0.85f, scale, *this);
            ed.setText (dbText (value()), false);   // el mismo texto que se ve (ej. "-3.00 dB")
            ed.setVisible (true);
            if (onEditing) onEditing (true);
            ed.grabKeyboardFocus();
            ed.selectAll();
        }
        void applyEdit()
        {
            const String txt = ed.getText().trim().replaceCharacter (',', '.').toLowerCase();
            endEdit();
            const bool valido = txt.containsAnyOf ("0123456789") || txt.contains ("inf") || txt.startsWith ("off") || txt.startsWith ("sil") || txt.startsWith ("mute");
            if (! valido) return;
            att.setValueAsCompleteGesture (par.convertFrom0to1 (jlimit (0.0f, 1.0f, par.getValueForText (txt))));
        }
        bool hitTest (int x, int y) override { const auto p = designOf (x, y); return p.y <= kBottom + 24.0f || (std::abs (p.x - kFaderX) <= 95.0f && p.y >= kValueBase - 36.0f && p.y <= kValueBase + 10.0f); }   // el recorrido o el numero de abajo (doble clic para editarlo)
        float yOf (float n) const { return kBottom - n * (kBottom - kTop); }
        void paint (Graphics& g) override
        {
            beginDesign (g);
            const int cy = roundToInt (yOf (norm()));
            g.drawImageTransformed (slot, AffineTransform::translation (kFaderX - (float) slot.getWidth() * 0.5f, (float) ((int) (kTop - 0.5f) - 25)));            // recorrido
            g.drawImageTransformed (capImg, AffineTransform::translation (kFaderX - (float) capImg.getWidth() * 0.5f, (float) (cy - capImg.getHeight() / 2)));   // perilla
        }
        void setFromY (float y) { setNorm ((kBottom - (y - offset)) / (kBottom - kTop)); }
        void mouseDown (const MouseEvent& e) override
        {
            if (altReset (e)) return;
            // solo se arrastra si el clic cae sobre la perilla del fader; en cualquier otro lado no pasa nada
            const auto p = toDesign (e.position);
            if (std::abs (p.x - kFaderX) <= 95.0f && p.y >= kValueBase - 36.0f && p.y <= kValueBase + 10.0f)
            { valueClick = true; dragging = false; return; }   // un solo clic sobre el valor de abajo: se edita (se abre al soltar el mouse)
            const float c = yOf (norm());
            dragging = std::abs (p.y - c) <= 38.0f && std::abs (p.x - kFaderX) <= 72.0f;
            if (! dragging) return;
            att.beginGesture();
            offset = p.y - c;
        }
        void mouseDrag (const MouseEvent& e) override { if (! resetting && dragging) setFromY (toDesign (e.position).y); }
        void mouseUp (const MouseEvent& e) override
        {
            if (valueClick) { valueClick = false; if (! e.mouseWasDraggedSinceMouseDown()) startEdit(); return; }
            if (! resetting && dragging) att.endGesture();
            dragging = false;
        }
        bool valueClick = false;
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
        ToggleBtn (RangedAudioParameter& p, float cx)
            : Ctl (p), centreX (cx),
              offImg (img (BinaryData::ui_btn_off_png, BinaryData::ui_btn_off_pngSize)),
              onImg (img (BinaryData::ui_btn_on_png, BinaryData::ui_btn_on_pngSize))
        {
            setDesign ({ (int) std::floor (cx - 61.0f), 804, 122, 124 });
        }
        void paint (Graphics& g) override
        {
            beginDesign (g);
            g.drawImageTransformed (norm() >= 0.5f ? onImg : offImg, AffineTransform::translation (centreX - 60.0f, 804.5f));
        }
        // solo responde sobre el boton (no sobre el resplandor que lo rodea)
        bool hitTest (int x, int y) override
        {
            const auto p = designOf (x, y);
            return std::abs (p.x - centreX) <= kBtnHit && std::abs (p.y - kBtnCy) <= kBtnHit;
        }
        void mouseDown (const MouseEvent& e) override { if (e.mods.isPopupMenu()) return; att.setValueAsCompleteGesture (norm() >= 0.5f ? 0.0f : 1.0f); }
        float centreX; Image offImg, onImg;
    };
}

// Capa superior: dibuja los valores fijos que van debajo del fader (Output) y del vumetro (pico del sidechain).
// Es una capa encima de todo (no recibe clics), asi se ve por encima del fader.
struct ValueOverlay : Component
{
    explicit ValueOverlay (Typeface::Ptr t) : tf (t) { setInterceptsMouseClicks (false, false); }
    static Rectangle<float> meterZone() { return { 1499.5f, 140.5f, 60.0f, 568.75f }; }   // en unidades de diseno
    static Rectangle<float> peakTextZone() { return { 1468.5f, 752.0f, 120.0f, 37.0f }; }   // el valor numerico de abajo del vumetro

    void paint (Graphics& g) override
    {
        {   // valores fijos bajo el fader y el vumetro (mismo tamano de letra que el valor de Depth)
            Graphics::ScopedSaveState ss (g);
            g.addTransform (AffineTransform::translation (-kCropX, (float) -kCrop).scaled (scale));
            g.setColour (kText);
            if (! hideFaderVal) drawLightText (g, faderValText, tf, 38.0f, kFaderX, kValueBase);
            drawLightText (g, peakValText, tf, 38.0f, 1528.5f, kValueBase);
        }
    }
    String faderValText = "0.00 dB", peakValText = "-inf dB";
    Rectangle<int> valuesArea() const { return { roundToInt ((1226.0f - kCropX) * scale), roundToInt ((745.0f - (float) kCrop) * scale), roundToInt (400.0f * scale), roundToInt (50.0f * scale) }; }
    bool hideFaderVal = false; float peakDb = -100.0f; float scale = kDefaultScale; Typeface::Ptr tf;
};

// Agarradera invisible en la esquina inferior derecha: arrastrar hacia abajo / la derecha agranda, hacia arriba / la izquierda achica (misma proporcion);
// Option + clic vuelve al tamano original. No dibuja nada.
struct ResizeHandle : Component
{
    ResizeHandle() { setInterceptsMouseClicks (true, false); setMouseCursor (MouseCursor::BottomRightCornerResizeCursor); }   // solo cambia el puntero del mouse; no dibuja nada
    void paint (Graphics&) override {}
    void mouseDown (const MouseEvent& e) override
    {
        reset = e.mods.isAltDown();
        if (reset) { target->setSize (roundToInt (kArtW * kDefaultScale), roundToInt (kArtH * kDefaultScale)); return; }
        startY = e.getScreenY(); startX = e.getScreenX(); startH = target->getHeight(); startW = target->getWidth();
    }
    void mouseDrag (const MouseEvent& e) override
    {
        if (reset) return;
        // la esquina sigue al mouse: se promedia lo que se movio en vertical y en horizontal (cada uno convertido a escala)
        const float scY = ((float) startH + (float) (e.getScreenY() - startY)) / kArtH, scX = ((float) startW + (float) (e.getScreenX() - startX)) / kArtW;
        const float sc = jlimit (kDefaultScale, 2.0f * kDefaultScale, 0.5f * (scX + scY));
        const int w = roundToInt (kArtW * sc), h = roundToInt (kArtH * sc);
        if (w != target->getWidth() || h != target->getHeight()) target->setSize (w, h);
    }
    Component* target = nullptr; int startY = 0, startX = 0, startH = 0, startW = 0; bool reset = false;
};

struct PhantomAsymEditor::Impl
{
    Impl (PhantomAsymProcessor& p)
        : bg (img (BinaryData::ui_bg_png, BinaryData::ui_bg_pngSize)),
          tfLight (Typeface::createSystemTypefaceFor (BinaryData::InterLight_ttf, (size_t) BinaryData::InterLight_ttfSize)),
          depth (*p.apvts.getParameter ("depthdb"),
                 img (BinaryData::ui_depth_body_png, BinaryData::ui_depth_body_pngSize),
                 img (BinaryData::ui_depth_ptr_png, BinaryData::ui_depth_ptr_pngSize),
                 kDepthCx, kDepthCy, kDepthR, (0.475f + 0.85f) / 2.0f * kDepthR, 96,
                 { 57, 201, 642, 655 }, { 79, 223 }, 846, 38.0f, true, tfLight),
          fade (*p.apvts.getParameter ("fade"),
               img (BinaryData::ui_fade_body_png, BinaryData::ui_fade_body_pngSize),
               img (BinaryData::ui_fade_ptr_png, BinaryData::ui_fade_ptr_pngSize),
               kFadeCx, kFadeCy, kFadeR, (0.40f / 0.775f + 1.1f) / 2.0f * kFadeR, 78,
               { 726, 120, 303, 358 }, { 726, 122 }, 473, 38.0f, false, tfLight, 10.0f),
          mode (*p.apvts.getParameter ("mode"),
                img (BinaryData::ui_mode_half_png, BinaryData::ui_mode_half_pngSize),
                img (BinaryData::ui_mode_whole_png, BinaryData::ui_mode_whole_pngSize)),
          fader (*p.apvts.getParameter ("output"), tfLight),
          bypass (*p.apvts.getParameter ("bypass"), 1312.0f),
          listen (*p.apvts.getParameter ("listen"), 1528.5f),
          values (tfLight)
    {
        fader.onEditing = [this] (bool b) { values.hideFaderVal = b; values.repaint (values.valuesArea()); };
    }

    Image bg; Typeface::Ptr tfLight;
    Knob depth, fade; ModeKnob mode; Fader fader; ToggleBtn bypass, listen; ValueOverlay values; ResizeHandle handle;
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
    scale = jlimit (kDefaultScale, 2.0f * kDefaultScale, (float) proc.apvts.state.getProperty ("uiScale", kDefaultScale));   // recuerda el ultimo tamano (se guarda con el proyecto)
    forEachControl (*impl, [this] (DComp& c) { c.setScale (scale); addAndMakeVisible (c); });
    setResizable (false, false);
    impl->values.scale = scale;
    addAndMakeVisible (impl->values);
    impl->handle.target = this;
    addAndMakeVisible (impl->handle);                // encima de todo, invisible
    setSize (roundToInt (kArtW * scale), roundToInt (kArtH * scale));
    addMouseListener (this, true);                   // para enterarse de los clics sobre los controles (cierra la cajita de valor)
    startTimerHz (30);
}

PhantomAsymEditor::~PhantomAsymEditor() = default;

void PhantomAsymEditor::resized()
{
    if (impl == nullptr) return;
    const float newScale = (float) getWidth() / kArtW;
    if (std::abs (newScale - scale) > 1e-6f)
    {
        scale = newScale;
        forEachControl (*impl, [this] (DComp& c) { c.closeEdit(); c.setScale (scale); });
        impl->values.scale = scale;
        proc.apvts.state.setProperty ("uiScale", scale, nullptr);   // guarda el tamano para la proxima vez
        repaint();
    }
    impl->values.setBounds (getLocalBounds());
    impl->handle.setBounds (getWidth() - 24, getHeight() - 24, 24, 24);   // esquina inferior derecha
}

void PhantomAsymEditor::paint (Graphics& g)
{
    g.fillAll (Colour (40, 40, 41));
    g.addTransform (AffineTransform::translation (-kCropX, (float) -kCrop).scaled (scale));
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImageAt (impl->bg, 0, 0);

    // medidor de salida: barritas finas horizontales (estilo segmentado), misma escala que el fader
    constexpr int kSeg = 77;
    constexpr float kPitch = kMeterPitch;
    // las barritas se alinean a los pixeles de la pantalla (retina, 2x) para que todas se vean iguales y parejas
    auto snap = [this] (float yDesign) { const float k = scale * 2.0f; return std::round ((yDesign - (float) kCrop) * k) / k + (float) kCrop; };
    const int lit = roundToInt (dbToFaderNorm (meterShown) * (float) kSeg);
    const bool blue = proc.apvts.getRawParameterValue ("listen")->load() >= 0.5f;   // SC Listen prendido: barritas azules
    const float yBase = kBottom - 2.5f;
    for (int i = 0; i < kSeg; ++i)
    {
        const float yTop = snap (yBase - (float) (i + 1) * kPitch + kPitch * 0.2f), yBot = snap (yBase - (float) i * kPitch - kPitch * 0.2f);
        const float y = yTop, kBar = yBot - yTop, kHi = 1.0f / (scale * 2.0f);
        if (i < lit)
        {
            g.setColour (blue ? Colour (62, 128, 245) : Colour (170, 170, 172));
            g.fillRect (1505.5f, y, 46.0f, kBar);
            g.setColour (blue ? Colour (150, 200, 255) : Colour (214, 214, 216));
            g.fillRect (1505.5f, y, 46.0f, kHi);
        }
        else
        {
            g.setColour (Colour (46, 46, 48));
            g.fillRect (1505.5f, y, 46.0f, kBar);
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

    // un clic sobre el vumetro o sobre su valor de abajo reinicia el pico que muestra ese valor
    const auto clickPos = e.getEventRelativeTo (this).position / scale + Point<float> (kCropX, (float) kCrop);
    if (ValueOverlay::meterZone().contains (clickPos) || ValueOverlay::peakTextZone().contains (clickPos))
    {
        proc.peakHoldDb.store (-100.0f);
        impl->values.peakDb = -100.0f; impl->values.peakValText = meterText(); impl->values.repaint (impl->values.valuesArea());
    }

    if (e.eventComponent != this) return;
    // clic en las palabras "Half" / "Whole" elige el modo
    const auto p = e.position / scale + Point<float> (kCropX, (float) kCrop);
    if (p.y >= 936.0f && p.y <= 991.0f)
    {
        if (p.x >= 707.0f && p.x <= 797.0f)       impl->mode.setWhole (false);
        else if (p.x >= 957.0f && p.x <= 1047.0f) impl->mode.setWhole (true);
    }
}
