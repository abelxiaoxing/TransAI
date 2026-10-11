#include <QtTest>

#include "providers/ThinkingPolicy.h"

namespace {
ThinkingPolicy autoPolicy(const QString& model, const QString& apiServer = "https://api.openai.com",
                          const QString& provider = "openai")
{
    return ThinkingPolicyBuilder::build(provider, apiServer, model, "auto");
}
} // namespace

class TestThinkingPolicy : public QObject
{
    Q_OBJECT

private slots:
    void normalizeThinkingMode();
    void manualModeIsPassthrough();
    void unknownNonThinkingModelIsUntouched();
    void ollamaProviderUsesThinkFlag();
    void deepSeekChatUsesThinkingDisabled();
    void deepSeekReasonerUsesNoThinkTag();
    void qwen3UsesEnableThinking();
    void qwenQwqDetected();
    void openAiReasoningUsesLowEffort();
    void openRouterThinkingModelCapped();
    void openRouterNonThinkingUntouched();
    void fallbackKeywordHint();
    void forcedProfiles();
};

void TestThinkingPolicy::normalizeThinkingMode()
{
    QCOMPARE(ThinkingPolicyBuilder::normalizeThinkingMode("auto"), QString("auto"));
    QCOMPARE(ThinkingPolicyBuilder::normalizeThinkingMode(""), QString("auto"));
    QCOMPARE(ThinkingPolicyBuilder::normalizeThinkingMode("garbage"), QString("auto"));
    QCOMPARE(ThinkingPolicyBuilder::normalizeThinkingMode("Qwen"), QString("qwen"));
    QCOMPARE(ThinkingPolicyBuilder::normalizeThinkingMode("DEEPSEEK"), QString("deepseek"));
    QCOMPARE(ThinkingPolicyBuilder::normalizeThinkingMode("none"), QString("manual"));
    QCOMPARE(ThinkingPolicyBuilder::normalizeThinkingMode("manual"), QString("manual"));
}

void TestThinkingPolicy::manualModeIsPassthrough()
{
    const ThinkingPolicy policy =
        ThinkingPolicyBuilder::build("openai", "https://api.openai.com", "deepseek-reasoner", "manual");
    QVERIFY(policy.bodyParams.isEmpty());
    QVERIFY(!policy.noThinkTag);
    QVERIFY(!policy.directAnswerHint);
    QVERIFY(!policy.isActive());
}

void TestThinkingPolicy::unknownNonThinkingModelIsUntouched()
{
    const ThinkingPolicy policy = autoPolicy("gpt-3.5-turbo");
    QVERIFY(policy.bodyParams.isEmpty());
    QVERIFY(!policy.noThinkTag);
    QVERIFY(!policy.directAnswerHint);
}

void TestThinkingPolicy::ollamaProviderUsesThinkFlag()
{
    const ThinkingPolicy policy = autoPolicy("llama3", "http://localhost:11434", "ollama");
    QCOMPARE(policy.bodyParams.value("think"), QJsonValue(false));
}

void TestThinkingPolicy::deepSeekChatUsesThinkingDisabled()
{
    const ThinkingPolicy policy = autoPolicy("deepseek-chat");
    QCOMPARE(policy.bodyParams.value("thinking").toObject().value("type"), QJsonValue("disabled"));
    QVERIFY(!policy.noThinkTag);
}

void TestThinkingPolicy::deepSeekReasonerUsesNoThinkTag()
{
    const ThinkingPolicy policy = autoPolicy("deepseek-r1-0528");
    QVERIFY(policy.bodyParams.isEmpty());
    QVERIFY(policy.noThinkTag);

    const ThinkingPolicy reasoner = autoPolicy("deepseek-reasoner");
    QVERIFY(reasoner.noThinkTag);
}

void TestThinkingPolicy::qwen3UsesEnableThinking()
{
    const ThinkingPolicy policy = autoPolicy("qwen3-8b");
    QCOMPARE(policy.bodyParams.value("enable_thinking"), QJsonValue(false));
    QVERIFY(policy.noThinkTag);
}

void TestThinkingPolicy::qwenQwqDetected()
{
    const ThinkingPolicy policy = autoPolicy("qwq-32b");
    QCOMPARE(policy.bodyParams.value("enable_thinking"), QJsonValue(false));
    QVERIFY(policy.noThinkTag);
}

void TestThinkingPolicy::openAiReasoningUsesLowEffort()
{
    for (const QString& model : {QString("o1-mini"), QString("o3"), QString("o4-mini"), QString("gpt-5")}) {
        const ThinkingPolicy policy = autoPolicy(model);
        QCOMPARE(policy.bodyParams.value("reasoning_effort"), QJsonValue("low"));
    }
}

void TestThinkingPolicy::openRouterThinkingModelCapped()
{
    const ThinkingPolicy policy = autoPolicy("qwen3-8b", "https://openrouter.ai/api/v1");
    QCOMPARE(policy.bodyParams.value("reasoning").toObject().value("effort"), QJsonValue("low"));

    const ThinkingPolicy deepseek = autoPolicy("deepseek-chat", "https://openrouter.ai/api/v1");
    QCOMPARE(deepseek.bodyParams.value("reasoning").toObject().value("effort"), QJsonValue("low"));
}

void TestThinkingPolicy::openRouterNonThinkingUntouched()
{
    const ThinkingPolicy policy = autoPolicy("some-alias-model", "https://openrouter.ai/api/v1");
    QVERIFY(policy.bodyParams.isEmpty());
}

void TestThinkingPolicy::fallbackKeywordHint()
{
    // Keyword-matched thinking model without a vendor-specific parameter
    // gets the safe prompt-level hint only.
    const ThinkingPolicy policy = autoPolicy("minimax-m1");
    QVERIFY(policy.bodyParams.isEmpty());
    QVERIFY(policy.directAnswerHint);
}

void TestThinkingPolicy::forcedProfiles()
{
    // Relay alias model ids hide the real family; forced profiles apply anyway.
    const ThinkingPolicy qwen =
        ThinkingPolicyBuilder::build("openai", "https://relay.example", "LocalModel", "qwen");
    QCOMPARE(qwen.bodyParams.value("enable_thinking"), QJsonValue(false));
    QVERIFY(qwen.noThinkTag);

    const ThinkingPolicy deepseek =
        ThinkingPolicyBuilder::build("openai", "https://relay.example", "LocalModel", "deepseek");
    QCOMPARE(deepseek.bodyParams.value("thinking").toObject().value("type"), QJsonValue("disabled"));

    const ThinkingPolicy openai =
        ThinkingPolicyBuilder::build("openai", "https://relay.example", "LocalModel", "openai");
    QCOMPARE(openai.bodyParams.value("reasoning_effort"), QJsonValue("low"));

    const ThinkingPolicy ollama =
        ThinkingPolicyBuilder::build("openai", "https://relay.example", "LocalModel", "ollama");
    QCOMPARE(ollama.bodyParams.value("think"), QJsonValue(false));
}

QTEST_MAIN(TestThinkingPolicy)
#include "tst_thinkingpolicy.moc"
