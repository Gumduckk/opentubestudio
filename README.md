# OpentubeStudio

Aplicação de Live2D para Linux que anima o seu avatar com movimentos reais —
estilo VTube Studio, mas 100% offline e gratuito. Funciona com mouse,
microfone (lip sync) e **webcam com rastreio facial** (MediaPipe).

## Instalar (do zero ao app rodando em um comando)

```bash
git clone <seu-repo> opentubestudio
cd opentubestudio
./portable.sh      # instala Qt6 + compilador, baixa o SDK da Live2D,
                   # instala o mediapipe num venv, compila e cria o atalho
```

Ao final:

```bash
./iniciar.sh        # normal
./iniciar.sh --x11  # forçando X11/XWayland (melhor para OBS capturar)
```

Ou abra "OpentubeStudio" pelo menu de aplicativos.

### O que o `./portable.sh` faz

1. Instala as dependências do sistema (Qt6, C++, CMake, OpenGL, Python + pip + venv) — detecta pacman/apt/dnf/zypper.
2. Baixa o **Cubism SDK para Native** oficial da Live2D (versão grátis; precisa aceitar os termos da Live2D para usar, o que acontece ao baixar).
3. Baixa o modelo de rastreio `face_landmarker.task` (Google, grátis).
4. Cria um ambiente Python isolado em `tools/venv` com mediapipe + opencv e compila o app.

### Requisitos por distro

| Distro | Pacotes necessários (instalados pelo script) |
|---|---|
| Arch | `base-devel cmake git qt6-base qt6-multimedia mesa python` |
| Debian/Ubuntu | `build-essential cmake git qt6-base-dev qt6-multimedia-dev mesa-common-dev libgl1-mesa-dev python3 python3-venv python3-pip` |
| Fedora | `gcc-c++ cmake git qt6-qtbase-devel qt6-qtmultimedia-devel python3 python3-pip mesa-libGL-devel` |
| openSUSE | `gcc-c++ cmake git qt6-base-devel qt6-multimedia-devel Mesa-libGL-devel python3 python3-pip python3-virtualenv` |

## Uso (primeira execução)

1. Abra o app. Um assistente pede para escolher o **modelo Live2D**
   (qualquer pasta com um `*.model3.json`). Os modelos Live2D **não** vêm no
   repositório — use um seu ou um gratuito que você tenha direito.
2. Escolha webcam/microfone/mouse; calibre olhando para a câmera.
3. Para o OBS: ative "Fundo transparente" e rode `./iniciar.sh --x11` se
   necessário.

## Rastreio facial (MediaPipe)

O app usa o **Face Landmarker** do MediaPipe (BlazeFace + mesh refinado,
roda em CPU). Os sinais disponíveis:

- **Cabeça/ângulos** (yaw/pitch/roll com correção de eixo para GL)
- **Piscada** (EAR — Eye Aspect Ratio — fundido ao blendshape)
- **Boca** (`jawOpen` + MAR por geometria + eixo X/Y da boca)
- **Sobrancelha** (altura por geometria dos landmarks + franzir + 4 pontos/lado)
- **Olhos** (íris, olho feliz `eyeSmile`, EAR)
- **Posição 3D do rosto** (lean in/out, metros normalizados pelo tamanho da face)

Debug do rastreio: aba **Rastreio** → checkbox **"🔬 Mostrar debug do
rastreio"** (aparece o frame anotado com malha e valores ao vivo, incluindo
EAR/MAR/sobrancelha).

## Licença / atribuição

- Cubism SDK (Live2D): licença gratuita para uso pessoal/não comercial (ver
  `third_party/` depois da instalação).
- `tools/facemesh_connections.py`: código de conexões da malha facial
  (`FACEMESH_*`), © Google, licença Apache-2.0 (mantida no cabeçalho).
- Demais dependências: verificar `third_party/` após instalar.
