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
    QCOMPARE(pathOf(ProviderEndpointBuilder::buildChatCompletionUrl("https://api.openai.com", "openai")),
             QString("/v1/chat/completions"));
}

void TestProviderEndpointBuilder::openAiFullChatUrlKept()
{
    QCOMPARE(pathOf(ProviderEndpointBuilder::buildChatCompletionUrl("https://x.example/v1/chat/completions", "openai")),
             QString("/v1/chat/completions"));
}

void TestProviderEndpointBuilder::openAiModelsUrlSwapped()
{
    // Pasted model-list URL is swapped to the chat endpoint.
    QCOMPARE(pathOf(ProviderEndpointBuilder::buildChatCompletionUrl("https://x.example/models", "openai")),
             QString("/chat/completions"));
}

void TestProviderEndpointBuilder::openAiV1Appended()
{
    QCOMPARE(pathOf(ProviderEndpointBuilder::buildChatCompletionUrl("https://x.example/v1", "openai")),
             QString("/v1/chat/completions"));
    // Custom deployments keep their base path.
    QCOMPARE(pathOf(ProviderEndpointBuilder::buildChatCompletionUrl("https://x.example/api/openai", "openai")),
             QString("/api/openai/chat/completions"));
}

void TestProviderEndpointBuilder::openAiTrailingSlashTolerated()
{
    QCOMPARE(pathOf(ProviderEndpointBuilder::buildChatCompletionUrl("https://x.example/v1/", "openai")),
             QString("/v1/chat/completions"));
}

void TestProviderEndpointBuilder::openAiModelListUrls()
{
    QCOMPARE(pathOf(ProviderEndpointBuilder::buildModelListUrl("https://api.openai.com", "openai")), QString("/v1/models"));
    QCOMPARE(pathOf(ProviderEndpointBuilder::buildModelListUrl("https://x.example/v1", "openai")), QString("/v1/models"));
    QCOMPARE(pathOf(ProviderEndpointBuilder::buildModelListUrl("https://x.example/v1/models", "openai")),
             QString("/v1/models"));
    // Pasted chat URL reduces to the model list.
    QCOMPARE(pathOf(ProviderEndpointBuilder::buildModelListUrl("https://x.example/v1/chat/completions", "openai")),
             QString("/v1/models"));
}

void TestProviderEndpointBuilder::ollamaBareServerGetsV1Chat()
{
    QCOMPARE(pathOf(ProviderEndpointBuilder::buildChatCompletionUrl("http://localhost:11434", "ollama")),
             QString("/v1/chat/completions"));
}

void TestProviderEndpointBuilder::ollamaNativeChatUrlMigrated()
{
    // Native Ollama endpoints migrate to the OpenAI-compatible one.
    QCOMPARE(pathOf(ProviderEndpointBuilder::buildChatCompletionUrl("http://localhost:11434/api/chat", "ollama")),
             QString("/v1/chat/completions"));
    QCOMPARE(pathOf(ProviderEndpointBuilder::buildChatCompletionUrl("http://localhost:11434/v1", "ollama")),
             QString("/v1/chat/completions"));
    // Pasted model-list URL migrates too.
    QCOMPARE(pathOf(ProviderEndpointBuilder::buildChatCompletionUrl("http://localhost:11434/v1/models", "ollama")),
             QString("/v1/chat/completions"));
}

void TestProviderEndpointBuilder::ollamaTagsUrlSwapped()
{
    QCOMPARE(pathOf(ProviderEndpointBuilder::buildChatCompletionUrl("http://localhost:11434/api/tags", "ollama")),
             QString("/v1/chat/completions"));
}

void TestProviderEndpointBuilder::ollamaModelListUrls()
{
    QCOMPARE(pathOf(ProviderEndpointBuilder::buildModelListUrl("http://localhost:11434", "ollama")), QString("/api/tags"));
    QCOMPARE(pathOf(ProviderEndpointBuilder::buildModelListUrl("http://localhost:11434/v1", "ollama")), QString("/api/tags"));
    QCOMPARE(pathOf(ProviderEndpointBuilder::buildModelListUrl("http://localhost:11434/api/chat", "ollama")),
             QString("/api/tags"));
    QCOMPARE(pathOf(ProviderEndpointBuilder::buildModelListUrl("http://localhost:11434/v1/chat/completions", "ollama")),
             QString("/api/tags"));
    // Pasted OpenAI-compatible model-list URL reduces to the native tag list.
    QCOMPARE(pathOf(ProviderEndpointBuilder::buildModelListUrl("http://localhost:11434/v1/models", "ollama")),
             QString("/api/tags"));
}

void TestProviderEndpointBuilder::emptyServerFallsBackToDefault()
{
    QCOMPARE(pathOf(ProviderEndpointBuilder::buildChatCompletionUrl("", "openai")), QString("/v1/chat/completions"));
    QCOMPARE(pathOf(ProviderEndpointBuilder::buildChatCompletionUrl("  ", "ollama")), QString("/v1/chat/completions"));
    QCOMPARE(pathOf(ProviderEndpointBuilder::buildModelListUrl("", "openai")), QString("/v1/models"));
    QCOMPARE(pathOf(ProviderEndpointBuilder::buildModelListUrl("", "ollama")), QString("/api/tags"));
}

QTEST_MAIN(TestProviderEndpointBuilder)
#include "tst_providerendpointbuilder.moc"
