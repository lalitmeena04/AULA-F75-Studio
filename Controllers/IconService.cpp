#include "IconService.h"

#include <QFontDatabase>
#include <QtAwesomeEnumGenerated.h>
#include <array>

namespace {
struct IconEntry {
        const char* name;
        char16_t fluent;  // Segoe Fluent Icons / Segoe MDL2 Assets codepoint; 0 = none
        char16_t awesome; // Font Awesome 6 codepoint
        bool awesomeBrands;
};

constexpr std::array icons{
    IconEntry{"home", 0xE80F, fa::fa_house, false},
    IconEntry{"keyboard", 0xE765, fa::fa_keyboard, false},
    IconEntry{"lighting", 0xE82F, fa::fa_lightbulb, false},
    IconEntry{"keys", 0xE92E, fa::fa_arrow_right_arrow_left, false},
    IconEntry{"macro", 0xE943, fa::fa_code, false},
    IconEntry{"power", 0xE83F, fa::fa_battery_three_quarters, false},
    IconEntry{"sleep", 0xE708, fa::fa_moon, false},
    IconEntry{"profiles", 0xE8A5, fa::fa_file_lines, false},
    IconEntry{"diagnostics", 0xE9D9, fa::fa_stethoscope, false},
    IconEntry{"settings", 0xE713, fa::fa_gear, false},
    IconEntry{"receiver", 0xECF0, fa::fa_usb, true},
    IconEntry{"usb", 0xECF0, fa::fa_usb, true},
    IconEntry{"wireless", 0xE701, fa::fa_wifi, false},
    IconEntry{"connect", 0xE703, fa::fa_plug, false},
    IconEntry{"refresh", 0xE72C, fa::fa_rotate, false},
    IconEntry{"apply", 0xE73E, fa::fa_check, false},
    IconEntry{"revert", 0xE7A7, fa::fa_rotate_left, false},
    IconEntry{"reset", 0xE777, fa::fa_arrow_rotate_left, false},
    IconEntry{"warning", 0xE7BA, fa::fa_triangle_exclamation, false},
    IconEntry{"error", 0xEA39, fa::fa_circle_xmark, false},
    IconEntry{"ok", 0xE930, fa::fa_circle_check, false},
    IconEntry{"info", 0xE946, fa::fa_circle_info, false},
    IconEntry{"brightness", 0xE706, fa::fa_sun, false},
    IconEntry{"speed", 0xEC4A, fa::fa_gauge_high, false},
    IconEntry{"color", 0xE790, fa::fa_palette, false},
    IconEntry{"brush", 0xE771, fa::fa_paintbrush, false},
    IconEntry{"fill", 0xE8B3, fa::fa_fill_drip, false},
    IconEntry{"eraser", 0xE75C, fa::fa_eraser, false},
    IconEntry{"add", 0xE710, fa::fa_plus, false},
    IconEntry{"edit", 0xE70F, fa::fa_pen, false},
    IconEntry{"delete", 0xE74D, fa::fa_trash, false},
    IconEntry{"save", 0xE74E, fa::fa_floppy_disk, false},
    IconEntry{"copy", 0xE8C8, fa::fa_copy, false},
    IconEntry{"import", 0xE8B5, fa::fa_file_import, false},
    IconEntry{"export", 0xEDE1, fa::fa_file_export, false},
    IconEntry{"close", 0xE711, fa::fa_xmark, false},
    IconEntry{"chevron-right", 0xE76C, fa::fa_chevron_right, false},
    IconEntry{"chevron-down", 0xE70D, fa::fa_chevron_down, false},
    IconEntry{"play", 0xE768, fa::fa_play, false},
    IconEntry{"stop", 0xE71A, fa::fa_stop, false},
    IconEntry{"record", 0xE7C8, fa::fa_circle_dot, false},
    IconEntry{"music", 0xEC4F, fa::fa_music, false},
    IconEntry{"audio", 0xE767, fa::fa_volume_high, false},
    IconEntry{"heat", 0xE9CA, fa::fa_temperature_half, false},
    IconEntry{"cpu", 0xE950, fa::fa_microchip, false},
    IconEntry{"gpu", 0xF211, fa::fa_memory, false},
    IconEntry{"log", 0xE7C3, fa::fa_scroll, false},
    IconEntry{"upload", 0xE898, fa::fa_upload, false},
    IconEntry{"download", 0xE896, fa::fa_download, false},
    IconEntry{"layers", 0xE81E, fa::fa_layer_group, false},
    IconEntry{"timer", 0xE916, fa::fa_stopwatch, false},
    IconEntry{"mouse", 0xE962, fa::fa_computer_mouse, false},
    IconEntry{"unknown", 0xE9CE, fa::fa_circle_question, false},
    IconEntry{"link", 0xE71B, fa::fa_link, false},
    IconEntry{"star", 0xE735, fa::fa_star, false},
    IconEntry{"grid", 0xF0E2, fa::fa_table_cells, false},
    IconEntry{"game", 0xE7FC, fa::fa_gamepad, false},
    IconEntry{"lock", 0xE72E, fa::fa_lock, false},
    IconEntry{"unlock", 0xE785, fa::fa_lock_open, false},
    IconEntry{"plug", 0xE703, fa::fa_plug_circle_check, false},
};

QString registerFont(const QString& resource) {
    const int id = QFontDatabase::addApplicationFont(resource);
    const QStringList families = id >= 0 ? QFontDatabase::applicationFontFamilies(id) : QStringList{};
    return families.isEmpty() ? QString() : families.first();
}
} // namespace

// The qrc lives in a static library; without this reference the linker drops
// its registration object and the Font Awesome resources never appear.
static void initialiseAwesomeResources() {
    Q_INIT_RESOURCE(QtAwesomeFree);
}

IconService::IconService(QObject* parent) : QObject(parent) {
    initialiseAwesomeResources();
    for(const QString& candidate : {QStringLiteral("Segoe Fluent Icons"), QStringLiteral("Segoe MDL2 Assets")}) {
        if(QFontDatabase::hasFamily(candidate)) {
            m_fluentFamily = candidate;
            break;
        }
    }
    m_awesomeSolid = registerFont(QStringLiteral(":/fonts/Font Awesome 6 Free-Solid-900.otf"));
    m_awesomeBrands = registerFont(QStringLiteral(":/fonts/Font Awesome 6 Brands-Regular-400.otf"));
}

QString IconService::activeSet() const {
    if(!m_fluentFamily.isEmpty())
        return QStringLiteral("fluent");
    if(!m_awesomeSolid.isEmpty())
        return QStringLiteral("awesome");
    return QStringLiteral("none");
}

QVariantMap IconService::resolve(const QString& name) const {
    const IconEntry* entry = nullptr;
    for(const auto& candidate : icons)
        if(name == QLatin1String(candidate.name))
            entry = &candidate;
    if(!entry) {
        return {{QStringLiteral("family"), m_fluentFamily.isEmpty() ? m_awesomeSolid : m_fluentFamily},
                {QStringLiteral("glyph"),
                 m_fluentFamily.isEmpty() ? QString(QChar(char16_t(fa::fa_circle_question))) : QString(QChar(0xE9CE))},
                {QStringLiteral("set"), QStringLiteral("none")}};
    }
    if(!m_fluentFamily.isEmpty() && entry->fluent != 0)
        return {{QStringLiteral("family"), m_fluentFamily},
                {QStringLiteral("glyph"), QString(QChar(entry->fluent))},
                {QStringLiteral("set"), QStringLiteral("fluent")}};
    const QString family = entry->awesomeBrands ? m_awesomeBrands : m_awesomeSolid;
    if(!family.isEmpty())
        return {{QStringLiteral("family"), family},
                {QStringLiteral("glyph"), QString(QChar(entry->awesome))},
                {QStringLiteral("set"), QStringLiteral("awesome")}};
    return {{QStringLiteral("family"), QString()},
            {QStringLiteral("glyph"), QStringLiteral("?")},
            {QStringLiteral("set"), QStringLiteral("none")}};
}

QStringList IconService::names() const {
    QStringList list;
    for(const auto& entry : icons)
        list.append(QLatin1String(entry.name));
    return list;
}
