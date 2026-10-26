#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QVariantMap>

// Resolves semantic icon names to a font family + glyph. Prefers the system
// Fluent icon font (Segoe Fluent Icons, or Segoe MDL2 Assets on Windows 10,
// which shares the codepoints used here). Falls back to Font Awesome 6 Free,
// bundled through the QtAwesome package's font resources and codepoint enum.
class IconService : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Icons)
    QML_SINGLETON
    Q_PROPERTY(QString fluentFamily READ fluentFamily CONSTANT)
    Q_PROPERTY(QString awesomeFamily READ awesomeFamily CONSTANT)
    Q_PROPERTY(QString activeSet READ activeSet CONSTANT)
public:
    explicit IconService(QObject *parent = nullptr);

    QString fluentFamily() const { return m_fluentFamily; }
    QString awesomeFamily() const { return m_awesomeSolid; }
    QString activeSet() const;

    // {family, glyph, set}. Unknown names resolve to a question-mark glyph with set "none".
    Q_INVOKABLE QVariantMap resolve(const QString &name) const;
    Q_INVOKABLE QStringList names() const;

private:
    QString m_fluentFamily;
    QString m_awesomeSolid;
    QString m_awesomeBrands;
};
