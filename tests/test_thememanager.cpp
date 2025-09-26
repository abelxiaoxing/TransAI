#include <QtTest>
#include <QSignalSpy>
#include "../src/theme/ThemeManager.h"

class TestThemeManager : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testColorCalculation();
    void testThemeSwitching();
    void testThemePersistence();
    void testInvalidColorName();

private:
    ThemeManager* m_themeManager;
};

void TestThemeManager::initTestCase()
{
    m_themeManager = new ThemeManager();
}

void TestThemeManager::cleanupTestCase()
{
    delete m_themeManager;
}

void TestThemeManager::testColorCalculation()
{
    // 测试深色主题颜色
    m_themeManager->setCurrentTheme(ThemeManager::DarkTheme);

    QCOMPARE(m_themeManager->getColor("background"), QColor("#1E1E1E"));
    QCOMPARE(m_themeManager->getColor("foreground"), QColor("#D4D4D4"));
    QCOMPARE(m_themeManager->getColor("accent"), QColor("#4EC9B0"));
    QCOMPARE(m_themeManager->getColor("border"), QColor("#3E3E42"));

    // 测试浅色主题颜色
    m_themeManager->setCurrentTheme(ThemeManager::LightTheme);

    QCOMPARE(m_themeManager->getColor("background"), QColor("#FFFFFF"));
    QCOMPARE(m_themeManager->getColor("foreground"), QColor("#1E1E1E"));
    QCOMPARE(m_themeManager->getColor("accent"), QColor("#4EC9B0"));
    QCOMPARE(m_themeManager->getColor("border"), QColor("#E0E0E0"));
}

void TestThemeManager::testThemeSwitching()
{
    QSignalSpy themeChangedSpy(m_themeManager, &ThemeManager::currentThemeChanged);

    // 测试主题切换
    m_themeManager->setCurrentTheme(ThemeManager::DarkTheme);
    QCOMPARE(m_themeManager->currentTheme(), ThemeManager::DarkTheme);
    QCOMPARE(themeChangedSpy.count(), 1);

    m_themeManager->setCurrentTheme(ThemeManager::LightTheme);
    QCOMPARE(m_themeManager->currentTheme(), ThemeManager::LightTheme);
    QCOMPARE(themeChangedSpy.count(), 2);

    // 测试相同主题不触发信号
    m_themeManager->setCurrentTheme(ThemeManager::LightTheme);
    QCOMPARE(themeChangedSpy.count(), 2); // 不应该增加
}

void TestThemeManager::testThemePersistence()
{
    // 设置主题并保存
    m_themeManager->setCurrentTheme(ThemeManager::LightTheme);
    m_themeManager->saveTheme();

    // 创建新的主题管理器测试加载
    ThemeManager newManager;
    QCOMPARE(newManager.currentTheme(), ThemeManager::LightTheme);
}

void TestThemeManager::testInvalidColorName()
{
    // 测试无效颜色名称返回默认黑色
    QColor invalidColor = m_themeManager->getColor("invalidColorName");
    QCOMPARE(invalidColor, QColor("#000000"));
}

QTEST_APPLESS_MAIN(TestThemeManager)
#include "test_thememanager.moc"