#include <QtTest>

#include "vision/VisionModelCapability.h"

class TestVisionModelCapability : public QObject
{
    Q_OBJECT

private slots:
    void emptyAndUnknownAreNotVision();
    void openAiVisionModels();
    void vendorVisionModels();
    void textOnlyModels();
    void caseInsensitive();
};

void TestVisionModelCapability::emptyAndUnknownAreNotVision()
{
    QVERIFY(!VisionModelCapability::isLikelyVisionModel(QString()));
    QVERIFY(!VisionModelCapability::isLikelyVisionModel("   "));
    QVERIFY(!VisionModelCapability::isLikelyVisionModel("some-random-model"));
}

void TestVisionModelCapability::openAiVisionModels()
{
    QVERIFY(VisionModelCapability::isLikelyVisionModel("gpt-4o"));
    QVERIFY(VisionModelCapability::isLikelyVisionModel("gpt-4o-mini"));
    QVERIFY(VisionModelCapability::isLikelyVisionModel("gpt-4.1-mini"));
    QVERIFY(VisionModelCapability::isLikelyVisionModel("gpt-5.2"));
    QVERIFY(VisionModelCapability::isLikelyVisionModel("o4-mini"));
}

void TestVisionModelCapability::vendorVisionModels()
{
    QVERIFY(VisionModelCapability::isLikelyVisionModel("claude-3-5-sonnet-20241022"));
    QVERIFY(VisionModelCapability::isLikelyVisionModel("gemini-2.0-flash"));
    QVERIFY(VisionModelCapability::isLikelyVisionModel("qwen2.5-vl-72b-instruct"));
    QVERIFY(VisionModelCapability::isLikelyVisionModel("glm-4v-flash"));
    QVERIFY(VisionModelCapability::isLikelyVisionModel("llava:13b"));
    QVERIFY(VisionModelCapability::isLikelyVisionModel("minicpm-v:8b"));
    QVERIFY(VisionModelCapability::isLikelyVisionModel("pixtral-large-latest"));
    QVERIFY(VisionModelCapability::isLikelyVisionModel("doubao-vision-pro-32k"));
}

void TestVisionModelCapability::textOnlyModels()
{
    QVERIFY(!VisionModelCapability::isLikelyVisionModel("gpt-3.5-turbo"));
    QVERIFY(!VisionModelCapability::isLikelyVisionModel("deepseek-chat"));
    QVERIFY(!VisionModelCapability::isLikelyVisionModel("qwen3:14b"));
    QVERIFY(!VisionModelCapability::isLikelyVisionModel("llama3.1:8b"));
}

void TestVisionModelCapability::caseInsensitive()
{
    QVERIFY(VisionModelCapability::isLikelyVisionModel("GPT-4O-MINI"));
    QVERIFY(VisionModelCapability::isLikelyVisionModel("Qwen2.5-VL-7B"));
    QVERIFY(VisionModelCapability::isLikelyVisionModel("  gpt-4o  "));
}

QTEST_MAIN(TestVisionModelCapability)
#include "tst_visionmodelcapability.moc"
