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

// Pure helpers of the AI tag pipeline; free of app state so the unit
// tests exercise them without network or QML.
namespace aitagger {
// Tags a single model response may contribute; anything beyond the cap is
// dropped (defensive against runaway prose).
inline constexpr int kMaxTags = 10;
// Default prompt: 3-5 lowercase comma-separated search tags describing the
// wallpaper image; used while the stored prompt is empty.
QString defaultSystemPrompt();
// Normalizes a user-typed OpenAI-compatible endpoint to a base URL that
// ends in "/v1" (a scheme-less "host:port" gets "http://").
QString normalizeBase(const QString &endpoint);
// The text part of the user message for one wallpaper: file name and media
// type.
QString buildUserMessage(const QString &path, const QString &type);
// Decodes <path>, resizes it to at most <maxSide> px on the longest side
// (no upscale) and returns JPEG bytes; empty when the file cannot be
// decoded.
QByteArray resizedJpeg(const QString &path, int maxSide, int quality);
// The user message content for the chat request: the text part plus an
// OpenAI-style "image_url" part (base64 data URI) when the image data is
// available, a plain string otherwise.
QJsonValue buildUserContent(const QString &path, const QString &type, const QByteArray &imageData, const QString &mimeType);
// The vision-capable model ids of a /v1/models response: models whose
// reported input modalities (llama.cpp "architecture", OpenAI
// "capabilities") include "image"; servers that report no modalities keep
// every model.
QStringList parseModels(const QJsonDocument &doc);
// Extracts the tag list from a model response: a JSON array, an array
// embedded in prose, or plain comma/semicolon/newline-separated text.
QStringList parseTags(const QString &content);
} // namespace aitagger

// AI tag generation against an OpenAI-compatible chat API (llama-server,
// vLLM, ...). The endpoint, model, system prompt and last tagged folder
// persist in the [Wallpapers] group of kwalrc. Chat requests run one at a
// time, wallpaper by wallpaper.
class AiTagger : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by AppController")
    Q_PROPERTY(QString endpoint READ endpoint WRITE setEndpoint NOTIFY endpointChanged)
    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(QString systemPrompt READ systemPrompt WRITE setSystemPrompt NOTIFY systemPromptChanged)
    // Prompt used while the stored one is empty; editable in the settings.
    Q_PROPERTY(QString defaultSystemPrompt READ defaultSystemPrompt CONSTANT)
    Q_PROPERTY(QString folder READ folder WRITE setFolder NOTIFY folderChanged)
    // Model ids reported by the server's /v1/models endpoint.
    Q_PROPERTY(QStringList models READ models NOTIFY modelsChanged)
    Q_PROPERTY(bool modelsLoading READ modelsLoading NOTIFY modelsLoadingChanged)
    Q_PROPERTY(QString modelsError READ modelsError NOTIFY modelsChanged)
    // "Tag wallpapers" batch progress.
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

    // GETs {endpoint}/v1/models and fills <models>.
    Q_INVOKABLE void fetchModels();
    // Tags every wallpaper (image and video) in <folder>, sequentially.
    Q_INVOKABLE void tagFolder(const QString &folder);
    // Tags one wallpaper (the drawer's refresh button).
    Q_INVOKABLE void tagWallpaper(const QString &path);

signals:
    void endpointChanged();
    void modelChanged();
    void systemPromptChanged();
    void folderChanged();
    void modelsChanged();
    void modelsLoadingChanged();
    void taggingChanged();
    // Result for one wallpaper; <tags> is the list stored on success.
    void wallpaperTagged(const QString &path, bool ok, const QString &message, const QStringList &tags);
    // Emitted when a whole tagFolder batch finishes.
    void taggingFinished(int succeeded, int failed);
    // An operation could not start (missing endpoint/model, empty folder).
    void error(const QString &message);

private:
    // Pool job; nested so it can call the private sendRequest. It resolves
    // the image (videos: the cached first frame), resizes it off the GUI
    // thread and queues the finished JSON body back.
    class RequestJob;
    void startNext();
    // POSTs the prepared request body; runs on the owning thread.
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
