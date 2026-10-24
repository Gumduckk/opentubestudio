#pragma once
/*
 * SetupWizard — assistente de configuração automática, inspirado no
 * "importar modelo novo" do VTube Studio:
 *   1. Escolher o arquivo .model3.json
 *   2. Escolher o fundo (verde / transparente / cor)
 *   3. Escolher rastreio (mouse, microfone, webcam) + calibrar
 *   4. Resumo do que foi configurado automaticamente
 */
#include <QWizard>
#include <QColor>

class QLineEdit;
class QRadioButton;
class QCheckBox;
class QLabel;
class QComboBox;
class QPushButton;

class SetupWizard : public QWizard
{
    Q_OBJECT
public:
    enum { PageModel, PageBackground, PageTracking, PageSummary };

    explicit SetupWizard(const QStringList& cameraNames,
                         const QStringList& micNames,
                         QWidget* parent = nullptr);

    QString modelPath() const;

    bool transparentBackground() const;
    QColor backgroundColor() const;

    bool mouseEnabled() const;
    bool micEnabled() const;
    bool webcamEnabled() const;
    int  webcamIndex() const;
    int  micIndex() const;

    void setSummaryText(const QString& text);

signals:
    // Emitido quando o usuário clica "Analisar modelo" (página 1)
    void analyzeRequested(const QString& model3Path);
    // Emitido quando pede calibração (página 3)
    void calibrateRequested();

private:
    QLineEdit* _modelEdit = nullptr;

    QRadioButton* _bgGreen = nullptr;
    QRadioButton* _bgTransparent = nullptr;
    QRadioButton* _bgCustom = nullptr;
    QPushButton* _bgColorBtn = nullptr;
    QColor _bgColor = QColor(0, 200, 60);

    QCheckBox* _chkMouse = nullptr;
    QCheckBox* _chkMic = nullptr;
    QCheckBox* _chkWebcam = nullptr;
    QComboBox* _comboCam = nullptr;
    QComboBox* _comboMic = nullptr;
    QLabel* _calibStatus = nullptr;

    QLabel* _summaryLabel = nullptr;
};
