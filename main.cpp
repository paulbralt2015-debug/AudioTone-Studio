#include <QApplication>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMediaDevices>
#include <QMessageBox>
#include <QMutex>
#include <QMutexLocker>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>
#include <QIODevice>

#include <array>
#include <cmath>
#include <algorithm>

static constexpr int CHANNEL_COUNT = 6;
static constexpr int SAMPLE_RATE = 48000;
static constexpr double PI = 3.14159265358979323846;

static const std::array<QString, CHANNEL_COUNT> CHANNEL_NAMES = {
    "Avant gauche",
    "Avant droite",
    "Centre",
    "Surround gauche",
    "Surround droite",
    "LFE - Graves"
};

class ToneGenerator final : public QIODevice
{
public:
    ToneGenerator()
    {
        frequencies.fill(440.0);
        volumes.fill(0.25);
        enabled.fill(true);
        enabled[5] = false;
        phases.fill(0.0);
        open(QIODevice::ReadOnly);
    }

    void setChannel(int channel, double frequency,
                    double volume, bool active)
    {
        if (channel < 0 || channel >= CHANNEL_COUNT)
            return;

        QMutexLocker locker(&mutex);
        frequencies[channel] = frequency;
        volumes[channel] = volume;
        enabled[channel] = active;
    }

    void synchronize(double frequency)
    {
        QMutexLocker locker(&mutex);
        frequencies.fill(frequency);
    }

    bool isSequential() const override
    {
        return true;
    }

protected:
    qint64 readData(char *data, qint64 maxSize) override
    {
        QMutexLocker locker(&mutex);

        // PCM 16 bits, six canaux entrelacés.
        constexpr int bytesPerFrame = CHANNEL_COUNT * 2;
        const qint64 frameCount = maxSize / bytesPerFrame;
        auto *output = reinterpret_cast<qint16 *>(data);

        for (qint64 frame = 0; frame < frameCount; ++frame) {
            for (int channel = 0; channel < CHANNEL_COUNT; ++channel) {
                double sample = 0.0;

                if (enabled[channel]) {
                    sample = std::sin(phases[channel] * 2.0 * PI)
                             * volumes[channel];

                    phases[channel] +=
                        frequencies[channel] / double(SAMPLE_RATE);

                    if (phases[channel] >= 1.0)
                        phases[channel] -=
                            std::floor(phases[channel]);
                }

                sample = std::clamp(sample, -1.0, 1.0);

                *output++ = static_cast<qint16>(sample * 32767.0);
            }
        }

        return frameCount * bytesPerFrame;
    }

    qint64 writeData(const char *, qint64) override
    {
        return -1;
    }

private:
    QMutex mutex;
    std::array<double, CHANNEL_COUNT> frequencies;
    std::array<double, CHANNEL_COUNT> volumes;
    std::array<bool, CHANNEL_COUNT> enabled;
    std::array<double, CHANNEL_COUNT> phases;
};

class MainWindow final : public QWidget
{
public:
    MainWindow()
    {
        setWindowTitle("AudioTone Studio");
        resize(950, 620);

        auto *root = new QVBoxLayout(this);

        auto *title = new QLabel(
            "<h1>AudioTone Studio</h1>"
            "<p>Générateur de tonalités audio multicanal</p>");
        title->setTextFormat(Qt::RichText);
        root->addWidget(title);

        auto *deviceGroup = new QGroupBox("Sortie audio");
        auto *deviceLayout = new QHBoxLayout(deviceGroup);

        outputType = new QComboBox;
        outputType->addItem("Périphérique physique");
        outputType->addItem("Périphérique virtuel");

        outputDevice = new QComboBox;

        deviceLayout->addWidget(new QLabel("Type :"));
        deviceLayout->addWidget(outputType);
        deviceLayout->addWidget(new QLabel("Périphérique :"));
        deviceLayout->addWidget(outputDevice, 1);

        root->addWidget(deviceGroup);

        auto *help = new QLabel(
            "La sortie virtuelle nécessite un pilote audio virtuel "
            "compatible. Le programme ne crée pas de pilote.");
        help->setWordWrap(true);
        root->addWidget(help);

        auto *channelGroup = new QGroupBox("Réglages des canaux");
        auto *channelLayout = new QVBoxLayout(channelGroup);

        for (int i = 0; i < CHANNEL_COUNT; ++i) {
            auto *line = new QHBoxLayout;

            active[i] = new QCheckBox(CHANNEL_NAMES[i]);
            active[i]->setChecked(i != 5);
            active[i]->setMinimumWidth(135);

            frequency[i] = new QSpinBox;
            frequency[i]->setRange(1, 20000);
            frequency[i]->setValue(440);
            frequency[i]->setSuffix(" Hz");
            frequency[i]->setMinimumWidth(115);

            volume[i] = new QSlider(Qt::Horizontal);
            volume[i]->setRange(0, 100);
            volume[i]->setValue(25);
            volume[i]->setMinimumWidth(100);

            volumeLabel[i] = new QLabel("25 %");
            volumeLabel[i]->setMinimumWidth(45);

            line->addWidget(active[i]);
            line->addWidget(new QLabel("Fréquence :"));
            line->addWidget(frequency[i]);
            line->addWidget(new QLabel("Volume :"));
            line->addWidget(volume[i], 1);
            line->addWidget(volumeLabel[i]);

            channelLayout->addLayout(line);

            connect(frequency[i],
                    &QSpinBox::valueChanged,
                    this, [this, i](int) {
                updateChannel(i);
            });

            connect(volume[i],
                    &QSlider::valueChanged,
                    this, [this, i](int value) {
                volumeLabel[i]->setText(
                    QString("%1 %").arg(value));
                updateChannel(i);
            });

            connect(active[i],
                    &QCheckBox::toggled,
                    this, [this, i](bool) {
                updateChannel(i);
            });
        }

        root->addWidget(channelGroup);

        auto *buttons = new QHBoxLayout;

        auto *syncButton =
            new QPushButton("Synchroniser les fréquences");
        auto *startButton =
            new QPushButton("▶ Démarrer");
        auto *stopButton =
            new QPushButton("■ Arrêter");
        auto *fullscreenButton =
            new QPushButton("Plein écran");

        buttons->addWidget(syncButton);
        buttons->addStretch();
        buttons->addWidget(startButton);
        buttons->addWidget(stopButton);
        buttons->addWidget(fullscreenButton);

        root->addLayout(buttons);

        status = new QLabel("Prêt.");
        root->addWidget(status);

        connect(outputType,
                &QComboBox::currentIndexChanged,
                this, [this](int) {
            refreshDevices();
        });

        connect(syncButton, &QPushButton::clicked,
                this, [this]() {
            const int hz = frequency[0]->value();

            for (int i = 1; i < CHANNEL_COUNT; ++i)
                frequency[i]->setValue(hz);

            generator.synchronize(hz);

            status->setText(
                QString("Fréquences synchronisées : %1 Hz").arg(hz));
        });

        connect(startButton, &QPushButton::clicked,
                this, [this]() {
            startAudio();
        });

        connect(stopButton, &QPushButton::clicked,
                this, [this]() {
            stopAudio();
        });

        connect(fullscreenButton, &QPushButton::clicked,
                this, [this]() {
            if (isFullScreen())
                showNormal();
            else
                showFullScreen();
        });

        refreshDevices();

        for (int i = 0; i < CHANNEL_COUNT; ++i)
            updateChannel(i);
    }

    ~MainWindow() override
    {
        stopAudio();
    }

private:
    static bool looksVirtual(const QString &name)
    {
        const QString n = name.toLower();

        return n.contains("virtual")
            || n.contains("voicemeeter")
            || n.contains("vb-audio")
            || n.contains("cable")
            || n.contains("loopback");
    }

    void refreshDevices()
    {
        outputDevice->clear();

        const bool wantVirtual = outputType->currentIndex() == 1;
        const auto devices = QMediaDevices::audioOutputs();

        for (const QAudioDevice &device : devices) {
            const bool virtualDevice =
                looksVirtual(device.description());

            if (virtualDevice == wantVirtual) {
                outputDevice->addItem(
                    device.description(),
                    QString::fromLatin1(device.id()));
            }
        }

        if (outputDevice->count() == 0) {
            outputDevice->addItem(
                wantVirtual
                    ? "Aucun périphérique virtuel détecté"
                    : "Aucun périphérique physique détecté",
                QString());
        }
    }

    void updateChannel(int i)
    {
        generator.setChannel(
            i,
            frequency[i]->value(),
            volume[i]->value() / 100.0,
            active[i]->isChecked());
    }

    void startAudio()
    {
        stopAudio();

        const QString selectedId =
            outputDevice->currentData().toString();

        if (selectedId.isEmpty()) {
            QMessageBox::warning(
                this, "Sortie indisponible",
                "Aucun périphérique correspondant n'a été détecté.");
            return;
        }

        QAudioDevice selected;
        bool found = false;

        for (const QAudioDevice &device : QMediaDevices::audioOutputs()) {
            if (QString::fromLatin1(device.id()) == selectedId) {
                selected = device;
                found = true;
                break;
            }
        }

        if (!found) {
            QMessageBox::warning(
                this, "Périphérique introuvable",
                "Le périphérique sélectionné n'est plus disponible.");
            refreshDevices();
            return;
        }

        QAudioFormat format;
        format.setSampleRate(SAMPLE_RATE);
        format.setChannelCount(CHANNEL_COUNT);
        format.setChannelConfig(
            QAudioFormat::ChannelConfigSurround5Dot1);
        format.setSampleFormat(QAudioFormat::Int16);

        if (!selected.isFormatSupported(format)) {
            QMessageBox::warning(
                this, "Format non pris en charge",
                "Ce périphérique ne prend pas en charge la sortie "
                "PCM 16 bits, 48 kHz, 5.1 canaux. "
                "Choisis une sortie compatible.");
            return;
        }

        for (int i = 0; i < CHANNEL_COUNT; ++i)
            updateChannel(i);

        sink = new QAudioSink(selected, format, this);

        sink->start(&generator);

        if (sink->error() != QAudio::NoError) {
            QMessageBox::warning(
                this, "Erreur audio",
                "Impossible de démarrer la sortie audio.");
            stopAudio();
            return;
        }

        status->setText(
            "Lecture en cours — PCM 16 bits, 48 kHz, 5.1.");
    }

    void stopAudio()
    {
        if (sink) {
            sink->stop();
            delete sink;
            sink = nullptr;
        }

        status->setText("Lecture arrêtée.");
    }

    QComboBox *outputType = nullptr;
    QComboBox *outputDevice = nullptr;

    std::array<QCheckBox *, CHANNEL_COUNT> active{};
    std::array<QSpinBox *, CHANNEL_COUNT> frequency{};
    std::array<QSlider *, CHANNEL_COUNT> volume{};
    std::array<QLabel *, CHANNEL_COUNT> volumeLabel{};

    QLabel *status = nullptr;

    ToneGenerator generator;
    QAudioSink *sink = nullptr;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    MainWindow window;
    window.show();

    return app.exec();
}
