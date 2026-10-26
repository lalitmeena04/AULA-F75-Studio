#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

// Local profiles: lighting, per-key colours, key map, macros and sleep timer
// as versioned JSON with atomic writes. Profiles are host-side documents; the
// Device applies a profile to hardware explicitly.
class ProfileStore final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList profiles READ profiles NOTIFY changed)
    Q_PROPERTY(int activeIndex READ activeIndex WRITE setActiveIndex NOTIFY changed)
    Q_PROPERTY(QString storagePath READ storagePath CONSTANT)
    Q_PROPERTY(QString lastError READ lastError NOTIFY changed)
public:
    explicit ProfileStore(QString path, QObject *parent = nullptr);

    QVariantList profiles() const { return m_profiles; }
    int activeIndex() const { return m_active; }
    void setActiveIndex(int index);
    QString storagePath() const { return m_path; }
    QString lastError() const { return m_error; }

    Q_INVOKABLE QVariantMap profile(int index) const;
    Q_INVOKABLE int create(const QString &name);
    Q_INVOKABLE void update(int index, const QVariantMap &profile);
    Q_INVOKABLE void rename(int index, const QString &name);
    Q_INVOKABLE void duplicate(int index);
    Q_INVOKABLE void remove(int index);
    Q_INVOKABLE bool exportTo(int index, const QUrl &fileUrl);
    Q_INVOKABLE bool importFrom(const QUrl &fileUrl);
    static QVariantMap defaultProfile(const QString &name);

signals:
    void changed();

private:
    bool load();
    bool save();
    static QVariantMap sanitize(const QVariantMap &in);
    QString m_path;
    QVariantList m_profiles;
    int m_active = 0;
    QString m_error;
};
