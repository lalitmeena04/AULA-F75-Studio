#include <gtest/gtest.h>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>

#include "ProfileStore.h"

namespace {

class ProfileStoreTest : public ::testing::Test {
    protected:
        QString path() const {
            return m_dir.filePath(QStringLiteral("profiles.json"));
        }

        QTemporaryDir m_dir;
};

} // namespace

TEST_F(ProfileStoreTest, StartsWithOneDefaultProfileWhenNoFileExists) {
    ProfileStore store(path());
    ASSERT_EQ(store.profiles().size(), 1);
    EXPECT_EQ(store.profile(0).value(QStringLiteral("name")).toString(), QStringLiteral("Default"));
    EXPECT_EQ(store.activeIndex(), 0);
    EXPECT_TRUE(store.lastError().isEmpty());
    EXPECT_FALSE(QFile::exists(path()));
}

TEST_F(ProfileStoreTest, CreateAndUpdatePersistAcrossInstances) {
    {
        ProfileStore store(path());
        QSignalSpy changed(&store, &ProfileStore::changed);
        const int index = store.create(QStringLiteral("Gaming"));
        EXPECT_EQ(index, 1);
        QVariantMap profile = store.profile(index);
        profile[QStringLiteral("sleepSeconds")] = 600;
        profile[QStringLiteral("perKey")] = QVariantMap{{QStringLiteral("9"), QStringLiteral("#ff0000")}};
        store.update(index, profile);
        store.setActiveIndex(index);
        EXPECT_GE(changed.count(), 3);
    }
    ProfileStore reloaded(path());
    ASSERT_EQ(reloaded.profiles().size(), 2);
    EXPECT_EQ(reloaded.activeIndex(), 1);
    const QVariantMap gaming = reloaded.profile(1);
    EXPECT_EQ(gaming.value(QStringLiteral("name")).toString(), QStringLiteral("Gaming"));
    EXPECT_EQ(gaming.value(QStringLiteral("sleepSeconds")).toInt(), 600);
    EXPECT_EQ(gaming.value(QStringLiteral("perKey")).toMap().value(QStringLiteral("9")).toString(),
              QStringLiteral("#ff0000"));
}

TEST_F(ProfileStoreTest, SanitizeClampsOutOfRangeValues) {
    ProfileStore store(path());
    QVariantMap profile = ProfileStore::defaultProfile(QStringLiteral("Loud"));
    profile[QStringLiteral("lighting")] = QVariantMap{{QStringLiteral("effectIndex"), 99},
                                                      {QStringLiteral("brightness"), -4},
                                                      {QStringLiteral("speed"), 7},
                                                      {QStringLiteral("color"), 12}};
    profile[QStringLiteral("sleepSeconds")] = 99999;
    profile[QStringLiteral("perKey")] = QVariantMap{{QStringLiteral("999"), QStringLiteral("#00ff00")},
                                                    {QStringLiteral("3"), QStringLiteral("not-a-colour")}};
    store.update(0, profile);
    const QVariantMap stored = store.profile(0);
    const QVariantMap lighting = stored.value(QStringLiteral("lighting")).toMap();
    EXPECT_EQ(lighting.value(QStringLiteral("effectIndex")).toInt(), 19);
    EXPECT_EQ(lighting.value(QStringLiteral("brightness")).toInt(), 0);
    EXPECT_EQ(lighting.value(QStringLiteral("speed")).toInt(), 4);
    EXPECT_EQ(lighting.value(QStringLiteral("color")).toInt(), 7);
    EXPECT_EQ(stored.value(QStringLiteral("sleepSeconds")).toInt(), 1200);
    EXPECT_TRUE(stored.value(QStringLiteral("perKey")).toMap().isEmpty());
}

TEST_F(ProfileStoreTest, LastProfileCannotBeRemoved) {
    ProfileStore store(path());
    store.remove(0);
    EXPECT_EQ(store.profiles().size(), 1);
    store.create(QStringLiteral("Second"));
    store.remove(0);
    ASSERT_EQ(store.profiles().size(), 1);
    EXPECT_EQ(store.profile(0).value(QStringLiteral("name")).toString(), QStringLiteral("Second"));
}

TEST_F(ProfileStoreTest, ExportThenImportAppendsACopy) {
    ProfileStore store(path());
    store.rename(0, QStringLiteral("Travel"));
    const QUrl file = QUrl::fromLocalFile(m_dir.filePath(QStringLiteral("travel.json")));
    ASSERT_TRUE(store.exportTo(0, file));
    ASSERT_TRUE(store.importFrom(file));
    ASSERT_EQ(store.profiles().size(), 2);
    EXPECT_EQ(store.profile(1).value(QStringLiteral("name")).toString(), QStringLiteral("Travel"));
}

TEST_F(ProfileStoreTest, CorruptFileIsReportedAndLeftUntouched) {
    {
        QFile file(path());
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        file.write("{ not json");
    }
    ProfileStore store(path());
    EXPECT_FALSE(store.lastError().isEmpty());
    ASSERT_EQ(store.profiles().size(), 1);
    QFile file(path());
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    EXPECT_EQ(file.readAll(), QByteArray("{ not json"));
}
