#include "aitagger.h"
#include "wallpapermodel.h"
#include "wallpapersinfo.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QRunnable>
#include <QSet>
#include <QThreadPool>
#include <QUrl>

namespace aitagger {

QString defaultSystemPrompt()
{
    return QStringLiteral(
        "You are a desktop wallpaper tagging assistant. The user message "
        "contains the file name of one wallpaper (an image or a video) and, "
        "when available, the wallpaper image itself. Suggest 3 to 5 short "
        "search tags that describe what the wallpaper shows: the main "
        "subject, the setting or scene, and the style or medium (for "
        "example photo, anime, digital art or abstract). Do not use color "
        "names as tags. Respond with the tags only: lowercase, separated by "
        "commas, no quotes, no numbering, no other text.");
}

QString normalizeBase(const QString &endpoint)
{
    QString base = endpoint.trimmed();
    if (base.isEmpty()) {
        return {};
    }
    if (!base.contains(QLatin1String("://"))) {
        base.prepend(QStringLiteral("http://"));
    }
    while (base.endsWith(QLatin1Char('/'))) {
        base.chop(1);
    }
    if (!base.endsWith(QLatin1String("/v1"), Qt::CaseInsensitive)) {
        base += QStringLiteral("/v1");
    }
    return base;
}

QString buildUserMessage(const QString &path, const QString &type)
{
    return QStringLiteral("Wallpaper file name: %1\nMedia type: %2")
        .arg(QFileInfo(path).fileName(), type);
}

QByteArray resizedJpeg(const QString &path, int maxSide, int quality)
{
    QImageReader reader(path);
    const QSize sourceSize = reader.size();
    if (!sourceSize.isValid()) {
        return {};
    }
    if (sourceSize.width() > maxSide || sourceSize.height() > maxSide) {
        reader.setScaledSize(sourceSize.scaled(maxSide, maxSide, Qt::KeepAspectRatio));
    }
    const QImage image = reader.read();
    if (image.isNull()) {
        return {};
    }
    QByteArray buffer;
    QBuffer byteBuffer(&buffer);
    byteBuffer.open(QIODevice::WriteOnly);
    if (!image.save(&byteBuffer, "JPG", quality)) {
        return {};
    }
    return buffer;
}

QJsonValue buildUserContent(const QString &path, const QString &type, const QByteArray &imageData, const QString &mimeType)
{
    const QString text = buildUserMessage(path, type);
    if (imageData.isEmpty()) {
        return text;
    }
    const QString dataUri = QStringLiteral("data:%1;base64,%2")
        .arg(mimeType, QString::fromLatin1(imageData.toBase64()));
    QJsonArray parts;
    parts.append(QJsonObject{
        {QStringLiteral("type"), QStringLiteral("text")},
        {QStringLiteral("text"), text},
    });
    parts.append(QJsonObject{
        {QStringLiteral("type"), QStringLiteral("image_url")},
        {QStringLiteral("image_url"), QJsonObject{{QStringLiteral("url"), dataUri}}},
    });
    return parts;
}

QStringList parseModels(const QJsonDocument &doc)
{
    QStringList ids;
    for (const QJsonValue &value : doc.object().value(QStringLiteral("data")).toArray()) {
        const QJsonObject model = value.toObject();
        QJsonArray input;
        const QJsonObject architecture = model.value(QStringLiteral("architecture")).toObject();
        if (architecture.contains(QStringLiteral("input_modalities"))) {
            input = architecture.value(QStringLiteral("input_modalities")).toArray();
        } else {
            const QJsonObject capabilities = model.value(QStringLiteral("capabilities")).toObject();
            if (capabilities.contains(QStringLiteral("input_modalities"))) {
                input = capabilities.value(QStringLiteral("input_modalities")).toArray();
            }
        }
        if (input.isEmpty()) {
            // The server does not report modalities; the model might still
            // be vision-capable, so keep it.
            ids.append(model.value(QStringLiteral("id")).toString());
            continue;
        }
        bool acceptsImage = false;
        for (const QJsonValue &modality : input) {
            if (modality.toString() == QLatin1String("image")) {
                acceptsImage = true;
                break;
            }
        }
        if (acceptsImage) {
            ids.append(model.value(QStringLiteral("id")).toString());
        }
    }
    ids.removeAll(QString());
    ids.removeDuplicates();
    return ids;
}

namespace {
// Strips surrounding quotes, leftover brackets from broken JSON, a leading
// bullet dash and a trailing dot that models like to add around tags.
QString cleanTag(QString tag)
{
    tag = tag.trimmed();
    for (int i = 0; i < 2 && tag.size() >= 2; ++i) {
        const QChar first = tag.at(0);
        const QChar last = tag.at(tag.size() - 1);
        if (first == last && (first == QLatin1Char('"') || first == QLatin1Char('\'') || first == QLatin1Char('`'))) {
            tag = tag.mid(1, tag.size() - 2).trimmed();
        } else {
            break;
        }
    }
    while (tag.startsWith(QLatin1Char('[')) || tag.endsWith(QLatin1Char(']'))) {
        if (tag.startsWith(QLatin1Char('['))) {
            tag.remove(0, 1);
        }
        if (tag.endsWith(QLatin1Char(']'))) {
            tag.chop(1);
        }
        tag = tag.trimmed();
    }
    if (tag.startsWith(QLatin1Char('-'))) {
        tag = tag.mid(1).trimmed();
    }
    if (tag.endsWith(QLatin1Char('.'))) {
        tag.chop(1);
    }
    return tag.trimmed();
}
} // namespace

QStringList parseTags(const QString &content)
{
    const QString text = content.trimmed();
    if (text.isEmpty()) {
        return {};
    }
    QStringList raw;
    // Preferred shape: a JSON array, whole response or embedded in prose.
    const int open = text.indexOf(QLatin1Char('['));
    const int close = text.lastIndexOf(QLatin1Char(']'));
    if (open >= 0 && close > open) {
        const QJsonDocument doc = QJsonDocument::fromJson(text.mid(open, close - open + 1).toUtf8());
        if (doc.isArray()) {
            for (const QJsonValue &value : doc.array()) {
                if (value.isString()) {
                    raw.append(value.toString());
                } else if (value.isDouble()) {
                    raw.append(QString::number(value.toDouble()));
                }
            }
        }
    }
    if (raw.isEmpty()) {
        // Fallback: plain comma/semicolon/newline-separated text.
        const QStringView view = text;
        for (const QStringView token : view.split(QRegularExpression(QStringLiteral("[,;\\n]")))) {
            raw.append(token.toString());
        }
    }
    QStringList tags;
    QSet<QString> seen;
    for (const QString &token : raw) {
        const QString tag = cleanTag(token);
        if (tag.isEmpty() || tag.size() > 40) {
            continue;
        }
        if (seen.contains(tag.toLower())) {
            continue;
        }
        seen.insert(tag.toLower());
        tags.append(tag);
        if (tags.size() >= kMaxTags) {
            break;
        }
    }
    return tags;
}

} // namespace aitagger

namespace {
// Longest side of the image sent to the model. Vision encoders resize the
// input to roughly this range anyway, so the original 4K bytes (and the
// multi-megabyte JSON body) buy nothing.
constexpr int kMaxRequestSide = 1024;
constexpr int kJpegQuality = 85;
} // namespace

// ---------------------------------------------------------------------------
// AiTagger

class AiTagger::RequestJob : public QRunnable
{
public:
    RequestJob(AiTagger *tagger, const QString &path, const QString &model, const QString &systemPrompt)
        : m_tagger(tagger)
        , m_path(path)
        , m_model(model)
        , m_systemPrompt(systemPrompt)
    {
        setAutoDelete(true);
    }

    void run() override
    {
        // Video wallpapers are tagged from the cached first frame.
        const bool video = WallpaperModel::isVideo(m_path);
        QString source = m_path;
        if (video && WallpapersInfo::instance() != nullptr) {
            source = WallpapersInfo::instance()->firstFrameFor(m_path);
        }
        QByteArray imageData = aitagger::resizedJpeg(source, kMaxRequestSide, kJpegQuality);
        QString mimeType = QStringLiteral("image/jpeg");
        if (imageData.isEmpty() && !source.isEmpty()) {
            // Undecodable source format: fall back to the raw file.
            QFile file(source);
            if (file.open(QIODevice::ReadOnly)) {
                imageData = file.readAll();
                mimeType = QMimeDatabase().mimeTypeForFile(QFileInfo(source)).name();
            }
        }
        const QJsonObject body = {
            {QStringLiteral("model"), m_model},
            {QStringLiteral("messages"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("role"), QStringLiteral("system")},
                     {QStringLiteral("content"),
                      m_systemPrompt.isEmpty() ? aitagger::defaultSystemPrompt() : m_systemPrompt},
                 },
                 QJsonObject{
                     {QStringLiteral("role"), QStringLiteral("user")},
                     {QStringLiteral("content"),
                      aitagger::buildUserContent(
                          m_path,
                          video ? QStringLiteral("video") : QStringLiteral("image"),
                          imageData,
                          mimeType)},
                 },
             }},
        };
        const QByteArray bytes = QJsonDocument(body).toJson(QJsonDocument::Compact);
        // The job may be deleted before this runs; the lambda owns its data.
        AiTagger *tagger = m_tagger;
        const QString path = m_path;
        QMetaObject::invokeMethod(
            tagger,
            [tagger, path, bytes]() {
                tagger->sendRequest(path, bytes);
            },
            Qt::QueuedConnection);
    }

private:
    AiTagger *m_tagger;
    QString m_path;
    QString m_model;
    QString m_systemPrompt;
};

AiTagger::AiTagger(const KConfigGroup &config, QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_config(config)
    , m_endpoint(config.readEntry("aiEndpoint", QString()))
    , m_model(config.readEntry("aiModel", QString()))
    , m_systemPrompt(config.readEntry("aiSystemPrompt", QString()))
    , m_folder(config.readEntry("aiFolder", QString()))
{
}

QString AiTagger::endpoint() const
{
    return m_endpoint;
}

void AiTagger::setEndpoint(const QString &endpoint)
{
    if (m_endpoint == endpoint) {
        return;
    }
    m_endpoint = endpoint;
    m_config.writeEntry("aiEndpoint", m_endpoint);
    emit endpointChanged();
}

QString AiTagger::model() const
{
    return m_model;
}

void AiTagger::setModel(const QString &model)
{
    if (m_model == model) {
        return;
    }
    m_model = model;
    m_config.writeEntry("aiModel", m_model);
    emit modelChanged();
}

QString AiTagger::systemPrompt() const
{
    return m_systemPrompt;
}

void AiTagger::setSystemPrompt(const QString &prompt)
{
    if (m_systemPrompt == prompt) {
        return;
    }
    m_systemPrompt = prompt;
    m_config.writeEntry("aiSystemPrompt", m_systemPrompt);
    emit systemPromptChanged();
}

QString AiTagger::defaultSystemPrompt() const
{
    return aitagger::defaultSystemPrompt();
}

QString AiTagger::folder() const
{
    return m_folder;
}

void AiTagger::setFolder(const QString &folder)
{
    if (m_folder == folder) {
        return;
    }
    m_folder = folder;
    m_config.writeEntry("aiFolder", m_folder);
    emit folderChanged();
}

QStringList AiTagger::models() const
{
    return m_models;
}

bool AiTagger::modelsLoading() const
{
    return m_modelsLoading;
}

QString AiTagger::modelsError() const
{
    return m_modelsError;
}

bool AiTagger::tagging() const
{
    return m_tagging;
}

int AiTagger::tagTotal() const
{
    return m_total;
}

int AiTagger::tagCompleted() const
{
    return m_completed;
}

void AiTagger::fetchModels()
{
    const QString base = aitagger::normalizeBase(m_endpoint);
    if (base.isEmpty()) {
        m_modelsError = QStringLiteral("Set the API endpoint first.");
        emit modelsChanged();
        return;
    }
    m_modelsLoading = true;
    m_modelsError.clear();
    emit modelsLoadingChanged();
    emit modelsChanged();

    QNetworkRequest request(QUrl(base + QStringLiteral("/models")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        m_modelsLoading = false;
        QStringList names;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() == QNetworkReply::NoError && status >= 200 && status < 300) {
            // Only models accepting image input are listed (servers that do
            // not report modalities keep every model).
            names = aitagger::parseModels(QJsonDocument::fromJson(reply->readAll()));
            if (names.isEmpty()) {
                m_modelsError = QStringLiteral("No vision-capable models found on this endpoint.");
            }
        } else {
            m_modelsError = reply->error() == QNetworkReply::TimeoutError
                ? QStringLiteral("The models request timed out.")
                : QStringLiteral("Could not list models: %1").arg(reply->errorString());
        }
        m_models = std::move(names);
        emit modelsLoadingChanged();
        emit modelsChanged();
    });
}

void AiTagger::tagFolder(const QString &folder)
{
    if (m_tagging) {
        return;
    }
    if (aitagger::normalizeBase(m_endpoint).isEmpty() || m_model.isEmpty()) {
        emit error(QStringLiteral("Set the API endpoint and model in Settings first."));
        return;
    }
    const QFileInfoList entries =
        QDir(folder).entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    QStringList files;
    for (const QFileInfo &info : entries) {
        if (WallpaperModel::isVideo(info.filePath()) || WallpaperModel::isImage(info.filePath())) {
            files.append(info.filePath());
        }
    }
    if (files.isEmpty()) {
        emit error(QStringLiteral("No wallpapers found in the selected folder."));
        return;
    }
    m_batch = true;
    m_total = files.size();
    m_completed = 0;
    m_succeeded = 0;
    m_failed = 0;
    m_queue = files;
    emit taggingChanged();
    startNext();
}

void AiTagger::tagWallpaper(const QString &path)
{
    if (m_tagging || path.isEmpty()) {
        return;
    }
    if (aitagger::normalizeBase(m_endpoint).isEmpty() || m_model.isEmpty()) {
        emit error(QStringLiteral("Set the API endpoint and model in Settings first."));
        return;
    }
    m_batch = false;
    m_total = 1;
    m_completed = 0;
    m_succeeded = 0;
    m_failed = 0;
    m_queue = {path};
    emit taggingChanged();
    startNext();
}

void AiTagger::startNext()
{
    if (m_queue.isEmpty()) {
        m_tagging = false;
        emit taggingChanged();
        if (m_batch) {
            emit taggingFinished(m_succeeded, m_failed);
        }
        return;
    }
    const QString path = m_queue.takeFirst();
    m_tagging = true;
    emit taggingChanged();
    // Image resolution, resize and body building run off the GUI thread;
    // only the (small) reply is handled here.
    m_pool.start(new RequestJob(this, path, m_model, m_systemPrompt));
}

void AiTagger::sendRequest(const QString &path, const QByteArray &body)
{
    const QString base = aitagger::normalizeBase(m_endpoint);
    QNetworkRequest request(QUrl(base + QStringLiteral("/chat/completions")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    // Local models can be slow on CPU; the transfer timeout is the
    // inactivity window between response bytes (default is 30 s).
    request.setTransferTimeout(600000);
    QNetworkReply *reply = m_nam->post(request, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply, path]() {
        reply->deleteLater();
        QStringList tags;
        QString message;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() == QNetworkReply::NoError && status >= 200 && status < 300) {
            const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            const QJsonArray choices = doc.object().value(QStringLiteral("choices")).toArray();
            const QString content = choices.isEmpty()
                ? QString()
                : choices.first().toObject().value(QStringLiteral("message"))
                      .toObject()
                      .value(QStringLiteral("content"))
                      .toString();
            tags = aitagger::parseTags(content);
            if (tags.isEmpty()) {
                message = QStringLiteral("The model returned no usable tags.");
            } else if (WallpapersInfo::instance() == nullptr) {
                message = QStringLiteral("The wallpaper index is unavailable.");
            } else {
                // Merged into the existing tags (duplicates dropped); the
                // manual flag keeps them across folder rescans.
                WallpapersInfo::instance()->mergeTags(path, tags);
            }
        } else {
            const QString body = QString::fromUtf8(reply->readAll()).trimmed();
            message = reply->error() == QNetworkReply::TimeoutError
                ? QStringLiteral("The model request timed out.")
                : status > 0
                      ? QStringLiteral("HTTP %1: %2").arg(status).arg(body.left(160))
                      : QStringLiteral("Request failed: %1").arg(reply->errorString());
        }
        const bool ok = !tags.isEmpty();
        m_completed++;
        if (ok) {
            m_succeeded++;
        } else {
            m_failed++;
        }
        emit wallpaperTagged(path, ok, message, tags);
        emit taggingChanged();
        startNext();
    });
}
