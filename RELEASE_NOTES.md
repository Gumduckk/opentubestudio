# OpentubeStudio v0.1.0

Primeira release pública do **OpentubeStudio** — aplicativo Live2D para Linux,
offline e gratuito, para animar seu avatar com rastreio facial real (MediaPipe),
microfone (lip sync) e mouse, no estilo VTube Studio.

> Se você é o amigo que está lendo isso: seja bem-vindo 😄.

---

## O que já funciona

- **Avatar Live2D no Linux** nativo (renderização OpenGL + Cubism Framework, em
  CPU/GPU — sem internet, sem servidores).
- **Rastreio facial completo** com webcam, embutido no AppImage:
  - ângulos da cabeça (yaw/pitch/roll), com eixos corrigidos;
  - piscadas e olhos (EAR + blendshape);
  - boca aberta/boca fechada (MAR + jawOpen), eixo X/Y da boca;
  - sorriso (blendshapes + geometria);
  - sobrancelhas (altura por geometria + franzir);
  - **posição 3D do rosto** (lean in/out, normalizado pelo tamanho da face);
  -  língua para fora, bochechas, nariz, etc.
- **Lip sync** pelo microfone (VU meter, boca abre com a voz).
- **Mouse** controlando cabeça/olhos (inclusive fora da janela, com grupo `input`
  no Linux/Wayland).
- **Expressões e físicas** do modelo (incluindo o nosso Akari, com ParamShock etc.).
- **Captura/OBS**: fundo verde ou transparente, janela ou tela cheia, atalhos
  Ctrl+1..9 para expressões.
- **Debug de rastreio** (aba Rastreio → checkbox 🔬) com a malha da face e valores
  ao vivo (MAR, EAR, yaw/pitch/roll, faceX/Z…).

## Como usar (amigos)

1. Baixe `OpentubeStudio-x86_64.AppImage` nesta página.
2. Marque como executável (`chmod +x`) e abra.
3. Na primeira vez, aponte a pasta do **seu modelo Live2D** (a pasta que contém
   o arquivo `.model3.json`). Os modelos **não acompanham** o app.
4. Escolha microfone/câmera na aba Rastreio, posicione o rosto e clique
   **🎯 Calibrar**.

**Dicas**
- Para OBS: ative "Fundo transparente" e, no X11/XWayland, rode `./iniciar.sh --x11`.
- No Wayland puro (KDE/GNOME) o rastreio do mouse fora da janela exige usuário
  no grupo `input` (veja o `setup.sh` para instrução rápida).

## Requisitos

- **Linux x86_64**, glibc recente.
- `libfuse2` instalada (caso contrário rode o AppImage com
  `--appimage-extract-and-run` ou extraia com `--appimage-extract`).
- GPU com OpenGL (mesa). Webcam real ou virtual (OBS Virtual Cam / v4l2loopback)
  para o rastreio facial.
- Para o áudio funcionar como esperado no microfone, o sistema deve ter
  suporte a PulseAudio (ou PipeWire com `pipewire-pulse`).

## Limitações conhecidas (honestas)

- Somente Linux **x86_64**. Sem build para ARM/Apple Silicon.
- O AppImage **não embute um modelo Live2D** — traga seu próprio `.model3.json`.
- `libffmpegmediaplugin.so` foi excluído do AppImage (causava SIGSEGV no
  carregamento). O áudio funciona via backend PulseAudio.
- O rastreio foi otimizado para uso pessoal/offline; **não há telemetria nem
  qualquer envio de dados para fora da sua máquina**.

## Licença / Atribuição

- Código próprio sob a licença deste repositório.
- **Cubism SDK for Native** — © Live2D Inc. Uso pessoal/não comercial (ver
  `third_party/` no código-fonte; no AppImage o Core está estaticamente linkado).
- **MediaPipe** (rastreio facial) — © Google, Apache-2.0.
- **facemesh_connections.py** (malha da malha facial) — © Google, Apache-2.0.
- Qt6 — LGPL/LPGL conforme módulos usados.

## Agradecimentos

Ao time do Live2D pelo Cubism SDK, ao Google pelo MediaPipe e à comunidade
VTuber/open-source que tornou isso possível.

---

## Para desenvolvedores

```bash
git clone <seu-repo> && cd <pasta>
./portable.sh   # instala dependências + compila
./iniciar.sh    # abre o app
```

Ou gere o AppImage com:

```bash
./tools/appimage.sh
```
