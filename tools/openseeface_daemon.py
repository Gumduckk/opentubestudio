#!/usr/bin/env python3
"""
openseeface_daemon.py — rastreador facial do GummVTuber via OpenSeeFace.

O OpenSeeFace (github.com/emilianavt/OpenSeeFace) é o mesmo rastreador que o
VTube Studio usa por baixo: rede MobileNetV3 em ONNX, roda na CPU a 30-60 fps.
Este daemon usa a classe `Tracker` diretamente (sem socket UDP) e fala com o
app pelo mesmo protocolo JSON-lines do tracker_daemon.py:

  {"found":1,"yaw":-3.2,"pitch":1.1,"roll":0.4,
   "blinkL":0.05,"blinkR":0.04,"jaw":0.0,"smile":0.1,"brow":0.2,
   "eyeX":-0.02,"eyeY":0.01}

Comandos no stdin (JSON por linha):  {"cmd":"quit"}
"""
import argparse
import json
import math
import os
import select
import sys
import time

def log(*a):
    print("[GummVTuber:webcam]", *a, file=sys.stderr, flush=True)

def die(msg, code):
    log("ERRO:", msg)
    sys.exit(code)

def parse_args():
    ap = argparse.ArgumentParser(description="Rastreador facial OpenSeeFace")
    ap.add_argument("--model", default="", help="(ignorado; compat com o app)")
    ap.add_argument("--camera", type=int, default=0, help="índice da câmera (v4l2)")
    ap.add_argument("--width", type=int, default=1280)
    ap.add_argument("--height", type=int, default=720)
    ap.add_argument("--model-type", type=int, default=3,
                    help="-3..4: maior = melhor qualidade, mais lento (0/1 rígidos)")
    ap.add_argument("--fps", type=int, default=30)
    return ap.parse_args()


def patch_opencv_compat():
    """Compatibilidade com OpenCV novo.

    O OpenSeeFace foi escrito esperando rvec/tvec em 1D. OpenCV 5 devolve
    (3,1), e o np.transpose do tracker vira (1,3), quebrando o solvePnP
    seguinte (ValueError: shapes (3,3) and (1,3) not aligned).
    """
    import cv2
    import numpy as np

    if getattr(cv2, "_gumm_solvepnp_1d", False):
        return
    original = cv2.solvePnP

    def solve_pnp_1d(*args, **kwargs):
        ok, rvec, tvec = original(*args, **kwargs)
        return (ok,
                np.asarray(rvec, dtype=np.float32).reshape(3),
                np.asarray(tvec, dtype=np.float32).reshape(3))

    cv2.solvePnP = solve_pnp_1d
    cv2._gumm_solvepnp_1d = True


def limit_frame(frame, max_w=1280, max_h=720):
    """Reduz frames gigantes (ex.: webcam virtual 4K) para acelerar o rastreio."""
    import cv2

    h, w = frame.shape[:2]
    if w <= max_w and h <= max_h:
        return frame
    scale = min(max_w / float(w), max_h / float(h))
    return cv2.resize(frame, (int(w * scale), int(h * scale)),
                      interpolation=cv2.INTER_AREA)


def open_camera(index, width, height, fps):
    """Abre a câmera pelo backend V4L2 e tenta aplicar resolução/FPS."""
    import cv2

    # OpenCV 5 pode falhar ao abrir por índice numérico; o caminho funciona.
    path = "/dev/video%d" % index
    cap = cv2.VideoCapture(path, cv2.CAP_V4L2)
    if not cap.isOpened():
        cap.release()
        cap = cv2.VideoCapture(path)
    if not cap.isOpened():
        die("não abriu a câmera %d (/dev/video%d)" % (index, index), 3)

    # Mantém só o frame mais recente: evita atraso quando a detecção demora.
    cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, width)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
    cap.set(cv2.CAP_PROP_FPS, fps)

    # A primeira leitura pode falhar enquanto a câmera ajusta exposição.
    for _ in range(20):
        ok, frame = cap.read()
        if ok and frame is not None:
            break
        time.sleep(0.05)
    else:
        cap.release()
        die("a câmera %d abriu, mas não entregou imagem" % index, 3)

    frame = limit_frame(frame)
    h, w = frame.shape[:2]
    real_fps = cap.get(cv2.CAP_PROP_FPS)
    log("câmera %d: %dx%d @ %.0f fps" % (index, w, h, real_fps))
    return cap, frame

def main():
    opts = parse_args()

    # O OpenSeeFace fica em tools/OpenSeeFace. O script pode estar em
    # tools/ (projeto) ou build/tools/ (cópia do CMake); procuramos subindo.
    here = os.path.dirname(os.path.abspath(__file__))
    osf_dir = None
    bases = [here, os.path.join(here, ".."), os.path.join(here, "..", ".."),
             os.path.join(here, "..", "..", ".."), os.path.expanduser("~"),
             os.path.expanduser("~/OpenSeeFace")]
    for base in bases:
        for cand in (os.path.join(base, "OpenSeeFace"),
                     os.path.join(base, "tools", "OpenSeeFace")):
            if os.path.isfile(os.path.join(cand, "tracker.py")):
                osf_dir = os.path.abspath(cand)
                break
        if osf_dir is not None:
            break
    if osf_dir is None:
        die("OpenSeeFace não encontrado. Rode ./setup.sh.", 2)
    sys.path.insert(0, osf_dir)

    try:
        import cv2
        import numpy as np
        from tracker import Tracker, get_model_base_path
        from input_reader import InputReader
    except Exception as e:  # noqa: BLE001
        die("dependências faltando (%s). Rode ./setup.sh para instalar "
            "onnxruntime, opencv-python, pillow e numpy." % e, 2)

    patch_opencv_compat()

    model_dir = get_model_base_path(None)

    cap, first_frame = open_camera(opts.camera, opts.width, opts.height, opts.fps)
    pending_frame = first_frame

    tracker = None
    last_seen = time.monotonic()
    was_found = False
    log("OpenSeeFace ativo (câmera %d, modelo %d). Ctrl+C ou quit para sair."
        % (opts.camera, opts.model_type))
    print(json.dumps({"ok": 1}), flush=True)  # handshake

    stdin_fd = sys.stdin.fileno()
    try:
        while True:
            # comandos do app (não bloqueia)
            try:
                r, _, _ = select.select([stdin_fd], [], [], 0)
                if r:
                    line = sys.stdin.readline()
                    if not line:
                        break
                    try:
                        cmd = json.loads(line)
                    except Exception:  # noqa: BLE001
                        continue
                    if cmd.get("cmd") == "quit":
                        break
            except (ValueError, OSError):
                pass

            if pending_frame is not None:
                frame = pending_frame
                pending_frame = None
            else:
                ok, frame = cap.read()
                if not ok:
                    time.sleep(0.01)
                    continue
                frame = limit_frame(frame)

            if tracker is None:
                height, width = frame.shape[:2]
                tracker = Tracker(
                    width, height, threshold=0.5, max_threads=2, max_faces=1,
                    discard_after=15, scan_every=1, silent=True,
                    model_type=opts.model_type, model_dir=model_dir,
                    no_gaze=False, detection_threshold=0.4,
                    use_retinaface=0, max_feature_updates=900,
                    static_model=False, try_hard=0, bbox_growth=0.12)
                # RetinaFace só entra quando o detector rápido não acha o rosto
                # por alguns instantes (rosto pequeno/longe, pouca luz etc.).
                tracker.retinaface.min_conf = 0.30
                tracker.retinaface_scan.min_conf = 0.30

            # Detector rápido primeiro; se demorar a achar, usa RetinaFace
            # (640x640), que lida melhor com rostos pequenos e luz ruim.
            if time.monotonic() - last_seen > 1.2:
                tracker.use_retinaface = 1
            else:
                tracker.use_retinaface = 0

            faces = tracker.predict(frame)
            if not faces:
                if was_found:
                    log("rosto perdido; procurando novamente...")
                    was_found = False
                print(json.dumps({"found": 0}), flush=True)
                continue

            last_seen = time.monotonic()
            if not was_found:
                log("rosto encontrado.")
                was_found = True

            f = faces[0]
            out = {"found": 1}

            # --- cabeça (euler em radianos -> graus) ---
            if f.euler is not None:
                out["yaw"] = math.degrees(float(f.euler[0]))
                out["pitch"] = math.degrees(float(f.euler[1]))
                out["roll"] = math.degrees(float(f.euler[2]))
            else:
                out["yaw"] = out["pitch"] = out["roll"] = 0.0

            # --- olhos / boca / sobrancelhas (features 0..1) ---
            feat = f.current_features or {}
            def gg(k, default=0.0):
                return float(feat.get(k, default))

            blink = f.eye_blink if f.eye_blink is not None else [0.0, 0.0]
            out["blinkL"] = min(1.0, float(blink[1]))   # espelhado p/ bater com app
            out["blinkR"] = min(1.0, float(blink[0]))

            # mouth_open do OSF costuma vir 0..1; sobrancelhas idem.
            out["jaw"] = max(0.0, min(1.0, gg("mouth_open")))
            smile = (gg("mouth_corner_updown_l") + gg("mouth_corner_updown_r")) / 2.0
            out["smile"] = max(0.0, min(1.0, smile))
            brow = (gg("eyebrow_updown_l") + gg("eyebrow_updown_r")) / 2.0
            out["brow"] = max(0.0, min(1.0, brow))

            # --- olhar (íris não existe no OSF; aproxima pelos cantos do olho) ---
            # landmarks OpenSeeFace (68 pts): olho esq. 36..41, dir. 42..47.
            try:
                lms = f.lms
                # coord é (y, x, conf) na estrutura do OSF
                def eye_center(a, b):
                    ax = (lms[a][0] + lms[b][0]) / 2.0
                    ay = (lms[a][1] + lms[b][1]) / 2.0
                    return ax, ay
                # centro dos olhos relativo ao nariz (30) e queixo (8)
                lcx, lcy = eye_center(36, 39)
                rcx, rcy = eye_center(42, 45)
                eyeCx = (lcx + rcx) / 2.0
                eyeCy = (lcy + rcy) / 2.0
                nx, ny = lms[30][0], lms[30][1]
                # distância inter-ocular como escala
                scale = abs(rcx - lcx) + 1e-6
                out["eyeX"] = max(-1.0, min(1.0, (eyeCx - nx) / scale * 2.0))
                out["eyeY"] = max(-1.0, min(1.0, (eyeCy - ny) / scale * 2.0))
            except Exception:  # noqa: BLE001
                out["eyeX"] = 0.0
                out["eyeY"] = 0.0

            print(json.dumps(out), flush=True)
    except KeyboardInterrupt:
        pass
    finally:
        cap.release()
    log("rastreador encerrado.")

if __name__ == "__main__":
    main()
