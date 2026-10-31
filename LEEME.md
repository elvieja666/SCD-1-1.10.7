# rmd-1 v1.0.21 (sidechain only)

Silencia el audio que entra, en full range, siguiendo los cycles de la señal que le mandes por el **sidechain input**. Interfaz propia (panel oscuro con perillas negras de luz fija, fader vertical, vúmetro de salida y botones con luz azul), imágenes y fuente Inter incluidas en `Resources/` (licencia OFL). La ventana mide 523 × 321 píxeles (tamaño fijo) (en pantallas retina se ve con el doble de nitidez).

## Controles (en este orden)

| Control | Qué hace |
|---|---|
| **Depth** | Perilla: **centro = 0 dB**, todo a la derecha = **+6 dB** (lineal, 1 dB por marca), todo a la izquierda = **-inf** (silencio, sin efecto). La mitad izquierda es logarítmica: marcas en 0, -3, -6, -12, -24, -48 y -inf. A 0 dB, un key a 0 dBFS silencia el audio por completo; con valores positivos, el silence total se alcanza con keys más bajos |
| **Bypass** | Bypass general del plugin (con un fade de 5 ms, sin clicks): el audio pasa intacto, sin Depth y sin Output |
| **SC Listen** | Solo se escucha el key (el sidechain input), en lugar del audio. Sirve para comprobar que el sidechain llega y a qué level. Sin nada conectado, queda en silencio. El Output no se aplica en este modo |
| **Mode** | **Half** (por defecto): silencia un solo semiciclo de cada cycle. **Whole**: silencia un cycle completo, deja pasar el siguiente, y así alternando. Se cambia con **clic y arrastre** sobre la perilla (derecha o abajo = Whole, izquierda o arriba = Half; si empezás a arrastrar desde la mitad de arriba de la perilla, el movimiento horizontal va al revés; un clic solo no cambia nada), o con un clic sobre las palabras Half / Whole. En Whole, un golpe nuevo (el pico del sidechain sube de golpe, aunque no haya habido silencio) arranca siempre con el cycle silenciado |
| *(Polarity fijo)* | La ventana de silencio queda siempre a 90° del cruce, así pasa señal un poco en cada polo (resultado simétrico). Ya no hay perilla |
| **Fade** | De 0% (por defecto) a 100%, de a 1 % al arrastrar o usar la rueda (un clic en uno de los 13 ticks del anillo salta a ese tick: 0, 8,3, 16,7, 25 ... 50 ... 100 %). Con 0 % el corte no es instantáneo: tiene una rampa mínima de 0,5 ms para evitar el ruido digital de los bordes. Suaviza la entrada y la salida de la ventana de silencio con una curva suave centrada en cada borde: empieza un poco antes y termina un poco después. Se mide como porcentaje del semiciclo, así acompaña el tono de la nota |
| **Output** | Volumen final, de **+6 dB a -inf**, fader logarítmico estilo consola: marcas equiespaciadas en +6, 0, -6, -12, -24, -48 y -inf (arriba mucha resolución, abajo se comprime). Por defecto 0 dB. Se mueve **arrastrando la perilla** del fader (un clic en la pista no hace nada); doble clic permite escribir el valor en dB |

**Perillas y fader:** se mueven arrastrando o con la rueda / gesto de dos dedos. **Depth salta de tick en tick**, incluidos los ticks chicos del medio (25 posiciones entre -inf y +6); **Fade** va de a 1 %. **Option + clic** vuelve al valor por defecto (Depth 0 dB, Output 0 dB, Mode Half). **Doble clic** en el fader Output permite escribir el valor en dB con el teclado (Enter acepta, Esc cancela, un clic en otra parte del plugin cierra la cajita; acepta `-inf`; un texto que no es un valor se ignora). Bypass e SC Listen se cambian con un clic sobre el botón.

## Cómo se calcula el efecto

La reducción **sigue el level del sidechain**: la cantidad de efecto es proporcional a la amplitud del key (en cada cycle se toma su peak) y se multiplica por **Depth**. Un key a -6 dBFS da la mitad del efecto de uno a 0 dBFS, y a -12 dBFS un cuarto. No hay threshold que lo corte de golpe.

## Cómo usarlo

1. Poné rmd-1 en la pista (o el master) que querés afectar.
2. Mandá al **sidechain input** una señal con solo el **sub** (un kick o un 808 con low-pass), a un level razonable. El plugin no tiene filtros ni ganancia de entrada.
3. Activá **SC Listen** para comprobar qué le llega. Si no escuchás nada, el problema está en el ruteo del DAW. Si lo escuchás muy bajo, el efecto va a ser muy leve: subí el level de la señal que mandás, o usá Depth positivo (hasta +6 dB).
4. Sin señal en el sidechain, el audio pasa sin cambios. Si el host (Logic con "None") manda en el sidechain la propia señal de la pista, idéntica a la entrada, el plugin lo ignora y deja pasar el audio.

Si mandás un bus completo (con medios y agudos), el plugin detecta cycles en todas las frecuencias y el patrón se vuelve irregular. Para que el ritmo sea limpio, el key tiene que contener solo el sub.

### Logic Pro

- El sidechain se elige en el menú **Side Chain** del header de la ventana del plugin.
- Si no aparece: cerrá Logic, reemplazá `rmd-1.component`, y en Logic Pro > Settings > Plug-in Manager seleccioná rmd-1 y tocá **Reset & Rescan Selection**.

## Notas

- El vúmetro de la derecha es mono (una sola columna) y muestra solo la señal que entra por el **sidechain** (el nivel del canal más fuerte), con la misma escala logarítmica del fader (+6, 0, -6, -12, -24, -48, -inf). No depende de Output ni de Bypass. Con **SC Listen** activo las barritas se vuelven azules.
- Depth y Output cambiaron de curva: los proyectos guardados mantienen su valor en dB (el parámetro guarda dB), pero la perilla y el fader se ven en otra posición.
- Solo atenúa, nunca amplifica. No agrega filtros en el camino del audio; sí 12 ms de latencia (ver abajo).
- Con un sidechain estéreo, el detector sigue el canal con más energía (no la suma de L y R, que se anula con canales desfasados); la misma bajada se aplica a todos los canales.
- En modo Half, si una ventana de silencio (o de audio) dura menos de un tercio de la anterior (un rebote chico del 808 al cambiar de nota), se funde con la vecina para que no suene como un glitch.
- Cuando el sidechain se apaga (entre golpes), el contador se reinicia y cada golpe arranca igual (el primer cycle se silencia).

## Compilar e instalar

1. En GitHub Desktop: escribí un Summary, **Commit to main**, **Push origin**. Cada push dispara la compilación; si querés guardar un cambio sin compilar, poné `[skip ci]` en el Summary.
2. Esperá la compilación en **Actions** ("Compilar plugin") y descargá el artifact **rmd-1-mac**.
3. `.vst3` a `~/Library/Audio/Plug-Ins/VST3/` y `.component` a `~/Library/Audio/Plug-Ins/Components/`, después `xattr -cr` sobre cada uno.

## Cambio de nombre

El plugin se llama **rmd-1** (antes SCD-1, y antes Phantom Asym). Aparece con el nombre nuevo en Logic y los archivos instalados se llaman `rmd-1.component` y `rmd-1.vst3`: **borrá los viejos `SCD-1.component` / `SCD-1.vst3` (y `Phantom Asym.*`)** de sus carpetas para que no queden duplicados. El identificador interno no cambió, así que los proyectos guardados siguen cargando el plugin con sus valores.

## Interfaz y carpetas

- `Source/`: el plugin (`PluginProcessor` = audio y parámetros, `PluginEditor` = interfaz).
- `Resources/`: imágenes de la interfaz (fondo, perillas, fader, botones) y la fuente Inter. Se incrustan en el plugin al compilar.
- `UI_definitiva/`: captura del diseño y los scripts de Python (`generadores/`) que dibujan las imágenes de `Resources/`. No se compilan.
- `Tests/`: pruebas de audio (ruteo, parámetros, SC Listen, robustez, fuzz) con detección de errores de memoria. No se compilan con el plugin; las instrucciones están en `Tests/CMakeLists.txt`.

**Valores y pico del vúmetro:** debajo del fader se ve el valor de Output en dB y debajo del vúmetro el pico más alto del sidechain (el pico sube hasta quedarse, hold; sin señal marca "-inf dB"). Un clic sobre el vúmetro reinicia el pico.

**Precisión y latencia:** el plugin retrasa el audio 12 ms y se lo informa al host (el DAW lo compensa solo; no está pensado para tocar en vivo). Con ese margen ve los cruces por cero antes de decidir: la ventana de silencio va exactamente del punto medio del semiciclo positivo (90°) al punto medio del negativo (270°), también en un 808 que baja de tono rápido. Con el 808 de prueba el error es menor a 1°.
