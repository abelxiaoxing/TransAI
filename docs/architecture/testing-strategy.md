# 测试策略

## 单元测试

**测试重点：**
- 主题管理器的颜色计算
- 文本选择检测的准确性
- 热键绑定的正确性
- 配置管理的持久化

```cpp
// TestThemeManager.cpp
class TestThemeManager : public QObject {
    Q_OBJECT

private slots:
    void testColorCalculation();
    void testThemeSwitching();
    void testThemePersistence();
};
```

## 集成测试

**测试重点：**
- 划词翻译的端到端流程
- 主题切换的视觉效果
- 动画性能测试
- 跨平台兼容性测试

## 手动测试

**测试场景：**
1. **主题测试**: 在不同系统上验证深色主题效果
2. **交互测试**: 验证输入框任意位置点击功能
3. **划词测试**: 在不同应用中测试划词翻译功能
4. **性能测试**: 长时间使用的性能表现