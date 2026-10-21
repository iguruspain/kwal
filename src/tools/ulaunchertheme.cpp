#include "tools/ulaunchertheme.h"
#include "tools/fileutil.h"
#include "templateinstaller.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryFile>

namespace tools
{

QString bundledTemplateDir()
{
    return templateDir() + QStringLiteral("/ulauncher/theme_template");
}

QString userThemesDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/ulauncher/user-themes");
}

QStringList listThemeDirs(const QString &root)
{
    QStringList dirs;
    QDir dir(root);
    if (!dir.exists())
        return dirs;
    const QFileInfoList info = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &fi : info)
        if (QFileInfo::exists(fi.filePath() + QStringLiteral("/manifest.json")))
            dirs.append(fi.filePath());
    return dirs;
}

QString stripCssComments(const QString &cssContent)
{
    QString out = cssContent;
    return out.replace(
        QRegularExpression(QStringLiteral("/\\*.*?\\*/"),
                          QRegularExpression::DotMatchesEverythingOption),
        QString());
}

QStringList cssColorOrder(const QString &cssContent)
{
    QStringList names;
    QRegularExpression re(QStringLiteral("@define-color\\s+([\\w-]+)\\s+([^;]+);"));
    QRegularExpressionMatchIterator it = re.globalMatch(stripCssComments(cssContent));
    while (it.hasNext())
        names.append(it.next().captured(1));
    return names;
}

ThemeData parseTheme(const QString &themePath)
{
    ThemeData data;
    QDir dir(themePath);
    if (!dir.exists())
        return data;

    // Read manifest.json
    QFile manifestFile(dir.filePath(QStringLiteral("manifest.json")));
    if (manifestFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        data.manifestJson = QString::fromUtf8(manifestFile.readAll());
        manifestFile.close();

        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(data.manifestJson.toUtf8(), &error);
        if (!error.error && doc.isObject()) {
            QJsonObject obj = doc.object();
            // Extract color-related keys from manifest
            auto extractColors = [&](const QJsonValue &val, const QString &prefix) {
                if (val.isObject()) {
                    QJsonObject colorObj = val.toObject();
                    for (auto it = colorObj.begin(); it != colorObj.end(); ++it) {
                        data.manifestColors[prefix + it.key()] = it.value().toString();
                    }
                }
            };

            // Root level colors
            if (obj.contains(QStringLiteral("when_selected")))
                data.manifestColors[QStringLiteral("when_selected")] = obj[QStringLiteral("when_selected")].toString();
            if (obj.contains(QStringLiteral("when_not_selected")))
                data.manifestColors[QStringLiteral("when_not_selected")] = obj[QStringLiteral("when_not_selected")].toString();

            // colors object
            extractColors(obj[QStringLiteral("colors")], QStringLiteral("colors_"));

            // matched_text_hl_colors object
            extractColors(obj[QStringLiteral("matched_text_hl_colors")], QStringLiteral("matched_text_hl_colors_"));
        }
    }

    // Read theme.css
    QFile cssFile(dir.filePath(QStringLiteral("theme.css")));
    if (cssFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        data.cssContent = QString::fromUtf8(cssFile.readAll());
        cssFile.close();
        data.themeColors = extractCssColors(data.cssContent);
    }

    data.valid = !data.manifestJson.isEmpty() && !data.cssContent.isEmpty();
    return data;
}

ColorMap extractCssColors(const QString &cssContent)
{
    ColorMap colors;
    // Match @define-color name value;
    QRegularExpression re(QStringLiteral("@define-color\\s+([\\w-]+)\\s+([^;]+);"));
    QRegularExpressionMatchIterator it = re.globalMatch(stripCssComments(cssContent));
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString name = match.captured(1);
        QString value = match.captured(2).trimmed();
        colors[name] = value;
    }
    return colors;
}

ColorMap parseManifestColors(const QString &manifestJson)
{
    ColorMap colors;
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(manifestJson.toUtf8(), &error);
    if (error.error || !doc.isObject())
        return colors;

    QJsonObject obj = doc.object();

    if (obj.contains(QStringLiteral("when_selected")))
        colors[QStringLiteral("when_selected")] = obj[QStringLiteral("when_selected")].toString();
    if (obj.contains(QStringLiteral("when_not_selected")))
        colors[QStringLiteral("when_not_selected")] = obj[QStringLiteral("when_not_selected")].toString();

    auto extractColors = [&](const QJsonValue &val, const QString &prefix) {
        if (val.isObject()) {
            QJsonObject colorObj = val.toObject();
            for (auto it = colorObj.begin(); it != colorObj.end(); ++it) {
                colors[prefix + it.key()] = it.value().toString();
            }
        }
    };

    extractColors(obj[QStringLiteral("colors")], QStringLiteral("colors_"));
    extractColors(obj[QStringLiteral("matched_text_hl_colors")], QStringLiteral("matched_text_hl_colors_"));

    return colors;
}

QString replaceCssColors(const QString &cssContent, const ColorMap &newColors)
{
    QString result = cssContent;
    // Replace each @define-color entry
    for (auto it = newColors.constBegin(); it != newColors.constEnd(); ++it) {
        const QString &name = it.key();
        const QString &value = it.value();

        // Escape for regex
        QString escapedName = QRegularExpression::escape(name);
        QRegularExpression re(QStringLiteral("@define-color\\s+") + escapedName + QStringLiteral("\\s+[^;]+;"));
        result.replace(re, QStringLiteral("@define-color %1 %2;").arg(name, value));
    }
    return result;
}

QString updateManifestColors(const QString &manifestJson, const ColorMap &newColors)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(manifestJson.toUtf8(), &error);
    if (error.error || !doc.isObject())
        return QString();

    QJsonObject obj = doc.object();

    // Root level
    if (newColors.contains(QStringLiteral("when_selected")))
        obj[QStringLiteral("when_selected")] = newColors[QStringLiteral("when_selected")];
    if (newColors.contains(QStringLiteral("when_not_selected")))
        obj[QStringLiteral("when_not_selected")] = newColors[QStringLiteral("when_not_selected")];

    // colors object
    QJsonObject colorsObj;
    if (obj.contains(QStringLiteral("colors")) && obj[QStringLiteral("colors")].isObject())
        colorsObj = obj[QStringLiteral("colors")].toObject();
    for (auto it = newColors.constBegin(); it != newColors.constEnd(); ++it) {
        if (it.key().startsWith(QStringLiteral("colors_"))) {
            QString key = it.key().mid(7); // remove "colors_"
            colorsObj[key] = it.value();
        }
    }
    if (!colorsObj.isEmpty())
        obj[QStringLiteral("colors")] = colorsObj;

    // matched_text_hl_colors object
    QJsonObject hlColorsObj;
    if (obj.contains(QStringLiteral("matched_text_hl_colors")) && obj[QStringLiteral("matched_text_hl_colors")].isObject())
        hlColorsObj = obj[QStringLiteral("matched_text_hl_colors")].toObject();
    for (auto it = newColors.constBegin(); it != newColors.constEnd(); ++it) {
        if (it.key().startsWith(QStringLiteral("matched_text_hl_colors_"))) {
            QString key = it.key().mid(23); // remove "matched_text_hl_colors_"
            hlColorsObj[key] = it.value();
        }
    }
    if (!hlColorsObj.isEmpty())
        obj[QStringLiteral("matched_text_hl_colors")] = hlColorsObj;

    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Indented));
}

QString applyTheme(const QString &themePath, const ColorMap &manifestColors, const ColorMap &themeColors)
{
    QDir dir(themePath);
    if (!dir.exists())
        return QStringLiteral("Theme directory does not exist: %1").arg(themePath);

    // Read original files
    QString manifestPath = dir.filePath(QStringLiteral("manifest.json"));
    QString cssPath = dir.filePath(QStringLiteral("theme.css"));

    QString originalManifest, originalCss;
    QFile manifestFile(manifestPath);
    if (manifestFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        originalManifest = QString::fromUtf8(manifestFile.readAll());
        manifestFile.close();
    }
    QFile cssFile(cssPath);
    if (cssFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        originalCss = QString::fromUtf8(cssFile.readAll());
        cssFile.close();
    }

    // Back up the current files before overwriting them
    backupFile(manifestPath);
    backupFile(cssPath);

    // Update manifest
    QString newManifest = updateManifestColors(originalManifest, manifestColors);
    if (newManifest.isEmpty())
        return QStringLiteral("Failed to update manifest.json");

    // Update CSS
    QString newCss = replaceCssColors(originalCss, themeColors);
    if (newCss.isEmpty())
        return QStringLiteral("Failed to update theme.css");

    // Write atomically
    if (!writeFileAtomic(manifestPath, newManifest))
        return QStringLiteral("Failed to write manifest.json");

    if (!writeFileAtomic(cssPath, newCss))
        return QStringLiteral("Failed to write theme.css");

    return QString();
}

bool hasThemeBackup(const QString &themePath)
{
    QDir dir(themePath);
    if (!dir.exists())
        return false;
    return hasBackup(dir.filePath(QStringLiteral("manifest.json")))
        || hasBackup(dir.filePath(QStringLiteral("theme.css")));
}

bool restoreThemeBackup(const QString &themePath)
{
    QDir dir(themePath);
    if (!dir.exists())
        return false;

    bool restored = false;
    restored |= restoreBackup(dir.filePath(QStringLiteral("manifest.json")));
    restored |= restoreBackup(dir.filePath(QStringLiteral("theme.css")));
    return restored;
}

QString createTheme(const QString &sourcePath, const QString &newName)
{
    QDir srcDir(sourcePath);
    if (!srcDir.exists())
        return QString();

    QString destPath = userThemesDir() + QLatin1Char('/') + newName;
    QDir destDir(destPath);
    if (destDir.exists())
        return QString();

    if (!QDir().mkpath(destPath))
        return QString();

    // Copy all files except .bak
    QStringList files = srcDir.entryList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QString &file : files) {
        if (file.endsWith(QStringLiteral(".bak")))
            continue;
        QFile::copy(srcDir.filePath(file), destDir.filePath(file));
    }

    // Update manifest.json name and display_name
    QString manifestPath = destDir.filePath(QStringLiteral("manifest.json"));
    QFile mf(manifestPath);
    if (mf.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString content = QString::fromUtf8(mf.readAll());
        mf.close();

        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(content.toUtf8(), &error);
        if (!error.error && doc.isObject()) {
            QJsonObject obj = doc.object();
            obj[QStringLiteral("name")] = newName;
            obj[QStringLiteral("display_name")] = newName;
            if (writeFileAtomic(manifestPath, QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Indented)))) {
                return destPath;
            }
        }
    }

    // If manifest update failed, clean up
    QDir(destPath).removeRecursively();
    return QString();
}

bool deleteTheme(const QString &themePath, const QString &activeThemePath)
{
    QDir dir(themePath);
    if (!dir.exists())
        return false;

    // Safety: cannot delete active theme
    QFileInfo themeInfo(themePath);
    QFileInfo activeInfo(activeThemePath);
    if (activeInfo.exists() && themeInfo.absoluteFilePath() == activeInfo.absoluteFilePath())
        return false;

    // Safety: cannot delete templates (must be under user-themes)
    QString userThemes = userThemesDir();
    QFileInfo userThemesInfo(userThemes);
    if (userThemesInfo.exists() && !themeInfo.absoluteFilePath().startsWith(userThemesInfo.absoluteFilePath() + QLatin1Char('/')))
        return false;

    return dir.removeRecursively();
}

// Find the theme directory under root whose manifest name or directory
// name matches themeName. Manifest names win over directory names.
static QString findThemeInRoot(const QString &root, const QString &themeName)
{
    QDir dir(root);
    if (!dir.exists())
        return QString();
    const QFileInfoList info = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    for (const QFileInfo &fi : info) {
        QFile manifestFile(fi.filePath() + QStringLiteral("/manifest.json"));
        if (!manifestFile.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(manifestFile.readAll(), &error);
        manifestFile.close();
        if (!error.error && doc.isObject()
            && doc.object()[QStringLiteral("name")].toString() == themeName)
            return fi.filePath();
    }
    for (const QFileInfo &fi : info)
        if (fi.fileName() == themeName)
            return fi.filePath();

    return QString();
}

QString resolveActiveThemePath()
{
    QString settingsPath = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/ulauncher/settings.json");

    QFile file(settingsPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();
    if (error.error || !doc.isObject())
        return QString();

    const QString themeName = doc.object()[QStringLiteral("theme_name")].toString();
    if (themeName.isEmpty())
        return QString();

    // Search in user themes first, then in system locations.
    const QStringList roots = {
        userThemesDir(),
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/ulauncher/themes"),
        QStringLiteral("/usr/share/ulauncher/themes"),
    };
    for (const QString &root : roots) {
        const QString found = findThemeInRoot(root, themeName);
        if (!found.isEmpty())
            return found;
    }

    return QString();
}

QString setActiveThemePath(const QString &themePath)
{
    if (!QFileInfo(themePath).isDir())
        return QStringLiteral("Theme directory does not exist: %1").arg(themePath);

    // Prefer the manifest name; fall back to the directory name.
    QString themeName = QFileInfo(themePath).fileName();
    QFile manifestFile(QDir(themePath).filePath(QStringLiteral("manifest.json")));
    if (manifestFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(manifestFile.readAll(), &error);
        manifestFile.close();
        if (!error.error && doc.isObject()) {
            const QString name = doc.object()[QStringLiteral("name")].toString();
            if (!name.isEmpty())
                themeName = name;
        }
    }

    const QString settingsPath = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/ulauncher/settings.json");
    QDir().mkpath(QFileInfo(settingsPath).absolutePath());

    QJsonObject settings;
    QFile settingsFile(settingsPath);
    if (settingsFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(settingsFile.readAll(), &error);
        if (!error.error && doc.isObject())
            settings = doc.object();
    }
    settingsFile.close();

    settings[QStringLiteral("theme_name")] = themeName;
    if (!writeFileAtomic(settingsPath,
                         QString::fromUtf8(QJsonDocument(settings).toJson(QJsonDocument::Indented))))
        return QStringLiteral("Failed to write Ulauncher settings.json");

    return QString();
}

} // namespace tools