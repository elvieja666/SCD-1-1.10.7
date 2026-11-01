#!/bin/bash
# Desinstalador de rmd-1 para Mac: hace doble clic. Borra el plugin (y las versiones viejas con otros nombres) de las carpetas de plugins de tu usuario.

VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"

borrados=0
for p in "$VST3_DIR/rmd-1.vst3" "$AU_DIR/rmd-1.component" \
         "$VST3_DIR/SCD-1.vst3" "$AU_DIR/SCD-1.component" \
         "$VST3_DIR/Phantom Asym.vst3" "$AU_DIR/Phantom Asym.component"; do
  if [ -e "$p" ]; then
    echo "Borrando $p ..."
    rm -rf "$p"
    borrados=$((borrados+1))
  fi
done

if [ "$borrados" -eq 0 ]; then
  echo "No encontre rmd-1 instalado en las carpetas de tu usuario."
else
  # que Logic / Live vuelvan a leer los plugins
  killall -9 AudioComponentRegistrar 2>/dev/null
  echo ""
  echo "Listo, rmd-1 desinstalado ($borrados archivos). Si Logic todavia lo muestra:"
  echo "Logic Pro > Settings > Plug-in Manager > Reset & Rescan All."
fi
echo ""
read -n 1 -s -r -p "Toca una tecla para cerrar..."
