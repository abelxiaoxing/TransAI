#include <QtTest>

#include "providers/ThinkingPolicy.h"

namespace {
ThinkingPolicy autoPolicy(const QString& model, const QString& apiServer = "https://api.openai.com",
                          const QString& provider = "openai")
{
    return ThinkingPolicyBuilder::build(provider, apiServer, model);
}
} // namespace

class TestThinkingPolicy : public QObject
{
    Q_OBJECT

private slots:
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
    void thinkingParamRejectionDetection();
};

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

void TestThinkingPolicy::thinkingParamRejectionDetection()
{
    // Rejections naming a thinking parameter: retry candidates.
    QVERIFY(ThinkingPolicyBuilder::isThinkingParamRejection(
        QStringLiteral("Unknown parameter: 'enable_thinking'")));
    QVERIFY(ThinkingPolicyBuilder::isThinkingParamRejection(
        QStringLiteral("Extra fields not permitted [input: enable_thinking]")));
    QVERIFY(ThinkingPolicyBuilder::isThinkingParamRejection(
        QStringLiteral("Extra inputs are not permitted [type=extra_forbidden, loc=['body', 'enable_thinking']]")));
    QVERIFY(ThinkingPolicyBuilder::isThinkingParamRejection(
        QStringLiteral("Unrecognized request argument supplied: reasoning_effort")));
    QVERIFY(ThinkingPolicyBuilder::isThinkingParamRejection(
        QStringLiteral("this model does not support the thinking parameter")));
    QVERIFY(ThinkingPolicyBuilder::isThinkingParamRejection(
        QStringLiteral("Invalid parameter: 'think'")));

    // Unrelated failures must not trigger a retry.
    QVERIFY(!ThinkingPolicyBuilder::isThinkingParamRejection(QStringLiteral("Incorrect API key provided")));
    QVERIFY(!ThinkingPolicyBuilder::isThinkingParamRejection(QStringLiteral("model `foo` not found")));
    QVERIFY(!ThinkingPolicyBuilder::isThinkingParamRejection(QStringLiteral("Request failed")));
    QVERIFY(!ThinkingPolicyBuilder::isThinkingParamRejection(QStringLiteral("streaming not supported")));
    QVERIFY(!ThinkingPolicyBuilder::isThinkingParamRejection(
        QStringLiteral("Unknown parameter: 'temperature'")));
    QVERIFY(!ThinkingPolicyBuilder::isThinkingParamRejection(QStringLiteral("")));
}

QTEST_MAIN(TestThinkingPolicy)
#include "tst_thinkingpolicy.moc"
