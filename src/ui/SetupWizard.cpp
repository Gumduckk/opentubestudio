#include "SetupWizard.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QColorDialog>
#include <QFile>
#include <QDir>

// ---------------------------------------------------------------------------
// Página 1: modelo
// ---------------------------------------------------------------------------
SetupWizard::SetupWizard(const QStringList& cameraNames,
                         const QStringList& micNames, QWidget* parent)
    : QWizard(parent)
{
    setWindowTitle(QStringLiteral("Configuração automática — GummVTuber"));
    setOption(QWizard::NoBackButtonOnLastPage, true);
    resize(560, 440);

    // ---------------- Página 1: modelo ----------------
    {
        auto* page = new QWizardPage;
        page->setTitle(QStringLiteral("Escolher o modelo Live2D"));
        auto* v = new QVBoxLayout(page);
        auto* label = new QLabel(
            QStringLiteral("Selecione o arquivo <b>.model3.json</b> do modelo "
                           "(Cubism 3/4/5) ou <b>model.json</b> (Cubism 2).<br><br>"
                           "O assistente vai analisar o modelo e configurar "
                           "parâmetros, expressões e física automaticamente."), page);
        label->setWordWrap(true);
        v->addWidget(label);

        auto* h = new QHBoxLayout;
        _modelEdit = new QLineEdit(page);
        _modelEdit->setPlaceholderText(QStringLiteral("/caminho/para/modelo.model3.json"));
        auto* browse = new QPushButton(QStringLiteral("Procurar..."), page);
        h->addWidget(_modelEdit, 1);
        h->addWidget(browse);
        v->addLayout(h);

        auto* analyze = new QPushButton(QStringLiteral("Analisar modelo"), page);
        v->addWidget(analyze);
        v->addStretch();

        connect(browse, &QPushButton::clicked, this, [this]() {
            const QString f = QFileDialog::getOpenFileName(
                this, QStringLiteral("Escolher modelo"),
                _modelEdit->text().isEmpty()
                    ? QDir::homePath()
                    : QFileInfo(_modelEdit->text()).absolutePath(),
                QStringLiteral("Modelos Live2D (*.model3.json model.json);;Todos (*)"));
            if (!f.isEmpty()) {
                _modelEdit->setText(f);
                emit analyzeRequested(f);
            }
        });
        connect(analyze, &QPushButton::clicked, this, [this]() {
            if (!_modelEdit->text().isEmpty())
                emit analyzeRequested(_modelEdit->text());
        });
        connect(_modelEdit, &QLineEdit::editingFinished, this, [this]() {
            if (!_modelEdit->text().isEmpty())
                emit analyzeRequested(_modelEdit->text());
        });

        setPage(PageModel, page);
    }

    // ---------------- Página 2: fundo ----------------
    {
        auto* page = new QWizardPage;
        page->setTitle(QStringLiteral("Fundo (para usar no OBS)"));
        auto* v = new QVBoxLayout(page);
        auto* label = new QLabel(
            QStringLiteral("• <b>Tela verde</b>: capture a janela no OBS e aplique o "
                           "filtro <i>Chroma Key</i> (mais compatível).<br>"
                            "• <b>Transparente</b>: fundo alpha real — funciona bem em "
                           "captura de janela no X11/compositores que suportam janela ARGB."), page);
        label->setWordWrap(true);
        v->addWidget(label);

        _bgGreen = new QRadioButton(QStringLiteral("Tela verde (recomendado para OBS)"), page);
        _bgGreen->setChecked(true);
        _bgTransparent = new QRadioButton(QStringLiteral("Fundo transparente"), page);
        _bgCustom = new QRadioButton(QStringLiteral("Cor personalizada"), page);
        v->addWidget(_bgGreen);
        v->addWidget(_bgTransparent);
        v->addWidget(_bgCustom);

        auto* h = new QHBoxLayout;
        _bgColorBtn = new QPushButton(QStringLiteral("Escolher cor..."), page);
        auto* preview = new QLabel(page);
        preview->setFixedSize(48, 24);
        preview->setStyleSheet(QStringLiteral(
            "background-color:#00c83c;border:1px solid #555;border-radius:4px;"));
        h->addWidget(_bgColorBtn);
        h->addWidget(preview);
        h->addStretch();
        v->addLayout(h);
        v->addStretch();

        connect(_bgColorBtn, &QPushButton::clicked, this, [this, preview]() {
            const QColor c = QColorDialog::getColor(_bgColor, this,
                                                    QStringLiteral("Cor de fundo"));
            if (c.isValid()) {
                _bgColor = c;
                preview->setStyleSheet(
                    QStringLiteral("background-color:%1;border:1px solid #555;border-radius:4px;")
                        .arg(c.name()));
                _bgCustom->setChecked(true);
            }
        });

        setPage(PageBackground, page);
    }

    // ---------------- Página 3: rastreio ----------------
    {
        auto* page = new QWizardPage;
        page->setTitle(QStringLiteral("Rastreio (como o modelo te segue)"));
        auto* v = new QVBoxLayout(page);
        auto* label = new QLabel(
            QStringLiteral("Dá pra usar tudo junto: a webcam tem prioridade sobre o "
                           "mouse quando ela encontra seu rosto."), page);
        label->setWordWrap(true);
        v->addWidget(label);

        _chkMouse = new QCheckBox(QStringLiteral("Mouse (cabeça e olhos seguem o cursor)"), page);
        _chkMouse->setChecked(true);
        _chkMic = new QCheckBox(QStringLiteral("Microfone (boca abre quando você fala)"), page);
        _chkMic->setChecked(true);
        _chkWebcam = new QCheckBox(QStringLiteral("Webcam (rastreio facial completo)"), page);
        v->addWidget(_chkMouse);
        v->addWidget(_chkMic);
        v->addWidget(_chkWebcam);

        auto* form = new QFormLayout;
        _comboCam = new QComboBox(page);
        _comboMic = new QComboBox(page);
        for (const QString& n : cameraNames)
            _comboCam->addItem(n);
        if (_comboCam->count() == 0)
            _comboCam->addItem(QStringLiteral("(nenhuma câmera encontrada)"));
        for (const QString& n : micNames)
            _comboMic->addItem(n);
        if (_comboMic->count() == 0)
            _comboMic->addItem(QStringLiteral("(nenhum microfone encontrado)"));
        form->addRow(QStringLiteral("Câmera:"), _comboCam);
        form->addRow(QStringLiteral("Microfone:"), _comboMic);
        v->addLayout(form);

        auto* h = new QHBoxLayout;
        auto* calib = new QPushButton(QStringLiteral("Calibrar agora (olhe reto pra câmera)"), page);
        _calibStatus = new QLabel(page);
        h->addWidget(calib);
        h->addWidget(_calibStatus, 1);
        v->addLayout(h);
        v->addStretch();

        connect(_chkWebcam, &QCheckBox::toggled, _comboCam, &QComboBox::setEnabled);
        connect(_chkMic, &QCheckBox::toggled, _comboMic, &QComboBox::setEnabled);
        _comboCam->setEnabled(false);
        connect(calib, &QPushButton::clicked, this, [this]() {
            _calibStatus->setText(QStringLiteral("Calibrando... (olhe reto, ~1.5s)"));
            emit calibrateRequested();
        });

        setPage(PageTracking, page);
    }

    // ---------------- Página 4: resumo ----------------
    {
        auto* page = new QWizardPage;
        page->setTitle(QStringLiteral("Pronto!"));
        auto* v = new QVBoxLayout(page);
        _summaryLabel = new QLabel(page);
        _summaryLabel->setWordWrap(true);
        _summaryLabel->setTextFormat(Qt::RichText);
        v->addWidget(_summaryLabel);
        v->addStretch();
        setPage(PageSummary, page);
    }
}

// ---------------------------------------------------------------------------
// Acessores
// ---------------------------------------------------------------------------
QString SetupWizard::modelPath() const
{
    return _modelEdit->text().trimmed();
}

bool SetupWizard::transparentBackground() const
{
    return _bgTransparent->isChecked();
}

QColor SetupWizard::backgroundColor() const
{
    return _bgColor;
}

bool SetupWizard::mouseEnabled() const { return _chkMouse->isChecked(); }
bool SetupWizard::micEnabled() const   { return _chkMic->isChecked(); }
bool SetupWizard::webcamEnabled() const{ return _chkWebcam->isChecked(); }
int  SetupWizard::webcamIndex() const  { return _comboCam->currentIndex(); }
int  SetupWizard::micIndex() const     { return _comboMic->currentIndex(); }

void SetupWizard::setSummaryText(const QString& text)
{
    _summaryLabel->setText(QStringLiteral(
        "<div style='font-size:12pt;'><b>Análise concluída:</b></div>"
        "<pre style='font-family:monospace;background:#202020;padding:8px;"
        "border-radius:6px;'>%1</pre>").arg(text.toHtmlEscaped()));
}
