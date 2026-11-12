#!/usr/bin/env python3
"""
tracker_daemon.py — rastreador facial do GummVTuber.

Usa o MediaPipe Face Landmarker (Google, grátis, roda 100% offline) para ler
a webcam e emitir, uma linha JSON por frame, os dados usados pelo app:

  {"found":1,"yaw":-3.2,"pitch":1.1,"roll":0.4,
   "blinkL":0.05,"blinkR":0.04,"jaw":0.0,"smile":0.1,"brow":0.2,
   "eyeX":-0.02,"eyeY":0.01,
   "pucker":0.0,"funnel":0.0,"cheek":0.0,"browDown":0.1,
   "jawSide":0.0,"frown":0.0,"stretch":0.0}

- yaw/pitch/roll: ângulos da cabeça em graus (calibração remove o offset).
- blink*: 0 = olho aberto, 1 = fechado.
- jaw: boca aberta (0..1, escala do blendshape).
- eyeX/eyeY: deslocamento da íris normalizado pelo tamanho do olho.

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


try:
    import cv2
    import numpy as np
    import mediapipe as mp
    from mediapipe.tasks.python import vision
    from mediapipe.tasks.python.core.base_options import BaseOptions
except Exception as e:  # noqa: BLE001
    die("dependências faltando (%s). Instale com:\n"
        "        python3 -m pip install --user mediapipe opencv-python" % e, 2)


def parse_args():
    ap = argparse.ArgumentParser(description="Rastreador facial do GummVTuber")
    ap.add_argument("--model", required=True, help="caminho do face_landmarker.task")
    ap.add_argument("--camera", type=int, default=0, help="índice da câmera (v4l2)")
    ap.add_argument("--width", type=int, default=1280, help="(ignorado; compat)")
    ap.add_argument("--height", type=int, default=720, help="(ignorado; compat)")
    return ap.parse_args()


def limit_frame(frame, max_w=1280, max_h=720):
    """Reduz frames gigantes (ex.: webcam virtual 4K) para acelerar o rastreio."""
    h, w = frame.shape[:2]
    if w <= max_w and h <= max_h:
        return frame
    scale = min(max_w / float(w), max_h / float(h))
    return cv2.resize(frame, (int(w * scale), int(h * scale)),
                      interpolation=cv2.INTER_AREA)


def list_video_devices():
    """Lista os índices de /dev/video* presentes no sistema."""
    return [i for i in range(16) if os.path.exists("/dev/video%d" % i)]


def open_video(index, width, height, fourcc=None, tries=20):
    """Abre um dispositivo e devolve (cap, frame) só se ele entregar imagem."""
    # OpenCV 5 pode falhar ao abrir por índice numérico ("can't open camera
    # by index"), mas abre normalmente pelo caminho /dev/videoN.
    path = "/dev/video%d" % index
    cap = cv2.VideoCapture(path, cv2.CAP_V4L2)
    if not cap.isOpened():
        cap.release()
        cap = cv2.VideoCapture(path)
    if not cap.isOpened():
        cap.release()
        return None, None

    # Mantém só o frame mais recente: evita atraso quando a detecção demora.
    cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)
    if fourcc:
        # Webcams USB em 720p+ costumam precisar de MJPG; em YUYV o
        # dispositivo abre mas não consegue entregar frames.
        cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*fourcc))
    if width > 0:
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, width)
    if height > 0:
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, height)

    for _ in range(tries):
        ok, frame = cap.read()
        if ok and frame is not None:
            return cap, frame
        time.sleep(0.05)
    cap.release()
    return None, None


def is_virtual_camera(index):
    """True para câmeras virtuais (v4l2loopback: OBS/Iriun), que não têm
    o symlink 'device' no sysfs como as webcams USB reais."""
    return not os.path.exists("/sys/class/video4linux/video%d/device" % index)


def open_camera(index, width, height):
    """Abre a câmera escolhida e garante que ela realmente entrega frames.

    Se o dispositivo abrir mas não entregar imagem (nó de metadados, câmera
    virtual sem produtor como OBS Virtual Cam / v4l2loopback parado etc.),
    tenta os outros /dev/video* automaticamente. Assim o rastreio funciona
    mesmo quando o índice salvo aponta para uma câmera vazia.
    """
    candidates = [index] + [i for i in list_video_devices() if i != index]
    tested = []
    for cam in candidates:
        if cam not in tested:
            tested.append(cam)
        if is_virtual_camera(cam):
            # Câmera virtual já tem formato/resolução definidos pelo produtor
            # (OBS/Iriun). Não forçar MJPG/tamanho: use o formato atual e só
            # depois tente a resolução pedida.
            attempts = ((0, 0, None), (width, height, None))
        else:
            # Webcam USB: em 720p+ normalmente precisa de MJPG; se falhar,
            # tenta o formato padrão em 640x480.
            attempts = ((width, height, "MJPG"), (640, 480, None))

        for w, h, fourcc in attempts:
            cap, frame = open_video(cam, w, h, fourcc)
            if frame is not None:
                if cam != index:
                    log("câmera %d não entregou imagem; usando /dev/video%d"
                        % (index, cam))
                frame = limit_frame(frame)
                log("câmera %d: %dx%d"
                    % (cam, frame.shape[1], frame.shape[0]))
                return cap, frame
        log("câmera %d abriu, mas não entregou imagem; tentando outra..." % cam)

    die("nenhuma câmera entregou imagem (testadas: %s).\n"
        "        Se usa OBS Virtual Cam, inicie a câmera virtual no OBS.\n"
        "        Se usa Iriun/telefone, conecte o celular.\n"
        "        Feche VTube Studio/OpenSeeFace/OBS que estejam usando a\n"
        "        mesma câmera e escolha a câmera correta na aba Rastreio."
        % ", ".join("/dev/video%d" % i for i in tested), 3)


def euler_deg(R):
    """Extrai yaw/pitch/roll (graus) SEMÂNTICOS da matriz de rotação 3x3.

    O modelo canônico do MediaPipe é (x direita, y pra cima, z saindo da tela
    em direção ao rosto), então:
      - virar a cabeça  -> rotação sobre Y
      - inclinar o queixo (nod) -> rotação sobre X
      - deitar a cabeça (tilt)  -> rotação sobre Z

    A decomposição crua (ZYX intrínseca) devolve os eixos na ordem trocada;
    aqui remapeamos para os nomes corretos, com os sinais no padrão do
    VTube Studio (o mesmo usado pelo lilacs-vts-tracker):
      yaw   = -rotação_Y   (FaceAngleX)
      pitch = -rotação_X   (FaceAngleY)
      roll  = +rotação_Z   (FaceAngleZ)
    """
    sy = math.sqrt(R[0][0] ** 2 + R[1][0] ** 2)
    if sy > 1e-6:
        rot_y = math.atan2(-R[2][0], sy)
        rot_z = math.atan2(R[1][0], R[0][0])
        rot_x = math.atan2(R[2][1], R[2][2])
    else:
        rot_y = math.atan2(R[0][1], R[1][1])
        rot_z = math.atan2(-R[2][0], sy)
        rot_x = 0.0
    yaw = -math.degrees(rot_y)
    pitch = -math.degrees(rot_x)
    roll = math.degrees(rot_z)
    if os.environ.get("GUMM_POSE_DEBUG") == "1":
        now = time.time()
        if now - getattr(euler_deg, "_last_log", 0.0) >= 1.0:
            euler_deg._last_log = now
            log("pose yaw=%.1f pitch=%.1f roll=%.1f" % (yaw, pitch, roll))
    return yaw, pitch, roll


def main():
    opts = parse_args()

    options = vision.FaceLandmarkerOptions(
        base_options=BaseOptions(model_asset_path=opts.model),
        running_mode=vision.RunningMode.VIDEO,
        num_faces=1,
        # Limiares mais tolerantes: ajuda com rosto pequeno, de lado ou com
        # pouca luz. O mapeamento de parâmetros já suaviza falsos positivos.
        min_face_detection_confidence=0.3,
        min_face_presence_confidence=0.3,
        min_tracking_confidence=0.3,
        output_face_blendshapes=True,
        output_facial_transformation_matrixes=True,
    )
    try:
        landmarker = vision.FaceLandmarker.create_from_options(options)
    except Exception as e:  # noqa: BLE001
        die("não carregou o modelo de rastreio: %s" % e, 2)

    cap, first = open_camera(opts.camera, opts.width, opts.height)

    log("rastreador ativo (câmera %d). Ctrl+C ou comando quit para sair."
        % opts.camera)
    print(json.dumps({"ok": 1}), flush=True)  # handshake

    last_ts = -1
    stdin_fd = sys.stdin.fileno()
    pending = first
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

            if pending is not None:
                frame = pending
                pending = None
            else:
                ok, frame = cap.read()
                if not ok:
                    time.sleep(0.01)
                    continue
                frame = limit_frame(frame)

            rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb)

            ts = int(time.time() * 1000)
            if ts <= last_ts:
                ts = last_ts + 1
            last_ts = ts

            result = landmarker.detect_for_video(mp_image, ts)

            if not result.face_landmarks:
                print(json.dumps({"found": 0}), flush=True)
                continue

            lm = result.face_landmarks[0]
            out = {"found": 1}

            # --- cabeça ---
            # mediapipe >= 0.10 devolve um np.ndarray 4x4 row-major direto;
            # versões antigas devolvem MatrixData com o buffer em .data.
            tm = result.facial_transformation_matrixes[0]
            try:
                M = np.asarray(tm, dtype=np.float32).reshape(4, 4)
            except Exception:
                M = np.asarray(tm.data, dtype=np.float32).reshape(4, 4)
            R = [[float(M[0][0]), float(M[0][1]), float(M[0][2])],
                 [float(M[1][0]), float(M[1][1]), float(M[1][2])],
                 [float(M[2][0]), float(M[2][1]), float(M[2][2])]]
            yaw, pitch, roll = euler_deg(R)
            out["yaw"], out["pitch"], out["roll"] = yaw, pitch, roll

            # --- blendshapes (olhos, boca, sobrancelhas) ---
            bs = {}
            for b in result.face_blendshapes[0]:
                bs[b.category_name] = b.score
            out["blinkL"] = min(1.0, bs.get("eyeBlinkLeft", 0.0)
                                + 0.5 * bs.get("eyeSquintLeft", 0.0))
            out["blinkR"] = min(1.0, bs.get("eyeBlinkRight", 0.0)
                                + 0.5 * bs.get("eyeSquintRight", 0.0))
            out["jaw"] = min(1.0, bs.get("jawOpen", 0.0))
            out["smile"] = min(1.0, (bs.get("mouthSmileLeft", 0.0)
                                     + bs.get("mouthSmileRight", 0.0)) / 2.0)
            out["brow"] = min(1.0, bs.get("browInnerUp", 0.0)
                              + 0.5 * bs.get("browOuterUpLeft", 0.0))
            # --- expressões extras (biquinho, bochecha, sobrancelha baixa,
            # mandíbula lateral, careta, arqueamento lateral da boca) ---
            out["pucker"] = min(1.0, bs.get("mouthPucker", 0.0))
            out["funnel"] = min(1.0, bs.get("mouthFunnel", 0.0))
            out["cheek"] = min(1.0, bs.get("cheekPuff", 0.0))
            out["browDown"] = min(1.0, (bs.get("browDownLeft", 0.0)
                                        + bs.get("browDownRight", 0.0)) / 2.0)
            out["jawSide"] = max(-1.0, min(1.0, bs.get("jawRight", 0.0)
                                           - bs.get("jawLeft", 0.0)))
            out["frown"] = min(1.0, (bs.get("mouthFrownLeft", 0.0)
                                     + bs.get("mouthFrownRight", 0.0)) / 2.0)
            out["stretch"] = min(1.0, (bs.get("mouthStretchLeft", 0.0)
                                       + bs.get("mouthStretchRight", 0.0)) / 2.0)

            # --- íris (para onde os olhos olham) ---
            try:
                def eye_offset(outer, inner, iris):
                    cx = (lm[outer].x + lm[inner].x) / 2.0
                    cy = (lm[outer].y + lm[inner].y) / 2.0
                    w = abs(lm[inner].x - lm[outer].x) + 1e-6
                    return (lm[iris].x - cx) / w, (lm[iris].y - cy) / w

                lx, ly = eye_offset(33, 133, 468)   # olho esquerdo
                rx, ry = eye_offset(362, 263, 473)   # olho direito
                out["eyeX"] = (lx + rx) / 2.0
                out["eyeY"] = (ly + ry) / 2.0
            except (IndexError, AttributeError):
                out["eyeX"] = 0.0
                out["eyeY"] = 0.0

            print(json.dumps(out), flush=True)
    except KeyboardInterrupt:
        pass
    finally:
        cap.release()
        landmarker.close()
    log("rastreador encerrado.")


if __name__ == "__main__":
    main()
