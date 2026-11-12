#!/usr/bin/env bash
# GummVTuber / OpentubeStudio — monta o AppImage.
#
#   ./tools/appimage.sh           # gera ./dist/OpentubeStudio-x86_64.AppImage
#   ./tools/appimage.sh --skip-deps
#
# Requisitos: curl, patchelf, FUSE (fusermount), rede para baixar o
# linuxdeploy e o plugin Qt na primeira execução.
set -euo pipefail
cd "$(dirname "$0")/.."

APP=OpentubeStudio
DIST=dist
TOOLS=build/appimage-tools
APPDIR="$DIST/$APP.AppDir"
DESKTOP=resources/opentubestudio.desktop
APPRUN=resources/AppRun
APPICON=opentubestudio.png

echo "=== OpentubeStudio :: build do AppImage ==="

# ---- 1) Requisitos ---------------------------------------------------------
for bin in curl patchelf fusermount; do
  if ! command -v "$bin" >/dev/null 2>&1; then
    echo "ERRO: '$bin' não encontrado. Instale: sudo apt/pacman install $bin (ou libfuse2 para fuse)"
    exit 1
  fi
done

# ---- 2) Build limpo do app -------------------------------------------------
echo "-> Compilando..."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"

# ---- 3) Ferramentas (linuxdeploy + plugin Qt) ------------------------------
mkdir -p "$TOOLS"
LINUXDEPLOY="$TOOLS/linuxdeploy-x86_64.AppImage"
QT_PLUGIN="$TOOLS/linuxdeploy-plugin-qt-x86_64.AppImage"
if [ ! -x "$LINUXDEPLOY" ]; then
  echo "-> Baixando linuxdeploy..."
  curl -L --fail -o "$LINUXDEPLOY" \
    https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
  chmod +x "$LINUXDEPLOY"
fi
if [ ! -x "$QT_PLUGIN" ]; then
  echo "-> Baixando linuxdeploy-plugin-qt..."
  curl -L --fail -o "$QT_PLUGIN" \
    https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
  chmod +x "$QT_PLUGIN"
fi

# ---- 4) Monta o AppDir ------------------------------------------------------
echo "-> Montando AppDir em $APPDIR"
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin" \
         "$APPDIR/usr/share/applications" \
         "$APPDIR/usr/share/icons/hicolor/256x256/apps"

# Ícone raiz para o QIcon do próprio app (lê icon.png do lado do executável)
cp icon.png "$APPDIR/usr/bin/icon.png"
# Ícone no diretório esperado pelo desktop/linuxdeploy
cp icon.png "$APPDIR/usr/share/icons/hicolor/256x256/apps/opentubestudio.png"
cp "$DESKTOP" "$APPDIR/usr/share/applications/opentubestudio.desktop"
cp "$APPRUN" "$APPDIR/AppRun"; chmod +x "$APPDIR/AppRun"

# Binário + shaders do Cubism Framework
cp build/OpentubeStudio "$APPDIR/usr/bin/OpentubeStudio"
cp -r build/FrameworkShaders "$APPDIR/usr/bin/FrameworkShaders"

# Ferramentas do rastreador facial — SÓ o necessário (sem venv!)
mkdir -p "$APPDIR/usr/bin/tools/models"
cp tools/tracker_daemon.py tools/facemesh_connections.py "$APPDIR/usr/bin/tools/"
if [ -f tools/models/face_landmarker.task ]; then
  cp tools/models/face_landmarker.task "$APPDIR/usr/bin/tools/models/"
fi

# ---- 4.5) Staging dos plugins do Qt (exclui kimg_jxr, cuja lib está ausente) -
echo "-> Montando staging dos plugins do Qt..."
STAGE_DIR=/tmp/opentubestudio-qt-stage
rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR/plugins"
(cd /usr/lib/qt6/plugins && find . -type d -exec mkdir -p "$STAGE_DIR/plugins"/{} \;)
(cd /usr/lib/qt6/plugins && find . -type f | while read -r f; do
  case "$f" in
    ./imageformats/kimg_jxr.so) echo "   pulando $f (libjpegxr ausente)"; continue ;;
    ./multimedia/libffmpegmediaplugin.so) echo "   pulando $f (segfault no AppImage)"; continue ;;
  esac
  ln -sf "/usr/lib/qt6/plugins/$f" "$STAGE_DIR/plugins/$f"
done)
QT_VERSION="$(qmake6 -query QT_VERSION 2>/dev/null || echo 6)"

cat > "$TOOLS/qtdeploy-qmake" <<'QMAKE_EOF'
#!/usr/bin/env bash
# qmake de deploy: responde paths com o staging (sem espaços, senão o
# parser do plugin (split por espaço) descarta o caminho inteiro)
if [ "${1:-}" = "-query" ]; then
  if [ -z "${2:-}" ]; then
    cat <<EOF
QT_INSTALL_PLUGINS:/tmp/opentubestudio-qt-stage/plugins
QT_INSTALL_LIBEXECS:/usr/lib/qt6/bin
QT_INSTALL_DATA:/usr/share/qt6
QT_INSTALL_TRANSLATIONS:/usr/share/qt6/translations
QT_INSTALL_BINS:/usr/bin
QT_INSTALL_LIBS:/usr/lib
QT_INSTALL_QML:/usr/lib/qt6/qml
QT_VERSION:%QT_VERSION%
EOF
    exit 0
  fi
  case "${2:-}" in
    QT_INSTALL_PLUGINS)      echo /tmp/opentubestudio-qt-stage/plugins; exit 0 ;;
    QT_INSTALL_LIBS)         echo /usr/lib;            exit 0 ;;
    QT_INSTALL_TRANSLATIONS) echo /usr/share/qt6/translations; exit 0 ;;
    QT_INSTALL_QML)          echo /usr/lib/qt6/qml;    exit 0 ;;
    QT_INSTALL_BINS)         echo /usr/bin;            exit 0 ;;
    QT_INSTALL_LIBEXECS)     echo /usr/lib/qt6/bin;    exit 0 ;;
    QT_INSTALL_DATA)         echo /usr/share/qt6;      exit 0 ;;
    QT_VERSION)              echo %QT_VERSION%;        exit 0 ;;
  esac
fi
exec qmake6 "$@"
QMAKE_EOF
sed -i "s|%QT_VERSION%|$QT_VERSION|g" "$TOOLS/qtdeploy-qmake"
chmod +x "$TOOLS/qtdeploy-qmake"

# ---- 4.5) Python portátil + mediapipe (mínimo) ------------------------------
echo "-> Baixando Python portátil (cpython-3.12.16, ~66 MB)..."
PBS="$TOOLS/cpython-3.12.16.tar.gz"
if [ ! -f "$PBS" ]; then
  curl -fL --retry 3 --fail -o "$PBS" \
    "https://github.com/astral-sh/python-build-standalone/releases/download/20261110/cpython-3.12.16+20261110-x86_64-unknown-linux-gnu-install_only.tar.gz"
fi
PYDIR="$APPDIR/opt/python"
rm -rf "$PYDIR"
mkdir -p "$PYDIR"
tar -xzf "$PBS" -C "$PYDIR" --strip-components=1

echo "-> Instalando mediapipe (mínimo) no Python portátil..."
"$PYDIR/bin/python3" -m pip install --no-cache-dir \
  mediapipe --no-deps \
  numpy opencv-python-headless absl-py flatbuffers certifi \
  matplotlib packaging pillow contourpy cycler fonttools kiwisolver pillow pyparsing python-dateutil packaging

echo "-> Verificando import do mediapipe no Python portátil..."
"$PYDIR/bin/python3" -c "import mediapipe as mp, cv2, numpy; from mediapipe.tasks.python import vision; from mediapipe.tasks.python.core.base_options import BaseOptions; print('   mediapipe', mp.__version__, '/ cv2', cv2.__version__)"

# ---- 5) Deixa o linuxdeploy resolver Qt + libs ------------------------------
export NO_STRIP=1   # o strip embutido do linuxdeploy não reconhece .relr.dyn (binutils novo)
echo "-> Resolvendo dependências base (sem plugin)..."
"$LINUXDEPLOY" --appdir="$APPDIR" \
  --executable="$APPDIR/usr/bin/OpentubeStudio" \
  --desktop-file="$APPDIR/usr/share/applications/opentubestudio.desktop" \
  --icon-file="$APPDIR/usr/share/icons/hicolor/256x256/apps/opentubestudio.png"

export QMAKE="$PWD/$TOOLS/qtdeploy-qmake"
echo "-> Executando o plugin Qt em modo standalone..."
"$QT_PLUGIN" --appdir="$APPDIR"

echo "-> Gerando o AppImage..."
"$LINUXDEPLOY" --appdir="$APPDIR" --output appimage

# O linuxdeploy gera o .AppImage no diretório atual
mv -f OpentubeStudio-x86_64.AppImage "$DIST/" 2>/dev/null || true

echo ""
echo "=== Pronto! ==="
echo "Artefato:"
ls -lh dist/*.AppImage 2>/dev/null || ls -lh ./*.AppImage
echo "(dist/ está no .gitignore — não versionar o AppImage)"
