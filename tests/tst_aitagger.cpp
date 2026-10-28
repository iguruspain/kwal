#include "aitagger.h"

#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>

class tst_AiTagger : public QObject
{
    Q_OBJECT

private slots:
    void testNormalizeBase_data();
    void testNormalizeBase();
    void testDefaultSystemPrompt();
    void testBuildUserMessage();
    void testBuildUserContent();
    void testParseModels();
    void testResizedJpeg();
    void testParseTags_data();
    void testParseTags();
    void testParseTagsCap();
};

void tst_AiTagger::testNormalizeBase_data()
{
    QTest::addColumn<QString>("endpoint");
    QTest::addColumn<QString>("base");

    QTest::newRow("empty") << QString() << QString();
    QTest::newRow("bare host") << QStringLiteral("localhost:18080")
        << QStringLiteral("http://localhost:18080/v1");
    QTest::newRow("scheme host") << QStringLiteral("http://localhost:18080")
        << QStringLiteral("http://localhost:18080/v1");
    QTest::newRow("trailing slash") << QStringLiteral("http://localhost:18080/")
        << QStringLiteral("http://localhost:18080/v1");
    QTest::newRow("already v1") << QStringLiteral("http://localhost:18080/v1")
        << QStringLiteral("http://localhost:18080/v1");
    QTest::newRow("v1 with slash") << QStringLiteral("http://localhost:18080/v1/")
        << QStringLiteral("http://localhost:18080/v1");
    QTest::newRow("https remote") << QStringLiteral("https://api.example.com")
        << QStringLiteral("https://api.example.com/v1");
    QTest::newRow("whitespace") << QStringLiteral("  http://localhost:18080  ")
        << QStringLiteral("http://localhost:18080/v1");
}

void tst_AiTagger::testNormalizeBase()
{
    QFETCH(QString, endpoint);
    QFETCH(QString, base);
    QCOMPARE(aitagger::normalizeBase(endpoint), base);
}

void tst_AiTagger::testDefaultSystemPrompt()
{
    const QString prompt = aitagger::defaultSystemPrompt();
    QVERIFY(!prompt.isEmpty());
    QVERIFY(prompt.contains(QStringLiteral("commas")));
    QVERIFY(prompt.contains(QStringLiteral("3 to 5")));
}

void tst_AiTagger::testBuildUserMessage()
{
    const QString message = aitagger::buildUserMessage(
        QStringLiteral("/wallpapers/forest wallpaper.png"), QStringLiteral("image"));
    QVERIFY(message.contains(QStringLiteral("forest wallpaper.png")));
    QVERIFY(message.contains(QStringLiteral("image")));
    QVERIFY(!message.contains(QStringLiteral("/wallpapers")));
}

void tst_AiTagger::testBuildUserContent()
{
    const QByteArray data("fake-jpeg-bytes");
    const QJsonValue content =
        aitagger::buildUserContent(QStringLiteral("/wallpapers/forest.jpg"), QStringLiteral("image"), data, QStringLiteral("image/jpeg"));
    const QJsonArray parts = content.toArray();
    QCOMPARE(parts.size(), 2);
    QCOMPARE(parts.at(0).toObject().value(QStringLiteral("type")).toString(), QString::fromLatin1("text"));
    QVERIFY(parts.at(0).toObject().value(QStringLiteral("text")).toString().contains(QStringLiteral("forest.jpg")));
    QCOMPARE(parts.at(1).toObject().value(QStringLiteral("type")).toString(), QString::fromLatin1("image_url"));
    QCOMPARE(parts.at(1).toObject().value(QStringLiteral("image_url")).toObject().value(QStringLiteral("url")).toString(),
             QString::fromLatin1("data:image/jpeg;base64,") + QString::fromLatin1(data.toBase64()));

    // No image data -> plain text-only message (text-only servers).
    const QJsonValue plain =
        aitagger::buildUserContent(QStringLiteral("/wallpapers/forest.jpg"), QStringLiteral("image"), {}, QStringLiteral("image/jpeg"));
    QCOMPARE(plain.isString(), true);
    QVERIFY(plain.toString().contains(QStringLiteral("forest.jpg")));
}

void tst_AiTagger::testParseModels()
{
    // llama.cpp shape: architecture.input_modalities.
    const QString llama = R"({
        "data": [
            {"id": "qwen2.5-vl-gguf", "object": "model", "architecture": {"input_modalities": ["image", "text"]}},
            {"id": "llama3.1-text", "object": "model", "architecture": {"input_modalities": ["text"]}}
        ]
    })";
    QCOMPARE(aitagger::parseModels(QJsonDocument::fromJson(llama.toUtf8())), QStringList{QStringLiteral("qwen2.5-vl-gguf")});

    // OpenAI shape: capabilities.input_modalities.
    const QString openai = R"({
        "data": [
            {"id": "gpt-4o", "capabilities": {"input_modalities": ["image", "text"]}},
            {"id": "gpt-4o-mini", "capabilities": {"input_modalities": ["text"]}}
        ]
    })";
    QCOMPARE(aitagger::parseModels(QJsonDocument::fromJson(openai.toUtf8())), QStringList{QStringLiteral("gpt-4o")});

    // Servers that report no modalities keep every model.
    const QString legacy = R"({"data": [{"id": "m1"}, {"id": "m2"}]})";
    const QStringList legacyModels{QStringLiteral("m1"), QStringLiteral("m2")};
    QCOMPARE(aitagger::parseModels(QJsonDocument::fromJson(legacy.toUtf8())), legacyModels);

    // No models -> empty (the caller raises the error).
    const QString none = R"({"data": [{"id": "t", "architecture": {"input_modalities": ["text"]}}]})";
    QVERIFY(aitagger::parseModels(QJsonDocument::fromJson(none.toUtf8())).isEmpty());
}

void tst_AiTagger::testResizedJpeg()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // Large image -> resized, aspect kept, JPEG bytes.
    QImage large(2000, 1500, QImage::Format_RGB32);
    large.fill(Qt::red);
    const QString largePath = dir.filePath(QStringLiteral("large.png"));
    QVERIFY(large.save(largePath));
    const QByteArray largeJpeg = aitagger::resizedJpeg(largePath, 1024, 85);
    QVERIFY(!largeJpeg.isEmpty());
    // Decode through a file: this Qt build cannot sniff image formats from
    // raw byte data (the app only ever decodes from disk).
    const QString largeJpegPath = dir.filePath(QStringLiteral("large.jpg"));
    QFile largeJpegFile(largeJpegPath);
    QVERIFY(largeJpegFile.open(QIODevice::WriteOnly));
    largeJpegFile.write(largeJpeg);
    largeJpegFile.close();
    const QImage decoded(largeJpegPath);
    QVERIFY(!decoded.isNull());
    QVERIFY(decoded.width() <= 1024 && decoded.height() <= 1024);
    QCOMPARE(decoded.width(), 1024);
    QCOMPARE(decoded.height(), 768);

    // Small image -> not upscaled.
    QImage small(100, 50, QImage::Format_RGB32);
    small.fill(Qt::blue);
    const QString smallPath = dir.filePath(QStringLiteral("small.png"));
    QVERIFY(small.save(smallPath));
    const QByteArray smallJpeg = aitagger::resizedJpeg(smallPath, 1024, 85);
    const QString smallJpegPath = dir.filePath(QStringLiteral("small.jpg"));
    QFile smallJpegFile(smallJpegPath);
    QVERIFY(smallJpegFile.open(QIODevice::WriteOnly));
    smallJpegFile.write(smallJpeg);
    smallJpegFile.close();
    const QImage smallDecoded(smallJpegPath);
    QVERIFY(!smallDecoded.isNull());
    QCOMPARE(smallDecoded.width(), 100);
    QCOMPARE(smallDecoded.height(), 50);

    // Missing file -> empty.
    QVERIFY(aitagger::resizedJpeg(dir.filePath(QStringLiteral("missing.png")), 1024, 85).isEmpty());
}

void tst_AiTagger::testParseTags_data()
{
    QTest::addColumn<QString>("content");
    QTest::addColumn<QStringList>("tags");

    QTest::newRow("empty") << QString() << QStringList();
    QTest::newRow("whitespace only") << QStringLiteral(" \n ") << QStringList();
    QTest::newRow("json array") << QStringLiteral("[\"forest\", \"nature\"]")
        << QStringList({QStringLiteral("forest"), QStringLiteral("nature")});
    QTest::newRow("json in prose")
        << QStringLiteral("Here are the tags: [\"city\", \"night\", \"rain\"]. Hope that helps!")
        << QStringList({QStringLiteral("city"), QStringLiteral("night"), QStringLiteral("rain")});
    QTest::newRow("json numbers") << QStringLiteral("[1, 2.5]")
        << QStringList({QStringLiteral("1"), QStringLiteral("2.5")});
    QTest::newRow("commas") << QStringLiteral("a, b, c")
        << QStringList({QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")});
    QTest::newRow("semicolons and newlines") << QStringLiteral("a;b\nc; d")
        << QStringList({QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c"), QStringLiteral("d")});
    QTest::newRow("quoted tags") << QStringLiteral("\"a\", 'b', `c`")
        << QStringList({QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")});
    QTest::newRow("bullets") << QStringLiteral("- a\n- b")
        << QStringList({QStringLiteral("a"), QStringLiteral("b")});
    QTest::newRow("trailing dot") << QStringLiteral("forest.")
        << QStringList({QStringLiteral("forest")});
    QTest::newRow("dedupe case") << QStringLiteral("A, a, A.")
        << QStringList({QStringLiteral("A")});
    QTest::newRow("invalid json falls back") << QStringLiteral("[a, b]")
        << QStringList({QStringLiteral("a"), QStringLiteral("b")});
    QTest::newRow("overlong tag dropped")
        << (QString(50, QLatin1Char('x')) + QStringLiteral(", ok"))
        << QStringList({QStringLiteral("ok")});
    QTest::newRow("empty parts skipped") << QStringLiteral(", , a ,")
        << QStringList({QStringLiteral("a")});
}

void tst_AiTagger::testParseTags()
{
    QFETCH(QString, content);
    QFETCH(QStringList, tags);
    QCOMPARE(aitagger::parseTags(content), tags);
}

void tst_AiTagger::testParseTagsCap()
{
    QStringList many;
    for (int i = 0; i < 20; ++i) {
        many.append(QStringLiteral("tag%1").arg(i));
    }
    const QString joined = many.join(QStringLiteral(", "));
    const QStringList tags = aitagger::parseTags(joined);
    QCOMPARE(tags.size(), aitagger::kMaxTags);
    for (int i = 0; i < aitagger::kMaxTags; ++i) {
        QCOMPARE(tags.at(i), many.at(i));
    }
}

QTEST_GUILESS_MAIN(tst_AiTagger)
#include "tst_aitagger.moc"
