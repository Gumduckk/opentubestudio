#include "EvdevMouse.h"

#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <string.h>

#include <linux/input.h>

#include <QDir>
#include <QList>
#include <sys/ioctl.h>

EvdevMouse::EvdevMouse() = default;

EvdevMouse::~EvdevMouse()
{
    for (int fd : _fds)
        ::close(fd);
    _fds.clear();
}

// Verifica se o dispositivo tem a capacidade pedida usando ioctl.
static bool hasBit(int fd, unsigned long ev, unsigned long code)
{
    unsigned long bits[EV_MAX / (8 * sizeof(long)) + 2];
    memset(bits, 0, sizeof(bits));
    if (ioctl(fd, EVIOCGBIT(ev, sizeof(bits)), bits) < 0)
        return false;
    return (bits[code / (8 * sizeof(long))] >> (code % (8 * sizeof(long)))) & 1UL;
}

static bool hasEvBit(int fd, unsigned long ev)
{
    unsigned long bits[EV_MAX / (8 * sizeof(long)) + 2];
    memset(bits, 0, sizeof(bits));
    if (ioctl(fd, EVIOCGBIT(0, sizeof(bits)), bits) < 0)
        return false;
    return (bits[ev / (8 * sizeof(long))] >> (ev % (8 * sizeof(long)))) & 1UL;
}

int EvdevMouse::open()
{
    _fds.clear();
    _names.clear();
    _error.clear();

    const QStringList entries = QDir(QStringLiteral("/dev/input"))
        .entryList({ QStringLiteral("event*") }, QDir::System | QDir::Files, QDir::Name);

    if (entries.isEmpty()) {
        _error = QStringLiteral("Sem acesso a /dev/input (entre no grupo 'input' "
                                "ou instale a regra udev).");
        return 0;
    }

    for (const QString& e : entries) {
        const QString path = QStringLiteral("/dev/input/") + e;
        int fd = ::open(path.toUtf8().constData(), O_RDONLY | O_NONBLOCK);
        if (fd < 0) {
            if (_error.isEmpty())
                _error = QStringLiteral("Sem permissão de leitura em %1").arg(path);
            continue;
        }

        // Um mouse tem movimento relativo em X e Y. Teclados não têm EV_REL.
        const bool isRel  = hasEvBit(fd, EV_REL) && hasBit(fd, EV_REL, REL_X)
                            && hasBit(fd, EV_REL, REL_Y);
        const bool isAbs  = hasEvBit(fd, EV_ABS) && hasBit(fd, EV_ABS, ABS_X)
                            && hasBit(fd, EV_ABS, ABS_Y);
        if (!isRel && !isAbs) {
            ::close(fd);
            continue;
        }

        // Só consideramos ponteiros (que têm botões), para ignorar giroscópios.
        const bool hasBtn = hasEvBit(fd, EV_KEY) && (hasBit(fd, EV_KEY, BTN_LEFT)
                                                     || hasBit(fd, EV_KEY, BTN_TOUCH));
        if (!hasBtn) {
            ::close(fd);
            continue;
        }

        char name[256] = {0};
        ioctl(fd, EVIOCGNAME(sizeof(name)), name);

        _fds.append(fd);
        _names.append(QString::fromUtf8(name).trimmed().isEmpty()
                          ? e : QString::fromUtf8(name).trimmed());
    }

    if (_fds.isEmpty() && _error.isEmpty())
        _error = QStringLiteral("Nenhum mouse encontrado em /dev/input.");

    return _fds.size();
}

bool EvdevMouse::poll()
{
    if (_fds.isEmpty())
        return false;

    bool moved = false;

    // Coleta os fds prontos sem bloquear.
    QList<pollfd> pfds;
    pfds.reserve(_fds.size());
    for (int fd : _fds) {
        pollfd p;
        p.fd = fd;
        p.events = POLLIN;
        p.revents = 0;
        pfds.append(p);
    }

    const int n = ::poll(pfds.data(), pfds.size(), 0);
    if (n <= 0)
        return false;

    for (const pollfd& p : pfds) {
        if (!(p.revents & POLLIN))
            continue;

        input_event evs[64];
        for (;;) {
            const ssize_t got = ::read(p.fd, evs, sizeof(evs));
            if (got <= 0) {
                if (got < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
                    break;
                break;
            }
            const int count = static_cast<int>(got / sizeof(input_event));
            for (int i = 0; i < count; ++i) {
                const input_event& ev = evs[i];
                if (ev.type == EV_REL) {
                    if (ev.code == REL_X) { _pos.rx() += ev.value; moved = true; }
                    else if (ev.code == REL_Y) { _pos.ry() += ev.value; moved = true; }
                } else if (ev.type == EV_ABS) {
                    // Dispositivos absolutos (touchpad/tablet): valor já é
                    // coordenada na tela da maioria dos drivers libinput.
                    if (ev.code == ABS_X) { _pos.rx() = ev.value; moved = true; }
                    else if (ev.code == ABS_Y) { _pos.ry() = ev.value; moved = true; }
                }
            }
        }
    }

    return moved;
}
