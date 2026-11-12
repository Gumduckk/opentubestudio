#!/usr/bin/env bash
# GummVTuber — setup completo para quem baixou o repositório.
# Instala as dependências do sistema (com sudo), baixa o SDK/modelos e
# compila tudo sozinho. Um comando só:
#
#   git clone <repo> && cd <pasta> && ./portable.sh
#
# Suporta: Arch/pacman, Debian/Ubuntu/apt, Fedora/dnf, openSUSE/zypper.
set -e
cd "$(dirname "$0")"

echo "=== GummVTuber :: instalação 1-comando ==="

# ---- 1) Dependências do sistema -------------------------------------------
echo "-> Instalando dependências do sistema..."
if command -v pacman >/dev/null 2>&1; then
  sudo pacman -S --needed --noconfirm base-devel cmake git qt6-base \
    qt6-multimedia qt6-multimedia-ffmpeg mesa python python-pip
elif command -v apt-get >/dev/null 2>&1; then
  sudo apt-get update
  sudo apt-get install -y build-essential cmake git qt6-base-dev \
    qt6-multimedia-dev mesa-common-dev libgl1-mesa-dev \
    python3 python3-venv python3-pip
elif command -v dnf >/dev/null 2>&1; then
  sudo dnf install -y gcc-c++ cmake git qt6-qtbase-devel \
    qt6-qtmultimedia-devel mesa-libGL-devel python3 python3-pip python3-virtualenv
elif command -v zypper >/dev/null 2>&1; then
  sudo zypper in -y gcc-c++ cmake git qt6-base-devel qt6-multimedia-devel \
    Mesa-libGL-devel python3 python3-pip python3-virtualenv
else
  echo "DISTRO NÃO SUPORTADA automaticamente."
  echo "Instale manualmente: Qt6 (Base + Multimedia), CMake, C++17, OpenGL, Python 3 + venv + pip."
  exit 1
fi

# ---- 2) SDK Live2D + mediapipe + compilação --------------------------------
./setup.sh yes

echo ""
echo "=== Tudo pronto! ==="
echo "Abra com:  ./iniciar.sh       (ou 'GummVTuber' no menu do sistema)"
echo "Para OBS:  ./iniciar.sh --x11"
