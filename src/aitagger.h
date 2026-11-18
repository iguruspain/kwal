#pragma once

#include <KConfigGroup>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QThreadPool>
#include <QtQml/qqmlregistration.h>

class QNetworkAccessManager;

namespace aitagger {
// Defensive cap: model responses drift into prose; anything past this is dropped.
inline constexpr int kMaxTags = 10;
QString defaultSystemPrompt();
QString normalizeBase(const QString &endpoint);
QString buildUserMessage(const QString &path, const QString &type);
QByteArray resizedJpeg(const QString &path, int maxSide, int quality);
// OpenAI chat content: parts array with a base64 data URI when image data
// is present, plain string otherwise.
QJsonValue buildUserContent(const QString &path, const QString &type, const QByteArray &imageData, const QString &mimeType);
// Vision-capable ids from a /v1/models response: input modalities under
// llama.cpp "architecture" or OpenAI "capabilities"; none reported = keep.
QStringList parseModels(const QJsonDocument &doc);
// Accepts a JSON array (possibly embedded in prose) or comma/semicolon/newline text.
QStringList parseTags(const QString &content);
} // namespace aitagger

// Tags via an OpenAI-compatible chat API; settings persist in kwalrc's
// [Wallpapers] group. Chat requests run strictly one at a time.
class AiTagger : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by AppController")
    Q_PROPERTY(QString endpoint READ endpoint WRITE setEndpoint NOTIFY endpointChanged)
    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(QString systemPrompt READ systemPrompt WRITE setSystemPrompt NOTIFY systemPromptChanged)
    Q_PROPERTY(QString defaultSystemPrompt READ defaultSystemPrompt CONSTANT)
    Q_PROPERTY(QString folder READ folder WRITE setFolder NOTIFY folderChanged)
    Q_PROPERTY(QStringList models READ models NOTIFY modelsChanged)
    Q_PROPERTY(bool modelsLoading READ modelsLoading NOTIFY modelsLoadingChanged)
    Q_PROPERTY(QString modelsError READ modelsError NOTIFY modelsChanged)
    Q_PROPERTY(bool tagging READ tagging NOTIFY taggingChanged)
    Q_PROPERTY(int tagTotal READ tagTotal NOTIFY taggingChanged)
    Q_PROPERTY(int tagCompleted READ tagCompleted NOTIFY taggingChanged)

public:
    explicit AiTagger(const KConfigGroup &config, QObject *parent = nullptr);

    QString endpoint() const;
    void setEndpoint(const QString &endpoint);
    QString model() const;
    void setModel(const QString &model);
    QString systemPrompt() const;
    void setSystemPrompt(const QString &prompt);
    QString defaultSystemPrompt() const;
    QString folder() const;
    void setFolder(const QString &folder);
    QStringList models() const;
    bool modelsLoading() const;
    QString modelsError() const;
    bool tagging() const;
    int tagTotal() const;
    int tagCompleted() const;

    Q_INVOKABLE void fetchModels();
    Q_INVOKABLE void tagFolder(const QString &folder);
    Q_INVOKABLE void tagWallpaper(const QString &path);

signals:
    void endpointChanged();
    void modelChanged();
    void systemPromptChanged();
    void folderChanged();
    void modelsChanged();
    void modelsLoadingChanged();
    void taggingChanged();
    void wallpaperTagged(const QString &path, bool ok, const QString &message, const QStringList &tags);
    void taggingFinished(int succeeded, int failed);
    void error(const QString &message);

private:
    // Pool job, nested for private sendRequest access; image resolve/resize runs
    // off the GUI thread, the finished body is queued back.
    class RequestJob;
    void startNext();
    // Runs on the owning thread (queued in from the pool job).
    void sendRequest(const QString &path, const QByteArray &body);

    QNetworkAccessManager *m_nam = nullptr;
    KConfigGroup m_config;
    QString m_endpoint;
    QString m_model;
    QString m_systemPrompt;
    QString m_folder;
    QStringList m_models;
    bool m_modelsLoading = false;
    QString m_modelsError;
    bool m_tagging = false;
    int m_total = 0;
    int m_completed = 0;
    int m_succeeded = 0;
    int m_failed = 0;
    bool m_batch = false;
    QStringList m_queue;
    QThreadPool m_pool;
};
