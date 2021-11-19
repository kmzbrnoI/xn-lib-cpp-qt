#include "settings.h"

Settings::Settings() { this->loadDefaults(); }

void Settings::loadDefaults() {
	for (const auto &gm : DEFAULTS)
		for (const std::pair<const QString, QVariant> &k : gm.second)
			if (data[gm.first].find(k.first) == data[gm.first].end())
				data[gm.first][k.first] = k.second;
}

void Settings::load(const QString &filename, bool loadNonDefaults) {
	QSettings s(filename, QSettings::IniFormat);
#if QT_VERSION < 0x060000
	s.setIniCodec("UTF-8");
#endif
	data.clear();

	for (const auto &g : s.childGroups()) {
		if ((!loadNonDefaults) && (DEFAULTS.find(g) == DEFAULTS.end()))
			continue;

		s.beginGroup(g);
		for (const auto &k : s.childKeys()) {
			if ((!loadNonDefaults) && (DEFAULTS.at(g).find(k) == DEFAULTS.at(g).end()))
				continue;
			data[g][k] = s.value(k, "");
		}
		s.endGroup();
	}

	this->loadDefaults();
}

void Settings::save(const QString &filename) {
	QSettings s(filename, QSettings::IniFormat);
#if QT_VERSION < 0x060000
	s.setIniCodec("UTF-8");
#endif

	for (const auto &gm : data) {
		s.beginGroup(gm.first);
		for (const std::pair<const QString, QVariant> &k : gm.second)
			s.setValue(k.first, k.second);
		s.endGroup();
	}
}

std::map<QString, QVariant> &Settings::at(const QString &g) { return data[g]; }
std::map<QString, QVariant> &Settings::operator[](const QString &g) { return at(g); }
