# Interfaz definitiva (guardada el 30/10/2026)

Ventana de **542 × 321** (arte de 1711 × 1044 unidades, recortado por el marco a 1682,5 × 996; las líneas guía de la grilla están cada 124,5 unidades).

- `UI_definitiva.png`: captura del plugin tal como quedó.
- `generadores/`: scripts que dibujan las imágenes de `Resources/` (se corren desde esa carpeta):
  - `bg_gen.py`: fondo (títulos, escalas, marcas, Half / Whole, nombre y frase). Uso: `python3 bg_gen.py x ../../Resources/ui_bg.png` (el primer argumento no se usa).
  - `knobs_gen.py` (+ `k4_src.py`): cuerpos y punteros de Depth, Fade y Mode.
  - `fader_gen.py`: pista y perilla del fader. `button_gen.py`: botones Bypass y SC Listen.
  - `fonts2/`: `InterMedium.ttf`, `InterLight.ttf`, `Michroma-Regular.ttf`.
- El código de la interfaz está en `Source/PluginEditor.cpp`.

## Disposición
- Depth, Fade y Mode giran 300° (de -150° a +150°). Fade y Mode no tienen ticks; Mode tiene dos líneas (horizontal desde la prolongación del puntero y bajada a 90°) hasta Half y Whole, que quedan centrados en los bordes izquierdo y derecho de la perilla de Mode.
- Izquierda: Depth (R 249); a su derecha Fade arriba y Mode abajo (R 125); luego una línea divisoria y a la derecha Output (fader) y SC Input (vúmetro) con los botones Bypass y SC Listen (cuadrados, del ancho del vúmetro) debajo.
- Los textos (títulos, números, valores, Half / Whole) tienen el mismo tamaño (38, como el "Window" de la referencia). Título Depth, Fade, Output y SC Input sobre la línea azul de arriba (mitad entre el borde y la primera línea roja); Bypass, SC Listen, Half y Whole sobre la línea azul de abajo.
- El fader y el vúmetro terminan (-inf y fondo) en la línea azul de arriba del botón; los valores de abajo quedan sobre la línea roja que tienen encima. Los botones apoyan su borde inferior en la primera línea roja contando desde abajo.
- Los números de la escala de Depth quedan a 20,8 unidades de su puntito (el 0 toca la línea azul de arriba).
- Sin nombre del plugin dibujado en la interfaz.

## Tipografías
| Dónde | Letra | Grosor | Tamaño |
|---|---|---|---|
| Títulos (Depth, Fade, Mode, Output, SC Input), Bypass, SC Listen | Inter | Medium | 38 |
| Números de las escalas, valores de abajo, Half / Whole, caja de valor | Inter | Light | 38 |
