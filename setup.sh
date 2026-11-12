#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# OpentubeStudio — instalação em 1 clique
#   • baixa o Cubism SDK oficial da Live2D (grátis; ao baixar você aceita a
#     licença gratuita da Live2D — https://www.live2d.com/eula/)
#   • baixa o modelo de rastreio facial (MediaPipe, do Google, grátis)
#   • instala o MediaPipe (webcam) via pip --user (opcional)
#   • compila o app (Qt6 + CMake)
# ---------------------------------------------------------------------------
set -e
cd "$(dirname "$0")"

SDK_DIR="third_party/CubismSdkForNative-5-r.5"
SDK_URL="https://cubism.live2d.com/sdk-native/bin/CubismSdkForNative-5-r.5.zip"
FACE_URL="https://storage.googleapis.com/mediapipe-models/face_landmarker/face_landmarker/float16/1/face_landmarker.task"
OSF_DIR="tools/OpenSeeFace"
OSF_URL="https://github.com/emilianavt/OpenSeeFace.git"

# ---------------------------------------------------------------------------
echo "=== OpentubeStudio :: instalação ==="

# 0) OpenSeeFace (mesmo rastreador que o VTube Studio usa) -------------------
if [ ! -f "$OSF_DIR/tracker.py" ]; then
  echo "-> Baixando o OpenSeeFace (rastreador facial open source, igual ao VTS)..."
  if command -v git >/dev/null 2>&1; then
    git clone --depth 1 "$OSF_URL" "$OSF_DIR" \
      && echo "   OpenSeeFace pronto em $OSF_DIR" \
      || echo "   AVISO: falha ao clonar o OpenSeeFace (sem internet?)."
  else
    echo "   AVISO: git não instalado. No Arch: sudo pacman -S git"
  fi
else
  echo "-> OpenSeeFace já baixado ✓"
fi

# 1) Cubism SDK -------------------------------------------------------------
if [ ! -f "$SDK_DIR/Core/lib/linux/x86_64/libLive2DCubismCore.a" ]; then
  echo "-> Baixando o Cubism SDK oficial da Live2D (~27 MB, uma vez só)..."
  curl -L --fail -o /tmp/CubismSdkForNative.zip "$SDK_URL"
  mkdir -p third_party
  unzip -qo /tmp/CubismSdkForNative.zip -d third_party
  rm -f /tmp/CubismSdkForNative.zip
  echo "   SDK pronto em $SDK_DIR"
else
  echo "-> Cubism SDK já baixado ✓"
fi

# 2) Modelo de rastreio facial ----------------------------------------------
mkdir -p tools/models
if [ ! -f tools/models/face_landmarker.task ]; then
  echo "-> Baixando o modelo de rastreio facial MediaPipe (~3.7 MB)..."
  curl -L --fail -o tools/models/face_landmarker.task "$FACE_URL" || \
    echo "   AVISO: não baixou agora; a webcam não vai funcionar até baixar."
else
  echo "-> Modelo de rastreio facial já baixado ✓"
fi

CUBEM_SILENT_YESNO="${1:-}"  # use: ./setup.sh yes -> aceita tudo sem perguntar

# 3) Ambiente Python (OpenSeeFace + MediaPipe) em um venv dedicado -----------
# O sistema pode bloquear pip no Python global (PEP 668, ex.: Arch/Fedora).
# Criamos um venv dentro de tools/venv que o app usa automaticamente.
if [ "$CUBEM_SILENT_YESNO" = "yes" ]; then
  resp="s"
else
  read -r -p "-> Instalar o rastreio facial (OpenSeeFace + MediaPipe)? [S/n] " resp
fi
if [ -z "$resp" ] || [ "$resp" = "s" ] || [ "$resp" = "S" ]; then
  VENV="tools/venv"
  if [ ! -x "$VENV/bin/python" ]; then
    echo "-> Criando ambiente Python isolado em $VENV ..."
    python3 -m venv "$VENV" || {
      echo "   AVISO: falha ao criar venv. No Arch instale: sudo pacman -S python"
      echo "   O app segue funcionando com mouse + microfone."
    }
  fi
  if [ -x "$VENV/bin/python" ]; then
    # O venv do Arch às vezes nasce sem pip — garante via ensurepip.
    if ! "$VENV/bin/python" -m pip --version >/dev/null 2>&1; then
      echo "-> Habilitando pip no venv..."
      "$VENV/bin/python" -m ensurepip --upgrade >/dev/null 2>&1 || {
        echo "   AVISO: sem pip. No Arch: sudo pacman -S python-pip"
      }
    fi
    if "$VENV/bin/python" -m pip --version >/dev/null 2>&1; then
      echo "-> Instalando dependências do rastreio (mediapipe, opencv...) ..."
      "$VENV/bin/python" -m pip install --upgrade pip >/dev/null 2>&1 || true
      "$VENV/bin/python" -m pip install mediapipe onnxruntime opencv-python pillow numpy \
        || echo "   AVISO: não instalou. Webcam fica desativada."
    fi
  fi
else
  echo "   (pulando — webcam fica desativada, dá pra instalar depois com ./setup.sh)"
fi

# 4) Compilar ----------------------------------------------------------------
echo "-> Compilando (isso pode demorar um pouco na primeira vez)..."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"

# 5) Permissão de leitura do mouse (tracking global no Wayland) --------------
# O EvdevMouse lê /dev/input/event* para seguir o cursor mesmo fora da janela.
if id -nG "$USER" | grep -qw input; then
  echo "-> Usuário já está no grupo 'input' ✓"
else
  echo "-> Concedendo acesso a /dev/input (para rastrear o mouse fora da janela)..."
  if command -v usermod >/dev/null 2>&1; then
    sudo usermod -aG input "$USER" \
      && echo "   OK! Faça logout/login (ou reinicie) para valer." \
      || echo "   AVISO: não deu para adicionar ao grupo. Rode manualmente:"
  fi
  echo "   sudo usermod -aG input \$USER   (e faça login de novo)"
fi

# 6) Atalho no menu do sistema ----------------------------------------------
mkdir -p "$HOME/.local/share/applications"
cp -f "icon.png" build/opentubestudio-icon.png 2>/dev/null || true
cat > "$HOME/.local/share/applications/opentubestudio.desktop" << EOF
[Desktop Entry]
Type=Application
Name=OpentubeStudio
Comment=Player Live2D para Linux (estilo VTube Studio)
Exec=$(pwd)/build/OpentubeStudio
Icon=$(pwd)/build/opentubestudio-icon.png
Terminal=false
Categories=AudioVideo;Graphics;
EOF
update-desktop-database "$HOME/.local/share/applications" 2>/dev/null || true

echo ""
echo "=== Tudo pronto! ==="
echo "Abra com:  ./iniciar.sh     (ou 'OpentubeStudio' no menu do sistema)"
