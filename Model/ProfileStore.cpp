#include "ProfileStore.h"
#include "Protocol.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUrl>

namespace {
constexpr int schemaVersion = 1;
constexpr qint64 maxBytes = 4 * 1024 * 1024;
} // namespace

ProfileStore::ProfileStore(QString path, QObject* parent) : QObject(parent), m_path(std::move(path)) {
    if(!load() || m_profiles.isEmpty()) {
        if(m_profiles.isEmpty())
            m_profiles.append(defaultProfile(QStringLiteral("Default")));
    }
}

QVariantMap ProfileStore::defaultProfile(const QString& name) {
    return {
        {QStringLiteral("name"), name},
        {QStringLiteral("lighting"), QVariantMap{{QStringLiteral("effectIndex"), 9},
                                                 {QStringLiteral("brightness"), 7},
                                                 {QStringLiteral("speed"), 2},
                                                 {QStringLiteral("color"), 7}}},
        {QStringLiteral("perKey"), QVariantMap{}},
        {QStringLiteral("keymap"), QVariantMap{}},
        {QStringLiteral("macros"), QVariantList{}},
        {QStringLiteral("sleepSeconds"), 300},
    };
}

QVariantMap ProfileStore::sanitize(const QVariantMap& in) {
    QVariantMap out = defaultProfile(in.value(QStringLiteral("name")).toString().left(48));
    if(out[QStringLiteral("name")].toString().trimmed().isEmpty())
        out[QStringLiteral("name")] = QStringLiteral("Profile");
    const QVariantMap lighting = in.value(QStringLiteral("lighting")).toMap();
    QVariantMap l = out[QStringLiteral("lighting")].toMap();
    auto clampInt = [](const QVariant& v, int lo, int hi, int def) {
        bool ok = false;
        const int n = v.toInt(&ok);
        return ok ? std::clamp(n, lo, hi) : def;
    };
    l[QStringLiteral("effectIndex")] = clampInt(lighting.value(QStringLiteral("effectIndex")), 0, 19, 9);
    l[QStringLiteral("brightness")] = clampInt(lighting.value(QStringLiteral("brightness")), 0, 9, 7);
    l[QStringLiteral("speed")] = clampInt(lighting.value(QStringLiteral("speed")), 0, 4, 2);
    l[QStringLiteral("color")] = clampInt(lighting.value(QStringLiteral("color")), 0, 7, 7);
    out[QStringLiteral("lighting")] = l;

    QVariantMap perKey;
    const QVariantMap inPerKey = in.value(QStringLiteral("perKey")).toMap();
    for(auto it = inPerKey.begin(); it != inPerKey.end(); ++it) {
        bool ok = false;
        const int led = it.key().toInt(&ok);
        const QColor color(it.value().toString());
        if(ok && led >= 0 && led < F75::matrixLength && color.isValid())
            perKey.insert(QString::number(led), color.name());
    }
    out[QStringLiteral("perKey")] = perKey;

    QVariantMap keymap;
    const QVariantMap inKeymap = in.value(QStringLiteral("keymap")).toMap();
    for(auto layerIt = inKeymap.begin(); layerIt != inKeymap.end(); ++layerIt) {
        bool ok = false;
        const int layer = layerIt.key().toInt(&ok);
        if(!ok || layer < 0 || layer >= F75::layerCount)
            continue;
        QVariantMap entries;
        const QVariantMap inEntries = layerIt.value().toMap();
        for(auto it = inEntries.begin(); it != inEntries.end(); ++it) {
            const int led = it.key().toInt(&ok);
            const qulonglong code = it.value().toULongLong();
            if(ok && led >= 0 && led < F75::matrixLength && code <= 0xffffffffULL)
                entries.insert(QString::number(led), static_cast<quint32>(code));
        }
        keymap.insert(QString::number(layer), entries);
    }
    out[QStringLiteral("keymap")] = keymap;

    QVariantList macros;
    for(const QVariant& m : in.value(QStringLiteral("macros")).toList()) {
        const QVariantMap macro = m.toMap();
        QVariantList events;
        for(const QVariant& e : macro.value(QStringLiteral("events")).toList()) {
            const QVariantMap ev = e.toMap();
            events.append(QVariantMap{
                {QStringLiteral("kind"), clampInt(ev.value(QStringLiteral("kind")), 0, 5, 0)},
                {QStringLiteral("code"), clampInt(ev.value(QStringLiteral("code")), 0, 255, 0)},
                {QStringLiteral("release"), ev.value(QStringLiteral("release")).toBool()},
                {QStringLiteral("delayMs"), clampInt(ev.value(QStringLiteral("delayMs")), 0, 0xfffff, 0)},
            });
            if(events.size() >= 512)
                break;
        }
        macros.append(QVariantMap{
            {QStringLiteral("name"), macro.value(QStringLiteral("name")).toString().left(F75::macroNameMaxChars)},
            {QStringLiteral("events"), events}});
        if(macros.size() >= F75::macroMaxCount)
            break;
    }
    out[QStringLiteral("macros")] = macros;
    out[QStringLiteral("sleepSeconds")] = clampInt(in.value(QStringLiteral("sleepSeconds")), 0, 1200, 300);
    return out;
}

bool ProfileStore::load() {
    QFile file(m_path);
    if(!file.exists())
        return true;
    if(file.size() > maxBytes) {
        m_error = QStringLiteral("Profile file is larger than 4 MiB; not loading it.");
        return false;
    }
    if(!file.open(QIODevice::ReadOnly)) {
        m_error = file.errorString();
        return false;
    }
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if(parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        m_error = QStringLiteral("Profile file is not valid JSON (%1). Existing file left untouched.")
                      .arg(parseError.errorString());
        return false;
    }
    const QJsonObject root = doc.object();
    m_profiles.clear();
    for(const QJsonValue& v : root.value(QStringLiteral("profiles")).toArray())
        m_profiles.append(sanitize(v.toObject().toVariantMap()));
    m_active = std::clamp(root.value(QStringLiteral("activeIndex")).toInt(), 0,
                          std::max(0, static_cast<int>(m_profiles.size()) - 1));
    return true;
}

bool ProfileStore::save() {
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QSaveFile file(m_path);
    if(!file.open(QIODevice::WriteOnly)) {
        m_error = file.errorString();
        emit changed();
        return false;
    }
    const QJsonObject root{{QStringLiteral("schema"), schemaVersion},
                           {QStringLiteral("activeIndex"), m_active},
                           {QStringLiteral("profiles"), QJsonArray::fromVariantList(m_profiles)}};
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if(!file.commit()) {
        m_error = file.errorString();
        emit changed();
        return false;
    }
    m_error.clear();
    emit changed();
    return true;
}

void ProfileStore::setActiveIndex(int index) {
    if(index < 0 || index >= m_profiles.size() || index == m_active)
        return;
    m_active = index;
    save();
}

QVariantMap ProfileStore::profile(int index) const {
    return index >= 0 && index < m_profiles.size() ? m_profiles[index].toMap()
                                                   : defaultProfile(QStringLiteral("Profile"));
}

int ProfileStore::create(const QString& name) {
    m_profiles.append(
        defaultProfile(name.trimmed().isEmpty() ? QStringLiteral("Profile %1").arg(m_profiles.size() + 1) : name));
    save();
    return static_cast<int>(m_profiles.size()) - 1;
}

void ProfileStore::update(int index, const QVariantMap& profile) {
    if(index < 0 || index >= m_profiles.size())
        return;
    m_profiles[index] = sanitize(profile);
    save();
}

void ProfileStore::rename(int index, const QString& name) {
    if(index < 0 || index >= m_profiles.size() || name.trimmed().isEmpty())
        return;
    QVariantMap p = m_profiles[index].toMap();
    p[QStringLiteral("name")] = name.trimmed().left(48);
    m_profiles[index] = p;
    save();
}

void ProfileStore::duplicate(int index) {
    if(index < 0 || index >= m_profiles.size())
        return;
    QVariantMap p = m_profiles[index].toMap();
    p[QStringLiteral("name")] = p[QStringLiteral("name")].toString() + QStringLiteral(" copy");
    m_profiles.append(p);
    save();
}

void ProfileStore::remove(int index) {
    if(index < 0 || index >= m_profiles.size() || m_profiles.size() == 1)
        return;
    m_profiles.removeAt(index);
    if(m_active >= m_profiles.size())
        m_active = static_cast<int>(m_profiles.size()) - 1;
    save();
}

bool ProfileStore::exportTo(int index, const QUrl& fileUrl) {
    if(index < 0 || index >= m_profiles.size())
        return false;
    QSaveFile file(fileUrl.toLocalFile());
    if(!file.open(QIODevice::WriteOnly)) {
        m_error = file.errorString();
        emit changed();
        return false;
    }
    file.write(
        QJsonDocument(QJsonObject{{QStringLiteral("schema"), schemaVersion},
                                  {QStringLiteral("profile"), QJsonObject::fromVariantMap(m_profiles[index].toMap())}})
            .toJson(QJsonDocument::Indented));
    return file.commit();
}

bool ProfileStore::importFrom(const QUrl& fileUrl) {
    QFile file(fileUrl.toLocalFile());
    if(file.size() > maxBytes || !file.open(QIODevice::ReadOnly)) {
        m_error = QStringLiteral("Cannot read %1").arg(fileUrl.toLocalFile());
        emit changed();
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if(!doc.isObject() || !doc.object().contains(QStringLiteral("profile"))) {
        m_error = QStringLiteral("Not a profile export.");
        emit changed();
        return false;
    }
    m_profiles.append(sanitize(doc.object().value(QStringLiteral("profile")).toObject().toVariantMap()));
    return save();
}
