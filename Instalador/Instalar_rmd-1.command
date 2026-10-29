#!/bin/bash
# Instalador de rmd-1 para Mac. Ponelo en la misma carpeta que las carpetas "AU" y "VST3" que vienen en el zip
# (o suelto en cualquier carpeta que las contenga) y hace doble clic.
cd "$(dirname "$0")" || exit 1

VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3_DIR" "$AU_DIR"

instalados=0
while IFS= read -r -d '' p; do
  nombre=$(basename "$p")
  case "$nombre" in
    *.vst3) destino="$VST3_DIR" ;;
    *.component) destino="$AU_DIR" ;;
    *) continue ;;
  esac
  echo "Instalando $nombre en $destino ..."
  rm -rf "$destino/$nombre"
  cp -R "$p" "$destino/"
  xattr -cr "$destino/$nombre"                                   # saca el bloqueo de macOS (descargado de internet)
  codesign --force --deep --sign - "$destino/$nombre" 2>/dev/null   # firma local, para que macOS lo deje cargar
  instalados=$((instalados+1))
done < <(find . -maxdepth 3 \( -name "*.vst3" -o -name "*.component" \) -print0)

if [ "$instalados" -eq 0 ]; then
  echo "No encontre ningun .vst3 ni .component al lado de este archivo."
else
  # que Live / Logic vuelvan a leer los plugins
  killall -9 AudioComponentRegistrar 2>/dev/null
  echo ""
  echo "Listo. Abri Live y hace un Rescan de plugins (Preferencias > Plug-Ins)."
  echo "Arquitectura de esta Mac: $(uname -m)"
fi
echo ""
read -n 1 -s -r -p "Toca una tecla para cerrar..."
