#ifndef SELECTION_TEXT_PROVIDER_H
#define SELECTION_TEXT_PROVIDER_H

#include <QObject>

class SelectionTextProvider : public QObject
{
    Q_OBJECT

public:
    explicit SelectionTextProvider(QObject* parent = nullptr);

    Q_INVOKABLE void requestSelectedText();

signals:
    void selectedTextReady(QString text);
    void selectedTextFailed(QString reason);

private:
    bool simulateCopy() const;
};

#endif // SELECTION_TEXT_PROVIDER_H
