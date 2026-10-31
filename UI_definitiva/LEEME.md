# Interfaz definitiva (guardada el 30/10/2026)

Ventana de **523 × 321** (arte de 1671 × 1044 unidades, recortado por el marco a 1623 × 996).

- `UI_definitiva.png`: captura del plugin tal como quedó.
- `generadores/`: scripts que dibujan las imágenes de `Resources/` (se corren desde esa carpeta):
  - `bg_gen.py`: fondo (títulos, escalas, marcas, Half / Whole, nombre y frase). Uso: `python3 bg_gen.py x ../../Resources/ui_bg.png` (el primer argumento no se usa).
  - `knobs_gen.py` (+ `k4_src.py`): cuerpos y punteros de Depth, Fade y Mode.
  - `fader_gen.py`: pista y perilla del fader. `button_gen.py`: botones Bypass y SC Listen.
  - `fonts2/`: `InterMedium.ttf`, `InterLight.ttf`, `Michroma-Regular.ttf`.
- El código de la interfaz está en `Source/PluginEditor.cpp`.

## Disposición
- Izquierda: Depth (R 275) grande; a su derecha, Fade arriba y Mode abajo (R 114). Fade alineada arriba con el +6 del fader; Half / Whole en la misma línea que Bypass y SC Listen. Depth queda en el medio entre el tope de Fade y el fondo de Mode. El título Mode está a la misma distancia de su perilla que Fade de la suya (tinta a 41,5 unidades del borde superior).
- Abajo a la izquierda: nombre del plugin y "Fabricado en Buenos Aires Argentina"; el techo del nombre queda en el borde de abajo de los botones.
- Derecha: Output (fader) y SC Input (vúmetro), con los botones Bypass y SC Listen debajo.
- Separaciones laterales de 117 unidades (borde → Depth, Depth → Fade/Mode, Fade/Mode → línea divisoria, línea → Bypass → SC Listen → borde).
- Distancia del borde de arriba al +6 del fader y al tope del vúmetro = distancia del fondo de los botones al borde de abajo (105).
- Los números de la escala de Depth quedan a 13 unidades exactas de su puntito.

## Tipografías
| Dónde | Letra | Grosor | Tamaño |
|---|---|---|---|
| Títulos (Depth, Fade, Mode, Output, SC Input), Bypass, SC Listen | Inter | Medium | 30 |
| Números de las escalas, valores de abajo, Half / Whole, caja de valor del fader | Inter | Light | 30 |
| Fabricado en Buenos Aires Argentina | Inter | Light | 25 |
| Nombre del plugin (rmd-1) | Michroma | Regular | 37.5 (calculado) |
