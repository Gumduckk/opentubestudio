#!/usr/bin/env bash
# GummVTuber — inicia o app.
#   ./iniciar.sh          -> modo normal
#   ./iniciar.sh --x11    -> força X11 (XWayland): melhor p/ capturar no OBS
cd "$(dirname "$0")"
if [ "$1" = "--x11" ]; then
  export QT_QPA_PLATFORM=xcb
fi

# O app escolhe sozinho o Python: venv do projeto, ou ~/OpenSeeFace/env.
# (Defina GUMM_PYTHON para forçar um específico.)

exec ./build/GummVTuber "$@"
