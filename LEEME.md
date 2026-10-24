# SCD-1 v1.0.3 (sidechain only)

Silencia el audio que entra, en full range, siguiendo los cycles de la señal que le mandes por el **sidechain input**. Interfaz propia (wireframe blanco con perillas negras de luz fija, fader vertical y vúmetro de salida), imágenes y fuente Inter incluidas en `Resources/` (licencia OFL).

## Controles (en este orden)

| Control | Qué hace |
|---|---|
| **Depth** | Perilla: **centro = 0 dB**, todo a la derecha = **+6 dB** (lineal, 1 dB por marca), todo a la izquierda = **-inf** (silencio, sin efecto). La mitad izquierda es logarítmica: marcas en 0, -3, -6, -12, -24, -48 y -inf. A 0 dB, un key a 0 dBFS silencia el audio por completo; con valores positivos, el silence total se alcanza con keys más bajos |
| **Bypass** | Bypass general del plugin (con un fade de 5 ms, sin clicks): el audio pasa intacto, sin Depth y sin Output |
| **Input Listen** | Solo se escucha el key (el sidechain input), en lugar del audio. Sirve para comprobar que el sidechain llega y a qué level. Sin nada conectado, queda en silencio. El Output no se aplica en este modo |
| **Mode** | **Half** (por defecto): silencia un solo semiciclo de cada cycle. **Whole**: silencia un cycle completo, deja pasar el siguiente, y así alternando. Se cambia con **clic y arrastre** sobre la perilla (derecha o abajo = Whole, izquierda o arriba = Half; si empezás a arrastrar desde la mitad de arriba de la perilla, el movimiento horizontal va al revés; un clic solo no cambia nada), o con un clic sobre las palabras Half / Whole |
| **Polarity** | De 0° (Positive) a 180° (Negative). Mueve dónde del waveform cae el silence |
| **Output** | Volumen final, de **+6 dB a -inf**, fader logarítmico estilo consola: marcas equiespaciadas en +6, 0, -6, -12, -24, -48 y -inf (arriba mucha resolución, abajo se comprime). Por defecto 0 dB. Un clic en el fader no lo mueve: solo se mueve arrastrando o con doble clic y escribiendo el valor |

**Perillas y fader:** se mueven arrastrando (Shift = fino) o con la rueda; **doble clic** vuelve al valor por defecto. Bypass e Input Listen se cambian con un clic.

## Cómo se calcula el efecto

La reducción **sigue el level del sidechain**: la cantidad de efecto es proporcional a la amplitud del key (en cada cycle se toma su peak) y se multiplica por **Depth**. Un key a -6 dBFS da la mitad del efecto de uno a 0 dBFS, y a -12 dBFS un cuarto. No hay threshold que lo corte de golpe.

## Cómo usarlo

1. Poné SCD-1 en la pista (o el master) que querés afectar.
2. Mandá al **sidechain input** una señal con solo el **sub** (un kick o un 808 con low-pass), a un level razonable. El plugin no tiene filtros ni ganancia de entrada.
3. Activá **Input Listen** para comprobar qué le llega. Si no escuchás nada, el problema está en el ruteo del DAW. Si lo escuchás muy bajo, el efecto va a ser muy leve: subí el level de la señal que mandás, o usá Depth positivo (hasta +6 dB).
4. Sin señal en el sidechain, el audio pasa sin cambios.

Si mandás un bus completo (con medios y agudos), el plugin detecta cycles en todas las frecuencias y el patrón se vuelve irregular. Para que el ritmo sea limpio, el key tiene que contener solo el sub.

### Logic Pro

- El sidechain se elige en el menú **Side Chain** del header de la ventana del plugin.
- Si no aparece: cerrá Logic, reemplazá `SCD-1.component`, y en Logic Pro > Settings > Plug-in Manager seleccioná SCD-1 y tocá **Reset & Rescan Selection**.

## Notas

- El vúmetro de la derecha muestra el nivel de salida con la misma escala logarítmica del fader (+6, 0, -6, -12, -24, -48, -inf; el segmento de abajo se enciende con cualquier señal; la marca de pico se mantiene un momento). Con **Key Listen activo** los segmentos se vuelven cyan, como el botón (en ese modo muestra el nivel del key).
- Depth y Output cambiaron de curva: los proyectos guardados mantienen su valor en dB (el parámetro guarda dB), pero la perilla y el fader se ven en otra posición.
- Solo atenúa, nunca amplifica. No agrega latency ni filtros en el camino del audio.
- El sidechain se detecta en mono (promedio de L y R), y la misma bajada se aplica a todos los canales.
- Cuando el sidechain se apaga (entre golpes), el contador se reinicia y cada golpe arranca igual (el primer cycle se silencia).

## Compilar e instalar

1. Reemplazá los archivos de tu carpeta del proyecto por los de este zip (incluida la carpeta oculta `.github`).
2. En GitHub Desktop: escribí un Summary, **Commit to main**, **Push origin**.
3. Esperá la compilación en **Actions**, descargá el artifact de Windows o Mac.
4. Mac: `.vst3` a `~/Library/Audio/Plug-Ins/VST3/` y `.component` a `~/Library/Audio/Plug-Ins/Components/`, después `xattr -cr` sobre cada uno.
   Windows: `.vst3` a `C:\Program Files\Common Files\VST3\`

## Cambio de nombre

El plugin ahora se llama **SCD-1** (antes Phantom Asym). Aparece con el nombre nuevo en Logic. Los archivos instalados se llaman `SCD-1.component` y `SCD-1.vst3`: **borrá los viejos `Phantom Asym.component` / `Phantom Asym.vst3`** de sus carpetas para que no queden duplicados. El identificador interno no cambió, así que los proyectos guardados siguen cargando el plugin con sus valores.


## Interfaz
La interfaz propia se quito a proposito: el plugin usa la interfaz generica de JUCE (perillas/deslizadores estandar) hasta rehacerla.
El diseño anterior (con sus tipografias) esta guardado en `UI_anterior_guardada/` y no se compila.
