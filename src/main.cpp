/*
 * OpentubeStudio — player Live2D nativo para Linux (estilo VTube Studio).
 *
 * Renderiza modelos Live2D (Cubism 3/4/5) com:
 *   - fundo de tela verde (cor configurável) ou transparente, para OBS;
 *   - rastreio por mouse, microfone (lip sync) e webcam (MediaPipe);
 *   - configuração automática de parâmetros (importa do VTube Studio).
 */
#include <QApplication>
#include <QSurfaceFormat>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QTimer>

#include <cstdlib>
#include <cstring>

#include <CubismFramework.hpp>
#include <ICubismAllocator.hpp>

#include "ui/MainWindow.h"

// Alocador padrão do Cubism (malloc/free alinhado).
class AppAllocator : public Csm::ICubismAllocator
{
public:
    void* Allocate(Csm::csmSizeType size) override { return std::malloc(size); }
    void Deallocate(void* memory) override { std::free(memory); }
    void* AllocateAligned(Csm::csmSizeType size, Csm::csmUint32 alignment) override
    {
        const Csm::csmUint32 align = alignment < sizeof(void*)
                                         ? static_cast<Csm::csmUint32>(sizeof(void*))
                                         : alignment;
        void* p = nullptr;
        if (posix_memalign(&p, align, size) != 0)
            return nullptr;
        return p;
    }
    void DeallocateAligned(void* alignedMemory) override { std::free(alignedMemory); }
};

// Lê um arquivo para o Cubism Framework (shaders em FrameworkShaders/).
// O buffer é alocado com malloc porque o ReleaseBytes correspondente usa free.
static Csm::csmByte* loadCubismFile(const std::string filePath, Csm::csmSizeInt* outSize)
{
    QFile f(QString::fromStdString(filePath));
    if (!f.open(QIODevice::ReadOnly) || outSize == nullptr)
        return nullptr;
    const QByteArray data = f.readAll();
    if (data.isEmpty())
        return nullptr;
    auto* buf = static_cast<Csm::csmByte*>(std::malloc(static_cast<size_t>(data.size())));
    if (!buf)
        return nullptr;
    std::memcpy(buf, data.constData(), static_cast<size_t>(data.size()));
    *outSize = static_cast<Csm::csmSizeInt>(data.size());
    return buf;
}

static void releaseCubismFile(Csm::csmByte* byteData)
{
    std::free(byteData);
}

int main(int argc, char** argv)
{
    // Formato do contexto GL: stencil (máscaras do Cubism) e depth explícito
    // (alguns drivers Mesa/Wayland só apresentam o FBO do QOpenGLWidget
    // corretamente com depth definido). Alpha NÃO é pedido por padrão: com
    // alpha no swapchain o compositor Wayland pode exibir o widget como preto
    // (só atualiza no resize). O modo "fundo transparente" reconfigura o
    // formato quando necessário.
    QSurfaceFormat fmt;
    fmt.setRenderableType(QSurfaceFormat::OpenGL);
    fmt.setDepthBufferSize(24);
    fmt.setStencilBufferSize(8);
    fmt.setAlphaBufferSize(0);
    fmt.setSamples(0);
    fmt.setSwapInterval(1);
    QSurfaceFormat::setDefaultFormat(fmt);

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("OpentubeStudio"));
    QApplication::setOrganizationName(QStringLiteral("OpentubeStudio"));

    // Ícone do aplicativo (placeholder; trocar icon.png pela arte final)
    const QString iconPath = QCoreApplication::applicationDirPath() + QStringLiteral("/icon.png");
    if (QFileInfo::exists(iconPath))
        app.setWindowIcon(QIcon(iconPath));

    // Os shaders do Cubism Framework e o rastreador Python são resolvidos
    // relativamente ao diretório do executável.
    QDir::setCurrent(QCoreApplication::applicationDirPath());

    // Inicializa o Cubism Framework (o Core renderiza tudo offline).
    // O Cubism 5 carrega os shaders de FrameworkShaders/ via LoadFileFunction.
    static AppAllocator allocator;
    static Csm::CubismFramework::Option option{};
    option.LoggingLevel = Csm::CubismFramework::Option::LogLevel_Off;
    option.LoadFileFunction = loadCubismFile;
    option.ReleaseBytesFunction = releaseCubismFile;
    Csm::CubismFramework::StartUp(&allocator, &option);
    Csm::CubismFramework::Initialize();

    app.setStyleSheet(QStringLiteral(R"(
        QWidget { background: #232629; color: #e8e8e8; font-size: 13px; }
        QMainWindow::separator { background: #3a3f44; width: 1px; height: 1px; }
        QToolBar { background: #2b2f33; border: none; spacing: 6px; padding: 4px; }
        QToolButton, QPushButton {
            background: #3a3f44; border: 1px solid #4a4f55; border-radius: 5px;
            padding: 5px 10px;
        }
        QToolButton:hover, QPushButton:hover { background: #454b51; }
        QToolButton:checked, QPushButton:checked { background: #2d6a4f; }
        QComboBox, QLineEdit, QTableWidget, QSpinBox {
            background: #2b2f33; border: 1px solid #4a4f55; border-radius: 4px;
            padding: 3px 6px; selection-background-color: #2d6a4f;
        }
        QTableWidget::item:selected { background: #2d6a4f; }
        QHeaderView::section {
            background: #2b2f33; border: none; border-bottom: 1px solid #4a4f55;
            padding: 4px;
        }
        QTabWidget::pane { border: none; }
        QTabBar::tab {
            background: #2b2f33; padding: 6px 12px; border-bottom: 2px solid transparent;
        }
        QTabBar::tab:selected { border-bottom: 2px solid #40916c; }
        QDockWidget { titlebar-close-icon: none; }
        QStatusBar { background: #2b2f33; }
        QScrollBar:vertical { background: #232629; width: 10px; }
        QScrollBar::handle:vertical { background: #4a4f55; border-radius: 5px; min-height: 20px; }
        QCheckBox::indicator, QRadioButton::indicator { width: 16px; height: 16px; }
        QSlider::groove:horizontal { background: #3a3f44; height: 4px; border-radius: 2px; }
        QSlider::handle:horizontal {
            background: #40916c; width: 14px; margin: -5px 0; border-radius: 7px;
        }
    )"));

    MainWindow window;

    // Argumento opcional: --model caminho/para/modelo.model3.json
    // (também aceita o caminho solto, sem a flag).
    QString modelArg;
    for (int i = 1; i < argc; ++i) {
        const QString a = QString::fromLocal8Bit(argv[i]);
        if (a == QLatin1String("--model") && i + 1 < argc) {
            modelArg = QString::fromLocal8Bit(argv[++i]);
        } else if (!a.startsWith(QLatin1Char('-')) && a.endsWith(QLatin1String(".model3.json"))) {
            modelArg = a;
        }
    }
    if (!modelArg.isEmpty()) {
        const QFileInfo fi(modelArg);
        window.openModelPath(fi.absoluteFilePath());
    }

    window.show();

    const int ret = app.exec();
    Csm::CubismFramework::Dispose();
    return ret;
}
