#include "launchersettings.hpp"

#include <QTextStream>
#include <QString>
#include <QRegExp>
#include <QMultiMap>

#include <QDebug>

#include <boost/filesystem/operations.hpp>

#include <components/files/configurationmanager.hpp>

const char Config::LauncherSettings::sCurrentContentListKey[] = "Profiles/currentprofile";
const char Config::LauncherSettings::sLauncherConfigFileName[] = "launcher.cfg";
const char Config::LauncherSettings::sContentListsSectionPrefix[] = "Profiles/";
const char Config::LauncherSettings::sDirectoryListSuffix[] = "/data";
const char Config::LauncherSettings::sArchiveListSuffix[] = "/fallback-archive";
const char Config::LauncherSettings::sContentListSuffix[] = "/content";

QStringList Config::LauncherSettings::subKeys(const QString &key)
{
    QMultiMap<QString, QString> settings = SettingsBase::getSettings();
    QStringList keys = settings.uniqueKeys();

    QRegExp keyRe("(.+)/");

    QStringList result;

    for (const QString &currentKey : keys)
    {

        if (keyRe.indexIn(currentKey) != -1)
        {
            QString prefixedKey = keyRe.cap(1);

            if(prefixedKey.startsWith(key))
            {
                QString subKey = prefixedKey.remove(key);
                if (!subKey.isEmpty())
                    result.append(subKey);
            }
        }
    }

    result.removeDuplicates();
    return result;
}


bool Config::LauncherSettings::writeFile(QTextStream &stream)
{
    QString sectionPrefix;
    QRegExp sectionRe("([^/]+)/(.+)$");
    QMultiMap<QString, QString> settings = SettingsBase::getSettings();

    QMapIterator<QString, QString> i(settings);
    i.toBack();

    while (i.hasPrevious()) {
        i.previous();

        QString prefix;
        QString key;

        if (sectionRe.exactMatch(i.key())) {
             prefix = sectionRe.cap(1);
             key = sectionRe.cap(2);
        }

        // Get rid of legacy settings
        if (key.contains(QChar('\\')))
            continue;

        if (key == QLatin1String("CurrentProfile"))
            continue;

        if (sectionPrefix != prefix) {
            sectionPrefix = prefix;
            stream << "\n[" << prefix << "]\n";
        }

        stream << key << "=" << i.value() << "\n";
    }

    return true;

}

QStringList Config::LauncherSettings::getContentLists()
{
    return subKeys(QString(sContentListsSectionPrefix));
}

QString Config::LauncherSettings::makeDirectoryListKey(const QString& contentListName)
{
    return QString(sContentListsSectionPrefix) + contentListName + QString(sDirectoryListSuffix);
}

QString Config::LauncherSettings::makeArchiveListKey(const QString& contentListName)
{
    return QString(sContentListsSectionPrefix) + contentListName + QString(sArchiveListSuffix);
}

QString Config::LauncherSettings::makeContentListKey(const QString& contentListName)
{
    return QString(sContentListsSectionPrefix) + contentListName + QString(sContentListSuffix);
}

void Config::LauncherSettings::setContentList(const GameSettings& gameSettings)
{
    // obtain content list from game settings (if present)
    QStringList dirs(gameSettings.getDataDirs());
    const QStringList archives(gameSettings.getArchiveList());
    const QStringList files(gameSettings.getContentList());

    // if openmw.cfg has no content, exit so we don't create an empty content list.
    if (dirs.isEmpty() || files.isEmpty())
    {
        return;
    }

    // global and local data directories are not part of any profile
    const auto globalDataDir = QString(gameSettings.getGlobalDataDir().c_str());
    const auto dataLocal = gameSettings.getDataLocal();
    dirs.removeAll(globalDataDir);
    dirs.removeAll(dataLocal);

    // if any existing profile in launcher matches the content list, make that profile the default
    for (const QString &listName : getContentLists())
    {
        if (isEqual(files, getContentListFiles(listName)) &&
            isEqual(archives, getArchiveList(listName)) &&
            isEqual(dirs, getDataDirectoryList(listName)))
        {
            setCurrentContentListName(listName);
            return;
        }
    }

    // otherwise, add content list
    QString newContentListName(makeNewContentListName());
    setCurrentContentListName(newContentListName);
    setContentList(newContentListName, dirs, archives, files);
}

void Config::LauncherSettings::removeContentList(const QString &contentListName)
{
    remove(makeDirectoryListKey(contentListName));
    remove(makeArchiveListKey(contentListName));
    remove(makeContentListKey(contentListName));
}

void Config::LauncherSettings::setCurrentContentListName(const QString &contentListName)
{
    remove(QString(sCurrentContentListKey));
    setValue(QString(sCurrentContentListKey), contentListName);
}

void Config::LauncherSettings::setContentList(const QString& contentListName, const QStringList& dirNames, const QStringList& archiveNames, const QStringList& fileNames)
{
    auto const assign = [this](const QString key, const QStringList& list)
    {
        for (auto const& item : list)
            setMultiValue(key, item);
    };

    removeContentList(contentListName);
    assign(makeDirectoryListKey(contentListName), dirNames);
    assign(makeArchiveListKey(contentListName), archiveNames);
    assign(makeContentListKey(contentListName), fileNames);
}

QString Config::LauncherSettings::getCurrentContentListName() const
{
    return value(QString(sCurrentContentListKey));
}

QStringList Config::LauncherSettings::getDataDirectoryList(const QString& contentListName) const
{
    // QMap returns multiple rows in LIFO order, so need to reverse
    return reverse(getSettings().values(makeDirectoryListKey(contentListName)));
}

QStringList Config::LauncherSettings::getArchiveList(const QString& contentListName) const
{
    // QMap returns multiple rows in LIFO order, so need to reverse
    return reverse(getSettings().values(makeArchiveListKey(contentListName)));
}
QStringList Config::LauncherSettings::getContentListFiles(const QString& contentListName) const
{
    // QMap returns multiple rows in LIFO order, so need to reverse
    return reverse(getSettings().values(makeContentListKey(contentListName)));
}

QStringList Config::LauncherSettings::reverse(const QStringList& toReverse)
{
    QStringList result;
    result.reserve(toReverse.size());
    std::reverse_copy(toReverse.begin(), toReverse.end(), std::back_inserter(result));
    return result;
}

bool Config::LauncherSettings::isEqual(const QStringList& list1, const QStringList& list2)
{
    if (list1.count() != list2.count())
    {
        return false;
    }

    for (int i = 0; i < list1.count(); ++i)
    {
        if (list1.at(i) != list2.at(i))
        {
            return false;
        }
    }

    // if get here, lists are same
    return true;
}

QString Config::LauncherSettings::makeNewContentListName()
{
    // basically, use date and time as the name  e.g. YYYY-MM-DDThh:mm:ss
    time_t rawtime;
    struct tm * timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    int base = 10;
    QChar zeroPad('0');
    return QString("%1-%2-%3T%4:%5:%6")
        .arg(timeinfo->tm_year + 1900, 4).arg(timeinfo->tm_mon + 1, 2, base, zeroPad).arg(timeinfo->tm_mday, 2, base, zeroPad)
        .arg(timeinfo->tm_hour, 2, base, zeroPad).arg(timeinfo->tm_min, 2, base, zeroPad).arg(timeinfo->tm_sec, 2, base, zeroPad);
}


