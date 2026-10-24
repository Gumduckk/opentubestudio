#pragma once
/*
 * EvdevMouse — leitura GLOBAL do cursor via evdev (/dev/input/event*).
 *
 * No Wayland o Qt não informa a posição do cursor fora da janela do app
 * (QCursor::pos() congela). O evdev entrega os eventos crus do dispositivo,
 * então dá para reconstruir a posição global sem depender do compositor —
 * funciona em KDE, GNOME, wlroots, X11, etc.
 *
 * Requer acesso de leitura a /dev/input/event* (grupo "input" ou regra udev).
 * Se não houver permissão, poll() retorna false e o chamador deve cair de
 * volta para o Qt (QCursor::pos()).
 */
#include <QPoint>
#include <QString>
#include <QStringList>
#include <QList>

class EvdevMouse
{
public:
    EvdevMouse();
    ~EvdevMouse();

    // Abre os dispositivos de mouse disponíveis. Retorna o número aberto.
    int open();

    // true se há pelo menos um dispositivo de mouse aberto.
    bool running() const { return !_fds.isEmpty(); }

    // Lê todos os eventos pendentes e atualiza a posição global acumulada.
    // Retorna true se houve movimento desde a última chamada.
    bool poll();

    // Última posição global conhecida (pixels, espaço global do X/Wayland).
    QPoint pos() const { return _pos; }

    // Sincroniza a posição conhecida (ex.: quando o Qt informa a posição
    // real dentro da janela, corrigimos o acumulador).
    void setPos(const QPoint& p) { _pos = p; }

    // Diagnóstico dos dispositivos abertos.
    QStringList deviceNames() const { return _names; }

    // Motivo de falha (permissões etc.).
    QString lastError() const { return _error; }

private:
    bool openDevice(const QString& path, const QString& name);

    QList<int>   _fds;
    QStringList  _names;
    QPoint       _pos;
    QString      _error;
};
