# Prompt — Diagnóstico do OpentubeStudio (AppImage) na máquina do usuário

> Cole isto para um agente/pessoa que vai testar o `OpentubeStudio-x86_64.AppImage`
> no PC específico onde o app **não abriu**, e gerar um `relatorio.txt`.

---

Você é um técnico de diagnóstico Linux. Sua tarefa: descobrir **por que o
OpentubeStudio não abre** no PC desta pessoa e listar **possíveis bugs
específicos desta máquina**. Ao final, gere um arquivo `relatorio.txt` com tudo.

Contexto do app:

- `OpentubeStudio-x86_64.AppImage` é um AppImage Qt6 + OpenGL (Cubism SDK da
  Live2D) para Linux, 100% offline.
- Embute um Python portátil em `opt/python` com mediapipe para rastreio facial
  via webcam (`tools/tracker_daemon.py`).
- O `AppRun` define `LD_LIBRARY_PATH`, `QT_PLUGIN_PATH`,
  `QT_QPA_PLATFORM_PLUGIN_PATH` e `GUMM_PYTHON` apontando para o Python embutido.
- Config do usuário: `~/.config/OpentubeStudio/OpentubeStudio.conf` (QSettings).
- O app exige um modelo Live2D (`*.model3.json`) para exibir algo; ausência de
  modelo NÃO impede a janela de abrir, mas pode confundir o usuário.
- O rastreio facial precisa de webcam em `/dev/video*` e permissão de acesso.

## Regras

1. **Não modifique o sistema** (não instale pacotes, não rode `sudo` destrutivo).
   Pode apenas ler, executar diagnósticos e (se necessário) sugerir correções no
   relatório. Nunca apague nada.
2. Execute tudo **no PC da pessoa**, com os mesmos caminhos que ela usou.
3. Anote **todas** as saídas (stdout/stderr, códigos de saída) no relatório.
4. Se algo já foi tentado pela pessoa (ex.: "deu clique duplo e nada"), registre.

## Passo 1 — Onde está o arquivo e permissões

```bash
ls -l /caminho/para/OpentubeStudio-x86_64.AppImage
mkdir -p /tmp/opentube_diag && cd /tmp/opentube_diag
file /caminho/para/OpentubeStudio-x86_64.AppImage
```

Verifique e registre:

- O arquivo tem permissão de execução? (`chmod +x` foi feito?)
- Sistema de arquivos onde ele está é `noexec`? (`mount | grep \"$(dirname ...)\"`)
- Espaço em disco livre (`df -h`), inode, e se o download está completo
  (comparar tamanho com o original, ~tamanho esperado, `sha256sum` se houver).

## Passo 2 — FUSE / AppImage runtime

AppImages precisam de FUSE (`libfuse2`/`fuse2`) ou extração manual.

```bash
command -v fusermount fusermount3; ls -l /dev/fuse 2>&1
ldd --version | head -1
```

Teste os três modos de execução:

```bash
./OpentubeStudio-x86_64.AppImage ; echo "exit=$?"
./OpentubeStudio-x86_64.AppImage --appimage-extract-and-run ; echo "exit=$?"
./OpentubeStudio-x86_64.AppImage --appimage-extract
```

Registre qual modo funcionou (ou não) — isso isola FUSE vs. app.

## Passo 3 — Ambiente gráfico (causa nº 1 de "não abre" em Qt)

```bash
echo "XDG_SESSION_TYPE=$XDG_SESSION_TYPE WAYLAND_DISPLAY=$WAYLAND_DISPLAY DISPLAY=$DISPLAY"
echo "DESKTOP=$XDG_CURRENT_DESKTOP | QT_QPA_PLATFORM=$QT_QPA_PLATFORM"
gpu=$(command -v glxinfo >/dev/null && glxinfo -B 2>&1 || echo "glxinfo ausente"); echo "$gpu"
```

Se houver `glxinfo`, registre: renderer OpenGL, versão OpenGL, e se caiu em
`llvmpipe`/software rendering (indício de driver quebrado). Teste forçando:

```bash
QT_QPA_PLATFORM=xcb ./OpentubeStudio-x86_64.AppImage --appimage-extract-and-run
QT_QPA_PLATFORM=wayland ./OpentubeStudio-x86_64.AppImage --appimage-extract-and-run
```

Em Qt6 recente, falta de `libxcb-cursor0` / `libxcb-cursor` derruba o plugin
`xcb` silenciosamente — verifique:

```bash
ldconfig -p | grep -i -E "libxcb-cursor|libxcb\.|libxkbcommon|libGL|libEGL" 
```

## Passo 4 — Plugins Qt e bibliotecas faltando

```bash
QT_DEBUG_PLUGINS=1 QT_LOGGING_RULES="*=true" \
  ./OpentubeStudio-x86_64.AppImage --appimage-extract-and-run 2>&1 | tail -100
```

Depois, inspecione o binário e dependências:

```bash
cd /tmp/opentube_diag
./OpentubeStudio-x86_64.AppImage --appimage-extract >/dev/null
ldd squashfs-root/usr/bin/OpentubeStudio 2>&1
ls squashfs-root/usr/plugins/platforms/ 2>&1
```

Registre qualquer `not found`. Verifique também conflito entre libs do sistema e
do AppImage (`LD_LIBRARY_PATH`, `LD_PRELOAD` no `~/.bashrc`/`~/.profile`):

```bash
echo "LD_LIBRARY_PATH=$LD_LIBRARY_PATH LD_PRELOAD=$LD_PRELOAD"
grep -E "LD_(LIBRARY_PATH|PRELOAD)|QT_" ~/.bashrc ~/.profile ~/.xprofile 2>/dev/null
```

## Passo 5 — Python embutido / rastreador facial

```bash
squashfs-root/opt/python/bin/python3 --version 2>&1
squashfs-root/opt/python/bin/python3 -c "import mediapipe, cv2; print('mediapipe OK', mediapipe.__version__)" 2>&1
squashfs-root/opt/python/bin/python3 tools/tracker_daemon.py --help 2>&1 | head -5
ls /dev/video* 2>&1; groups
```

Registre se o Python interno roda (glibc do sistema vs. embutido), se mediapipe
importa (falha comum: CPU sem AVX / glibc antiga), e se a webcam aparece e o
usuário está no grupo `video`.

## Passo 6 — Hardware e sistema

```bash
cat /etc/os-release | head -3; uname -a; nproc; free -h
lspci -nnk | grep -A3 -i -E "vga|3d|display"
```

Registre: distro e versão, kernel, GPU e driver (nouveau/nvidia/amdgpu/i915),
RAM, e se é máquina virtual (afeta OpenGL/FUSE).

## Passo 7 — Config e dados do app

```bash
ls -la ~/.config/OpentubeStudio/ 2>&1
cat ~/.config/OpentubeStudio/OpentubeStudio.conf 2>&1
```

Config corrompida pode crashar na inicialização — teste com config limpa:

```bash
mv ~/.config/OpentubeStudio ~/.config/OpentubeStudio.bak-$(date +%s) 2>&1
./OpentubeStudio-x86_64.AppImage --appimage-extract-and-run ; echo "exit=$?"
```

(Mover de volta depois se não mudar nada. Registre o resultado.)

## Passo 8 — Coletar o relatório

Crie `relatorio.txt` no diretório atual com EXATAMENTE esta estrutura:

```
====================================================
RELATÓRIO — OpentubeStudio AppImage não abre
Data: <data/hora> | PC: <hostname> | Usuário: <user>
====================================================

1. RESUMO EXECUTIVO
   - Sintoma relatado pela pessoa:
   - Veredito provável (1 linha):
   - Gravidade / bloqueia uso? sim/não

2. AMBIENTE (distro, kernel, sessão X11/Wayland, DE, GPU+driver,
   OpenGL renderer, RAM, noexec?, FUSE?)

3. TESTES EXECUTADOS E RESULTADOS
   - execução direta: exit=X, saída: ...
   - --appimage-extract-and-run: exit=X, saída: ...
   - QT_QPA_PLATFORM=xcb / wayland: ...
   - QT_DEBUG_PLUGINS: trecho relevante ...
   - ldd: libs "not found" ...
   - python embutido / mediapipe: ...
   - config limpa testada: sim/não, resultado

4. CAUSA-RAIZ PROVÁVEL (ordenada por probabilidade)
   1. ...
   2. ...

5. POSSÍVEIS BUGS ESPECÍFICOS DESTA MÁQUINA
   - (ex.: falta libxcb-cursor0, FUSE ausente, driver nouveau
     com GL incompleto, CPU sem AVX p/ mediapipe, config corrompida,
     webcam sem permissão, AppImage em partição noexec, antivírus...)

6. CORREÇÕES SUGERIDAS (comando a comando, para a pessoa rodar)
   - ...

7. WORKAROUNDS IMEDIATOS
   - (ex.: --appimage-extract-and-run, QT_QPA_PLATFORM=xcb,
     iniciar via terminal para ver erro, rodar como outro usuário de teste)

8. O QUE NÃO FOI POSSÍVEL VERIFICAR E POR QUÊ

9. ANEXOS
   - saídas completas coladas ou salvas em arquivos
```

## Dicas de interpretação rápida

- `dlopen ... libxcb-cursor.so.0: cannot open` → instalar `libxcb-cursor0` /
  `libxcb-cursor` (Ubuntu 24.04+/Arch recente).
- `FUSE is not available` / `Cannot mount AppImage` → `sudo apt install libfuse2`
  (ou `fuse2`), ou usar `--appimage-extract-and-run`.
- Tela preta com live2d / `Failed to create OpenGL context` → driver GPU;
  testar `QT_QPA_PLATFORM=xcb`, atualizar driver, ou `llvmpipe` como teste.
- Segfault imediato sem mensagem → glibc antiga vs. Python embutido, ou
  conflito de `LD_PRELOAD`; testar com ambiente limpo `env -i HOME=$HOME PATH=/usr/bin:/bin ./AppImage --appimage-extract-and-run`.
- Janela abre mas sem rastreio → problema é webcam/mediapipe (Passo 5), não o
  app em si; verificar `/dev/video0`, grupo `video`, e o log do tracker
  (linhas `WebcamTracker` no stderr).
- `Permission denied` ao executar → `chmod +x`, ou sistema de arquivos `noexec`.
- Funciona para root mas não para o usuário → permissões, grupo `video`, ou
  config em `~/.config/OpentubeStudio`.

Seja objetivo, cole evidências reais (não invente saídas) e termine o
`relatorio.txt` com o veredito em uma frase.
