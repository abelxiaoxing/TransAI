#include <QtTest>

#include "providers/ProviderEndpointBuilder.h"

namespace {
// URL-building checks ignore scheme/host and focus on path normalization.
QString pathOf(const QUrl& url)
{
    return url.path();
}
} // namespace

class TestProviderEndpointBuilder : public QObject
{
    Q_OBJECT

private slots:
    void openAiBareServerGetsV1Chat();
    void openAiFullChatUrlKept();
    void openAiModelsUrlSwapped();
    void openAiV1Appended();
    void openAiTrailingSlashTolerated();
    void openAiModelListUrls();
    void ollamaBareServerGetsV1Chat();
    void ollamaNativeChatUrlMigrated();
    void ollamaTagsUrlSwapped();
    void ollamaModelListUrls();
    void emptyServerFallsBackToDefault();
};

void TestProviderEndpointBuilder::openAiBareServerGetsV1Chat()
{
    ProviderEndpointBuilder builder;
    QCOMPARE(pathOf(builder.buildChatCompletionUrl("https://api.openai.com", "openai")),
             QString("/v1/chat/completions"));
}

void TestProviderEndpointBuilder::openAiFullChatUrlKept()
{
    ProviderEndpointBuilder builder;
    QCOMPARE(pathOf(builder.buildChatCompletionUrl("https://x.example/v1/chat/completions", "openai")),
             QString("/v1/chat/completions"));
}

void TestProviderEndpointBuilder::openAiModelsUrlSwapped()
{
    ProviderEndpointBuilder builder;
    // Pasted model-list URL is swapped to the chat endpoint.
    QCOMPARE(pathOf(builder.buildChatCompletionUrl("https://x.example/models", "openai")),
             QString("/chat/completions"));
}

void TestProviderEndpointBuilder::openAiV1Appended()
{
    ProviderEndpointBuilder builder;
    QCOMPARE(pathOf(builder.buildChatCompletionUrl("https://x.example/v1", "openai")),
             QString("/v1/chat/completions"));
    // Custom deployments keep their base path.
    QCOMPARE(pathOf(builder.buildChatCompletionUrl("https://x.example/api/openai", "openai")),
             QString("/api/openai/chat/completions"));
}

void TestProviderEndpointBuilder::openAiTrailingSlashTolerated()
{
    ProviderEndpointBuilder builder;
    QCOMPARE(pathOf(builder.buildChatCompletionUrl("https://x.example/v1/", "openai")),
             QString("/v1/chat/completions"));
}

void TestProviderEndpointBuilder::openAiModelListUrls()
{
    ProviderEndpointBuilder builder;
    QCOMPARE(pathOf(builder.buildModelListUrl("https://api.openai.com", "openai")), QString("/v1/models"));
    QCOMPARE(pathOf(builder.buildModelListUrl("https://x.example/v1", "openai")), QString("/v1/models"));
    QCOMPARE(pathOf(builder.buildModelListUrl("https://x.example/v1/models", "openai")),
             QString("/v1/models"));
    // Pasted chat URL reduces to the model list.
    QCOMPARE(pathOf(builder.buildModelListUrl("https://x.example/v1/chat/completions", "openai")),
             QString("/v1/models"));
}

void TestProviderEndpointBuilder::ollamaBareServerGetsV1Chat()
{
    ProviderEndpointBuilder builder;
    QCOMPARE(pathOf(builder.buildChatCompletionUrl("http://localhost:11434", "ollama")),
             QString("/v1/chat/completions"));
}

void TestProviderEndpointBuilder::ollamaNativeChatUrlMigrated()
{
    ProviderEndpointBuilder builder;
    // Native Ollama endpoints migrate to the OpenAI-compatible one.
    QCOMPARE(pathOf(builder.buildChatCompletionUrl("http://localhost:11434/api/chat", "ollama")),
             QString("/v1/chat/completions"));
    QCOMPARE(pathOf(builder.buildChatCompletionUrl("http://localhost:11434/v1", "ollama")),
             QString("/v1/chat/completions"));
    // Pasted model-list URL migrates too.
    QCOMPARE(pathOf(builder.buildChatCompletionUrl("http://localhost:11434/v1/models", "ollama")),
             QString("/v1/chat/completions"));
}

void TestProviderEndpointBuilder::ollamaTagsUrlSwapped()
{
    ProviderEndpointBuilder builder;
    QCOMPARE(pathOf(builder.buildChatCompletionUrl("http://localhost:11434/api/tags", "ollama")),
             QString("/v1/chat/completions"));
}

void TestProviderEndpointBuilder::ollamaModelListUrls()
{
    ProviderEndpointBuilder builder;
    QCOMPARE(pathOf(builder.buildModelListUrl("http://localhost:11434", "ollama")), QString("/api/tags"));
    QCOMPARE(pathOf(builder.buildModelListUrl("http://localhost:11434/v1", "ollama")), QString("/api/tags"));
    QCOMPARE(pathOf(builder.buildModelListUrl("http://localhost:11434/api/chat", "ollama")),
             QString("/api/tags"));
    QCOMPARE(pathOf(builder.buildModelListUrl("http://localhost:11434/v1/chat/completions", "ollama")),
             QString("/api/tags"));
    // Pasted OpenAI-compatible model-list URL reduces to the native tag list.
    QCOMPARE(pathOf(builder.buildModelListUrl("http://localhost:11434/v1/models", "ollama")),
             QString("/api/tags"));
}

void TestProviderEndpointBuilder::emptyServerFallsBackToDefault()
{
    ProviderEndpointBuilder builder;
    QCOMPARE(pathOf(builder.buildChatCompletionUrl("", "openai")), QString("/v1/chat/completions"));
    QCOMPARE(pathOf(builder.buildChatCompletionUrl("  ", "ollama")), QString("/v1/chat/completions"));
    QCOMPARE(pathOf(builder.buildModelListUrl("", "openai")), QString("/v1/models"));
    QCOMPARE(pathOf(builder.buildModelListUrl("", "ollama")), QString("/api/tags"));
}

QTEST_MAIN(TestProviderEndpointBuilder)
#include "tst_providerendpointbuilder.moc"
